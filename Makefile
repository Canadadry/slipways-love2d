.DEFAULT_GOAL := help

# Reruns make under flock: one make of LOCKED_GOALS at a time over tmp/.
LOCKED_GOALS := test e2e test-all end-state e2e-baseline end-state-baseline

ifneq ($(if $(MAKE_LOCK_HELD),,$(filter $(LOCKED_GOALS),$(MAKECMDGOALS))),)
$(MAKECMDGOALS): make-lock ; @:

make-lock:
	@command -v flock > /dev/null || { echo "make: flock missing (macOS: brew install flock)" >&2; exit 1; }
	@mkdir -p tmp
	@flock -n -E 75 tmp/make.lock $(MAKE) --no-print-directory MAKE_LOCK_HELD=1 $(MAKECMDGOALS); \
	status=$$?; \
	[ $$status -ne 75 ] || echo "make: tmp/ is in use by another make of $(LOCKED_GOALS)" >&2; \
	exit $$status

.PHONY: make-lock
else
help: ## lists the targets
	@grep -hE '^[a-z0-9-]+:.*## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*## "}; {printf "%-30s %s\n", $$1, $$2}' | LC_ALL=C sort

.PHONY: help unit replay state test e2e test-all e2e-baseline

NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
MAKEFLAGS += -j$(NPROC)
ifneq ($(filter output-sync,$(.FEATURES)),)
    MAKEFLAGS += --output-sync=target
endif

CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g

TMP_DIR := tmp

OBJ_DIR := $(TMP_DIR)/obj

# `-MMD -MP`: next to each object, the compiler writes the list of headers that
# the translation unit actually opened. The include at the bottom of this file
# reads them back, so no header is ever named by hand in a prerequisite list.
DEPFLAGS := -MMD -MP

# `$(call objs,<sources>)` -> the paths of the matching object files in the
# tree concerned. A target names its sources once; its prerequisites and its
# link line follow from them.
objs = $(patsubst %.c,$(OBJ_DIR)/%.o,$(1))

# `mkdir -p $(@D)` in the recipe rather than a directory as a prerequisite:
# the mtime of a directory changes with every file written into it, which would
# rebuild everything.
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

UNAME_S := $(shell uname -s)

$(TMP_DIR):
	mkdir -p $(TMP_DIR)

# The .json prerequisites are the fixtures that the test reads at run time;
# `$(filter %.o,$^)` keeps them off the link line.
test_fake_love_replay_objs := $(call objs,tests/test_fake_love_replay.c tools/fake_love/replay.c)
$(TMP_DIR)/test_fake_love_replay: $(test_fake_love_replay_objs) $(wildcard tests/e2e/*.json) tests/fixtures/touch_frames.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_pad_objs := $(call objs,tests/test_fake_love_pad.c tools/fake_love/pad.c)
$(TMP_DIR)/test_fake_love_pad: $(test_fake_love_pad_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_canvas_objs := $(call objs,tests/test_fake_love_canvas.c tools/fake_love/canvas.c)
$(TMP_DIR)/test_fake_love_canvas: $(test_fake_love_canvas_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

test_fake_love_image_data_objs := $(call objs,tests/test_fake_love_image_data.c tools/fake_love/image_data.c)
$(TMP_DIR)/test_fake_love_image_data: $(test_fake_love_image_data_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

test_fake_love_image_rules_objs := $(call objs,tests/test_fake_love_image_rules.c tools/fake_love/image_rules.c tools/fake_love/point.c)
$(TMP_DIR)/test_fake_love_image_rules: $(test_fake_love_image_rules_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

test_fake_love_point_objs := $(call objs,tests/test_fake_love_point.c tools/fake_love/point.c)
$(TMP_DIR)/test_fake_love_point: $(test_fake_love_point_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

test_fake_love_frame_capture_objs := $(call objs,tests/test_fake_love_frame_capture.c tools/fake_love/frame_capture.c)
$(TMP_DIR)/test_fake_love_frame_capture: $(test_fake_love_frame_capture_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_transform_objs := $(call objs,tests/test_fake_love_transform.c tools/fake_love/transform.c)
$(TMP_DIR)/test_fake_love_transform: $(test_fake_love_transform_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_disk_objs := $(call objs,tests/test_fake_love_disk.c tools/fake_love/disk.c)
$(TMP_DIR)/test_fake_love_disk: $(test_fake_love_disk_objs) $(wildcard tests/fixtures/fake_love/disk_tree/* tests/fixtures/fake_love/disk_tree/*/*) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_png_file_objs := $(call objs,tests/test_fake_love_png_file.c tools/fake_love/png_file.c)
$(TMP_DIR)/test_fake_love_png_file: $(test_fake_love_png_file_objs) game/sprites.png game/font.png | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

# Recursive (`=`): only a target that builds the fake runs pkg-config.
luajit_flags = $(if $(filter LUAJIT_MISSING,$(1)),$(error pkg-config luajit failed: run tools/love_install.sh),$(1))
LUAJIT_CFLAGS = $(call luajit_flags,$(shell pkg-config --cflags luajit 2>/dev/null || echo LUAJIT_MISSING))
LUAJIT_LIBS = $(call luajit_flags,$(shell pkg-config --libs luajit 2>/dev/null || echo LUAJIT_MISSING))
FAKE_LOVE_LUA_SRCS := tools/fake_love/fake_love.c tools/fake_love/host.c tools/fake_love/joystick.c tools/fake_love/filesystem.c tools/fake_love/graphics.c tools/fake_love/image.c tools/fake_love/function_list.c

$(call objs,$(FAKE_LOVE_LUA_SRCS)): override CFLAGS += $(LUAJIT_CFLAGS)

# What a Lua game runs on, without a command.
FAKE_LOVE_HOST_SRCS := $(filter-out tools/fake_love/fake_love.c,$(FAKE_LOVE_LUA_SRCS)) tools/fake_love/pad.c tools/fake_love/disk.c tools/fake_love/canvas.c tools/fake_love/image_data.c tools/fake_love/image_rules.c tools/fake_love/point.c tools/fake_love/png_file.c tools/fake_love/transform.c tools/fake_love/frame_capture.c

fake_love_objs := $(call objs,tools/fake_love/fake_love.c tools/fake_love/replay.c $(FAKE_LOVE_HOST_SRCS))
$(TMP_DIR)/fake_love: $(fake_love_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(LUAJIT_LIBS) -lm

love_capture_check_objs := $(call objs,tools/fake_love/love_capture_check.c $(FAKE_LOVE_HOST_SRCS))
$(TMP_DIR)/love_capture_check: $(love_capture_check_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(LUAJIT_LIBS) -lm

fake-love: $(TMP_DIR)/fake_love ## builds tmp/fake_love, the headless LÖVE that plays a Lua game on a replay

.PHONY: fake-love

FAKE_LOVE_HARNESS := tools/fake_love/harness/main.lua

FAKE_LOVE_FIXTURES := $(wildcard tests/fixtures/fake_love/*.json tests/fixtures/fake_love/*.cfg tests/fixtures/fake_love/*.lua tests/fixtures/fake_love/*/*.lua tests/fixtures/fake_love/*/*.txt tests/fixtures/fake_love/*/*/*.lua tests/fixtures/fake_love/*/*/*.txt tests/fixtures/fake_love/*/*.png tests/fixtures/fake_love/*/*/*.png)

test_fake_love_objs := $(call objs,tests/test_fake_love.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love: $(test_fake_love_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_frames_objs := $(call objs,tests/test_fake_love_frames.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love_frames: $(test_fake_love_frames_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_failures_objs := $(call objs,tests/test_fake_love_failures.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love_failures: $(test_fake_love_failures_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_joystick_objs := $(call objs,tests/test_fake_love_joystick.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love_joystick: $(test_fake_love_joystick_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_window_objs := $(call objs,tests/test_fake_love_window.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love_window: $(test_fake_love_window_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_files_objs := $(call objs,tests/test_fake_love_files.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love_files: $(test_fake_love_files_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_graphics_objs := $(call objs,tests/test_fake_love_graphics.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love_graphics: $(test_fake_love_graphics_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_unlisted_objs := $(call objs,tests/test_fake_love_unlisted.c tests/fake_love_cli.c)
$(TMP_DIR)/test_fake_love_unlisted: $(test_fake_love_unlisted_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_points_objs := $(call objs,tests/test_fake_love_points.c tests/fake_love_cli.c tests/fake_love_png.c)
$(TMP_DIR)/test_fake_love_points: $(test_fake_love_points_objs) $(TMP_DIR)/fake_love $(TMP_DIR)/love_capture_check $(FAKE_LOVE_HARNESS) $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_rectangle_objs := $(call objs,tests/test_fake_love_rectangle.c tests/fake_love_cli.c tests/fake_love_png.c)
$(TMP_DIR)/test_fake_love_rectangle: $(test_fake_love_rectangle_objs) $(TMP_DIR)/fake_love $(TMP_DIR)/love_capture_check $(FAKE_LOVE_HARNESS) $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_set_color_objs := $(call objs,tests/test_fake_love_set_color.c tests/fake_love_cli.c tests/fake_love_png.c)
$(TMP_DIR)/test_fake_love_set_color: $(test_fake_love_set_color_objs) $(TMP_DIR)/fake_love $(TMP_DIR)/love_capture_check $(FAKE_LOVE_HARNESS) $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_image_draw_objs := $(call objs,tests/test_fake_love_image_draw.c tests/fake_love_cli.c tests/fake_love_png.c)
$(TMP_DIR)/test_fake_love_image_draw: $(test_fake_love_image_draw_objs) $(TMP_DIR)/fake_love $(TMP_DIR)/love_capture_check $(FAKE_LOVE_HARNESS) $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(call objs,tests/test_fake_love_image.c): override CFLAGS += $(LUAJIT_CFLAGS)
test_fake_love_image_objs := $(call objs,tests/test_fake_love_image.c tests/fake_love_cli.c tools/fake_love/image.c tools/fake_love/function_list.c tools/fake_love/image_data.c tools/fake_love/filesystem.c tools/fake_love/disk.c tools/fake_love/png_file.c)
$(TMP_DIR)/test_fake_love_image: $(test_fake_love_image_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(LUAJIT_LIBS) -lm

$(call objs,tests/test_fake_love_function_list.c): override CFLAGS += $(LUAJIT_CFLAGS)
test_fake_love_function_list_objs := $(call objs,tests/test_fake_love_function_list.c tools/fake_love/function_list.c)
$(TMP_DIR)/test_fake_love_function_list: $(test_fake_love_function_list_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(LUAJIT_LIBS) -lm

# The one translation unit compiling the vendored stb_image.h.
$(call objs,tools/fake_love/png_file.c): override CFLAGS += -Wno-unused-function

test_fake_love_capture_objs := $(call objs,tests/test_fake_love_capture.c tests/fake_love_cli.c tests/fake_love_png.c tools/fake_love/png_file.c)
$(TMP_DIR)/test_fake_love_capture: $(test_fake_love_capture_objs) $(TMP_DIR)/love_capture_check $(FAKE_LOVE_HARNESS) $(FAKE_LOVE_FIXTURES) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

test_love_e2e_objs := $(call objs,tests/test_love_e2e.c tests/fake_love_cli.c)
$(TMP_DIR)/test_love_e2e: $(test_love_e2e_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) tools/love_e2e_run.sh tools/love_e2e_report.sh tools/love_e2e_state_report.sh tools/frames_check.sh tools/end_state.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_love_runtime_objs := $(call objs,tests/test_love_runtime.c tests/fake_love_cli.c)
$(TMP_DIR)/test_love_runtime: $(test_love_runtime_objs) $(TMP_DIR)/fake_love $(wildcard game/p8/*.lua game/love/*.lua tests/fixtures/love_runtime*/*.lua tests/fixtures/love_runtime*/*.txt tests/fixtures/love_runtime*/*.bin) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_frames_check: $(call objs,tests/test_frames_check.c) tools/frames_check.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_e2e_limits: $(call objs,tests/test_e2e_limits.c) tools/e2e_limits.sh tests/fixtures/touch_frames.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_end_state: $(call objs,tests/test_end_state.c) tools/end_state.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_lua_guard: $(call objs,tests/test_lua_guard.c) tools/lua_guard.lua | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_lua_modules: $(call objs,tests/test_lua_modules.c) tests/lua/test_modules.lua | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_lua_fixed: $(call objs,tests/test_lua_fixed.c) tests/lua/test_fixed.lua | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_guard_objs := $(call objs,tests/test_fake_love_guard.c)
$(TMP_DIR)/test_fake_love_guard: $(test_fake_love_guard_objs) tools/fake_love_guard.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

UNIT_TESTS := \
    test_frames_check \
    test_e2e_limits \
    test_end_state \
    test_fake_love_guard \
    test_lua_guard \
    test_lua_modules \
    test_lua_fixed \
    test_fake_love_replay \
    test_fake_love_pad \
    test_fake_love_canvas \
    test_fake_love_image_data \
    test_fake_love_image_rules \
    test_fake_love_point \
    test_fake_love_transform \
    test_fake_love_disk \
    test_fake_love_png_file \
    test_fake_love \
    test_fake_love_failures \
    test_fake_love_joystick \
    test_fake_love_window \
    test_fake_love_files \
    test_fake_love_graphics \
    test_fake_love_points \
    test_fake_love_rectangle \
    test_fake_love_set_color \
    test_fake_love_image \
    test_fake_love_image_draw \
    test_fake_love_capture \
    test_fake_love_frame_capture \
    test_fake_love_frames \
    test_fake_love_function_list \
    test_fake_love_unlisted \
    test_love_e2e \
    test_love_runtime

# One run per test, each its own target: that is what `-j` runs in parallel.
UNIT_RUNS := $(addprefix unit-run-,$(UNIT_TESTS))

UNIT_RUNS_SPECIAL := unit-run-test_love_runtime

unit: $(UNIT_RUNS) ## builds and runs the binaries of tests/

$(filter-out $(UNIT_RUNS_SPECIAL),$(UNIT_RUNS)): unit-run-%: $(TMP_DIR)/%
	./$<

# Arguments: the fixture game directory, then the frame count of its scene.txt.
unit-run-test_love_runtime: $(TMP_DIR)/test_love_runtime
	./$< tests/fixtures/love_runtime 16

unit-run-test_love_runtime_slots: $(TMP_DIR)/test_love_runtime
	./$< tests/fixtures/love_runtime_slots 3

.PHONY: $(UNIT_RUNS) unit-run-test_love_runtime_slots

# One target per name of tools/love_capture/, from its .lua and .png alike,
# so that a lone half fails instead of being skipped. The halves are sorted
# here, not in a recipe: a pair's recipe then runs without a shell.
LOVE_CAPTURE_PROGRAMS := $(basename $(notdir $(wildcard tools/love_capture/*.lua)))
LOVE_CAPTURE_PNGS := $(basename $(notdir $(wildcard tools/love_capture/*.png)))
LOVE_CAPTURE_NAMES := $(sort $(LOVE_CAPTURE_PROGRAMS) $(LOVE_CAPTURE_PNGS))
LOVE_CAPTURE_CHECKS := $(addprefix love-capture-check-,$(LOVE_CAPTURE_NAMES))

$(addprefix love-capture-check-,$(filter $(LOVE_CAPTURE_PNGS),$(LOVE_CAPTURE_PROGRAMS))): love-capture-check-%: tools/love_capture/%.lua tools/love_capture/%.png $(TMP_DIR)/love_capture_check $(FAKE_LOVE_HARNESS)
	./$(TMP_DIR)/love_capture_check tools/fake_love/harness tools/love_capture/$*.lua tools/love_capture/$*.png

$(addprefix love-capture-check-,$(filter-out $(LOVE_CAPTURE_PROGRAMS),$(LOVE_CAPTURE_PNGS))): love-capture-check-%:
	@echo "love-capture-check: $*.lua missing for $*.png" >&2; exit 1

$(addprefix love-capture-check-,$(filter-out $(LOVE_CAPTURE_PNGS),$(LOVE_CAPTURE_PROGRAMS))): love-capture-check-%:
	@echo "love-capture-check: $*.png missing: run make love-capture" >&2; exit 1

.PHONY: $(LOVE_CAPTURE_CHECKS)

# Each check that runs the fake is its own target, listed here, so that `-j`
# runs it beside the unit tests instead of after them.
FAKE_LOVE_CHECKS := $(LOVE_CAPTURE_CHECKS) unit-run-test_fake_love unit-run-test_fake_love_failures unit-run-test_fake_love_joystick unit-run-test_fake_love_window unit-run-test_fake_love_files unit-run-test_fake_love_graphics unit-run-test_fake_love_points unit-run-test_fake_love_rectangle unit-run-test_fake_love_set_color unit-run-test_fake_love_image unit-run-test_fake_love_image_draw unit-run-test_fake_love_capture unit-run-test_fake_love_frame_capture unit-run-test_fake_love_frames unit-run-test_fake_love_function_list unit-run-test_fake_love_unlisted unit-run-test_love_e2e unit-run-test_love_runtime unit-run-test_love_runtime_slots

LUA_GAME_TESTS := $(wildcard tests/lua/game/test_*.lua)
LUA_GAME_TEST_RUNS := $(patsubst tests/lua/game/%.lua,lua-run-%,$(LUA_GAME_TESTS))

test: unit $(FAKE_LOVE_CHECKS) lua-test-fixed lua-test-runtime lua-guard lua-test-modules $(LUA_GAME_TEST_RUNS) lua-test-capture-read fake-love-guard ## fast loop: unit tests and static guards (~1 s)

test-all: test e2e ## test + e2e, the full suite

# The e2e: <name>.json (a recorded input file, as replay reads it), <name>.txt
# (what the replay does, the frames of the key actions) and <name>.json.sha256.
# All of them start from the title screen.
E2E_DIR := tests/e2e
E2E_EVENTS := $(sort $(wildcard $(E2E_DIR)/*.json))
# One `<events> reason` line per e2e allowed up to 600 frames instead of 180.
E2E_EXCEPTIONS := tools/e2e_long_exceptions.txt

ifeq ($(UNAME_S),Darwin)
    SHA256_CMD := shasum -a 256
else
    SHA256_CMD := sha256sum
endif

# An interrupted capture would leave a partial file that make would believe up
# to date, and the next hash would be taken over it.
.DELETE_ON_ERROR:

# The sha256 of the capture of each e2e, versioned next to it as
# <events>.sha256. The capture is the raw concatenation of the frames: its hash
# also covers their count.
frames_reference = $(1).sha256

# A `%` in LOVE_E2E_GAME becomes the replay's file name: one game directory
# per replay. Forced: make does not see a Lua edit.
LOVE_E2E_GAME ?= game
LOVE_E2E_DIR := $(TMP_DIR)/love-e2e
LOVE_E2E_HASHES := $(foreach e,$(E2E_EVENTS),$(LOVE_E2E_DIR)/$(e).sha256)
# Largest file first, which follows the frame count: `-j` starts the longest
# replays first.
LOVE_E2E_LONGEST_FIRST := $(foreach e,$(if $(E2E_EVENTS),$(shell ls -S $(E2E_EVENTS))),$(LOVE_E2E_DIR)/$(e).sha256)

$(LOVE_E2E_HASHES): $(LOVE_E2E_DIR)/%.sha256: % $(TMP_DIR)/fake_love tools/love_e2e_run.sh FORCE
	@mkdir -p $(@D)
	@sh tools/love_e2e_run.sh "$(SHA256_CMD)" ./$(TMP_DIR)/fake_love $(subst %,$(notdir $*),$(LOVE_E2E_GAME)) $* $(@:.sha256=.bin) $@ $(@:.sha256=.error) $(@:.sha256=.state)

e2e: $(LOVE_E2E_LONGEST_FIRST) tools/e2e_limits.sh $(E2E_EXCEPTIONS) tools/love_e2e_report.sh tools/frames_check.sh ## runs the Lua game in tmp/fake_love on tests/e2e/, checks each capture against its .json.sha256 (~7 s)
	@status=0; \
	sh tools/e2e_limits.sh $(E2E_EXCEPTIONS) $(E2E_EVENTS) || status=1; \
	sh tools/love_e2e_report.sh $(foreach e,$(E2E_EVENTS),$(e) $(call frames_reference,$(e)) $(LOVE_E2E_DIR)/$(e).sha256 $(LOVE_E2E_DIR)/$(e).error) || status=1; \
	exit $$status

e2e-baseline: $(LOVE_E2E_LONGEST_FIRST) ## sets or resets the .json.sha256 of tests/e2e/ from the Lua game in tmp/fake_love
	@status=0; \
	$(foreach e,$(E2E_EVENTS),if [ -f $(LOVE_E2E_DIR)/$(e).error ]; then echo "e2e-baseline: FAILED $(e), no reference written" >&2; status=1; else cp $(LOVE_E2E_DIR)/$(e).sha256 $(call frames_reference,$(e)) && echo "e2e: reference set in $(call frames_reference,$(e))" || status=1; fi;) \
	exit $$status

FORCE:

LOVE_ARCHIVE := $(TMP_DIR)/slipways.love

$(LOVE_ARCHIVE): $(shell find game -type f) | $(TMP_DIR)
	@rm -f $@
	@cd game && zip -q -9 -X -r $(CURDIR)/$@ .
	@echo "love: $@"

love: $(LOVE_ARCHIVE) ## zips game/ into tmp/slipways.love, playable with LÖVE 11.5 and a gamepad

.PHONY: love FORCE

# Fails before any compilation rather than replay a default file.
ifneq ($(filter replay state,$(MAKECMDGOALS)),)
    ifeq ($(strip $(EVENTS)),)
        $(error EVENTS missing: make $(firstword $(filter replay state,$(MAKECMDGOALS))) EVENTS=<events.json>)
    endif
endif

# $(1): the run's path without extension, $(2): the target named on failure.
# The outputs of a stopped fake are removed, so no stale file is read.
define love_run_events
	@mkdir -p $(dir $(1))
	@sh tools/love_e2e_run.sh "$(SHA256_CMD)" ./$(TMP_DIR)/fake_love $(subst %,$(notdir $(EVENTS)),$(LOVE_E2E_GAME)) $(EVENTS) $(1).bin $(1).sha256 $(1).error $(1).state
	@if [ -f $(1).error ]; then \
		echo "$(2): FAILED $(EVENTS) -- the fake stopped before the last frame." >&2; \
		sed 's/^/$(2):   /' $(1).error >&2; \
		exit 1; \
	fi
endef

REPLAY_RUN := $(TMP_DIR)/love-replay/$(notdir $(EVENTS))

replay: $(TMP_DIR)/fake_love tools/love_e2e_run.sh tools/capture_read.lua ## EVENTS=<events>: runs the Lua game in tmp/fake_love, last frame in tmp/replay_dump.txt, capture in tmp/love-replay/
	@rm -f $(TMP_DIR)/replay_dump.txt
	$(call love_run_events,$(REPLAY_RUN),replay)
	@frames=$$(( $$(wc -c < $(REPLAY_RUN).bin) / 9600 )); \
	{ echo "160 120"; luajit tools/capture_read.lua frame $(REPLAY_RUN).bin $$((frames - 1)); } > $(TMP_DIR)/replay_dump.txt.part || { rm -f $(TMP_DIR)/replay_dump.txt.part; exit 1; }; \
	mv $(TMP_DIR)/replay_dump.txt.part $(TMP_DIR)/replay_dump.txt; \
	echo "replay: $$frames frame(s) of $(EVENTS); last frame in $(TMP_DIR)/replay_dump.txt, capture in $(REPLAY_RUN).bin"

STATE_RUN := $(TMP_DIR)/love-state/$(notdir $(EVENTS))

state: $(TMP_DIR)/fake_love tools/love_e2e_run.sh ## EVENTS=<events>: runs the Lua game in tmp/fake_love, its last-frame state dump in tmp/state.txt
	@rm -f $(TMP_DIR)/state.txt
	$(call love_run_events,$(STATE_RUN),state)
	@cp $(STATE_RUN).state $(TMP_DIR)/state.txt
	@echo "state: last-frame state of $(EVENTS) in $(TMP_DIR)/state.txt"

END_STATE_DIR := $(TMP_DIR)/end-state
END_STATE_RUN := $(END_STATE_DIR)/$(notdir $(EVENTS))

ifneq ($(strip $(EVENTS)),)
end-state: $(TMP_DIR)/fake_love tools/love_e2e_run.sh tools/end_state.sh ## [EVENTS=<events> [REFERENCE=<file>]]: compares the Lua game's end states in tmp/fake_love with the committed .end-state files
	$(call love_run_events,$(END_STATE_RUN),end-state)
	@sh tools/end_state.sh check $(EVENTS) $(END_STATE_RUN).state $(or $(REFERENCE),$(EVENTS).end-state)
else
end-state: $(LOVE_E2E_LONGEST_FIRST) tools/love_e2e_state_report.sh tools/end_state.sh
	@sh tools/love_e2e_state_report.sh $(foreach e,$(E2E_EVENTS),$(e) $(LOVE_E2E_DIR)/$(e).state $(e).end-state $(LOVE_E2E_DIR)/$(e).error)
endif

end-state-baseline: $(LOVE_E2E_LONGEST_FIRST) tools/end_state.sh ## writes each missing .end-state of tests/e2e/ from the Lua game, never overwrites one
	@status=0; triples=; \
	$(foreach e,$(E2E_EVENTS),if [ -f $(LOVE_E2E_DIR)/$(e).error ]; then echo "end-state-baseline: FAILED $(e), no reference written" >&2; status=1; else triples="$$triples $(e) $(LOVE_E2E_DIR)/$(e).state $(e).end-state"; fi;) \
	[ -z "$$triples" ] || sh tools/end_state.sh baseline $$triples || status=1; \
	exit $$status

.PHONY: end-state end-state-baseline

# `<binary> <objects>` per binary of the fake side: each name below has its
# `<name>_objs` beside its link rule. tmp/luajit_version has no object: the
# include scan of tools/love_capture/ covers it.
FAKE_SIDE_BINARIES := fake_love love_capture_check test_fake_love_replay test_fake_love_pad test_fake_love_canvas test_fake_love_image_data test_fake_love_image_rules test_fake_love_point test_fake_love_transform test_fake_love_disk test_fake_love_png_file test_fake_love test_fake_love_failures test_fake_love_joystick test_fake_love_window test_fake_love_files test_fake_love_graphics test_fake_love_points test_fake_love_rectangle test_fake_love_set_color test_fake_love_image test_fake_love_image_draw test_fake_love_capture test_fake_love_frame_capture test_fake_love_frames test_fake_love_function_list test_fake_love_unlisted test_fake_love_guard test_love_e2e test_love_runtime
FAKE_SIDE_LINKS := $(foreach binary,$(FAKE_SIDE_BINARIES),$(TMP_DIR)/$(binary) $($(binary)_objs))
FAKE_SIDE_SRCS := tools/fake_love tools/love_capture $(wildcard tests/test_fake_love*.c tests/fake_love_*.[ch]) tests/test_love_e2e.c tests/test_love_runtime.c

fake-love-guard: tools/fake_love_guard.sh $(filter %.o,$(FAKE_SIDE_LINKS)) ## fails when the fake side includes a header or links an object from src/
	@sh tools/fake_love_guard.sh $(OBJ_DIR) $(FAKE_SIDE_LINKS) -- $(FAKE_SIDE_SRCS)

.PHONY: fake-love-guard

lua-test-fixed: tests/lua/test_fixed.lua $(wildcard game/p8/*.lua) tests/lua/fixed_reference.txt ## compares game/p8/fixed.lua with the frozen fixed-point reference on edge and sampled values
	luajit tests/lua/test_fixed.lua tests/lua/fixed_reference.txt

lua-test-runtime: tests/lua/test_runtime.lua $(wildcard game/p8/*.lua) ## checks the Lua-only behaviour of game/p8/ modules
	luajit tests/lua/test_runtime.lua

lua-test-capture-read: tests/lua/test_capture_read.lua tools/capture_read.lua ## checks tools/capture_read.lua prints and compares fake-LÖVE capture frames correctly
	luajit tests/lua/test_capture_read.lua

.PHONY: lua-test-fixed lua-test-runtime lua-test-capture-read

# wildcard, not find: the guard stays a single process.
LUA_GUARD_FILES := $(wildcard game/*.lua game/*/*.lua game/*/*/*.lua)

lua-guard: tools/lua_guard.lua ## fails when a game/ Lua file does not compile, uses an undeclared global or an unlisted LÖVE name
	@luajit tools/lua_guard.lua game $(LUA_GUARD_FILES)

.PHONY: lua-guard

$(LUA_GAME_TEST_RUNS): lua-run-%: tests/lua/game/%.lua tests/lua/game/check.lua $(LUA_GUARD_FILES)
	LUA_PATH='game/?.lua;tests/lua/game/?.lua' LUA_CPATH= luajit $<

lua-test-game: $(LUA_GAME_TEST_RUNS) ## runs the Lua regression tests of game/ modules under plain LuaJIT, one process per file

.PHONY: lua-test-game $(LUA_GAME_TEST_RUNS)

lua-test-modules: tests/lua/test_modules.lua $(LUA_GUARD_FILES) ## requires every game/ module under plain LuaJIT and reports each that fails to load
	@luajit tests/lua/test_modules.lua game $(LUA_GUARD_FILES)

.PHONY: lua-test-modules

LUA_NAMES_FILES := $(LUA_GUARD_FILES) $(wildcard tests/lua/*.lua tests/lua/*/*.lua tests/fixtures/love_runtime*/*.lua)

lua-names: tools/lua_names.lua tests/lua/test_lua_names.lua ## [NAMES_MODULES=...]: fails when a game/ export repeats its module prefix or a Lua file reads a missing module field
	@luajit tests/lua/test_lua_names.lua
	@luajit tools/lua_names.lua --modules '$(NAMES_MODULES)' game $(LUA_NAMES_FILES)

.PHONY: lua-names

$(TMP_DIR)/luajit_version: tools/love_capture/luajit_version.c | $(TMP_DIR)
	$(CC) $(CFLAGS) $$(pkg-config --cflags luajit) -o $@ $< $$(pkg-config --libs luajit)

LOVE_CAPTURE_DIR := $(CURDIR)/tools/love_capture

# One xvfb-run wraps the whole loop: it starts Xvfb and stops it on exit.
love-capture: ## runs the tools/love_capture/ programs in real LÖVE 11.5 under Xvfb, rewrites their PNGs
	@command -v love >/dev/null && command -v xvfb-run >/dev/null || \
		{ echo "love-capture: love or xvfb-run missing, run tools/love_install.sh" >&2; exit 1; }
	xvfb-run -a -s '-screen 0 640x480x24' sh -c ' \
		for program in $(LOVE_CAPTURE_DIR)/*.lua; do \
			LIBGL_ALWAYS_SOFTWARE=1 timeout 60 love $(LOVE_CAPTURE_DIR)/harness "$$program" "$${program%.lua}.png" \
				|| { echo "love-capture: $$program failed" >&2; exit 1; }; \
		done'

.PHONY: love-capture

love-check: $(TMP_DIR)/luajit_version ## checks that the installed LuaJIT is 2.1; make test compares the captures
	./$(TMP_DIR)/luajit_version

.PHONY: love-check

# The dependency fragments written by the compiler, one per object. The
# wildcard tolerates their absence: on a fresh clone, everything is compiled.
-include $(wildcard $(OBJ_DIR)/tests/*.d $(OBJ_DIR)/tools/*.d $(OBJ_DIR)/tools/*/*.d)

endif
