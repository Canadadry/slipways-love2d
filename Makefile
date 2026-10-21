.DEFAULT_GOAL := help

# Reruns make under flock: one make of LOCKED_GOALS at a time over tmp/.
LOCKED_GOALS := test e2e test-all love-e2e lua-sweep-fixed

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

.PHONY: help unit replay state test e2e test-all e2e-baseline e2e-ref

NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
MAKEFLAGS += -j$(NPROC)
ifneq ($(filter output-sync,$(.FEATURES)),)
    MAKEFLAGS += --output-sync=target
endif

CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g
override CFLAGS += -Isrc

TMP_DIR := tmp

# One object tree per set of flags. An object compiled without -fsanitize is
# not instrumented: sharing it with a target that is would leave the binary
# blind on that code, silently. Do not merge the five.
OBJ_DIR := $(TMP_DIR)/obj
OBJ_ASAN_DIR := $(TMP_DIR)/obj-asan
OBJ_UBSAN_DIR := $(TMP_DIR)/obj-ubsan
OBJ_SANITIZE_DIR := $(TMP_DIR)/obj-sanitize
OBJ_COVERAGE_DIR := $(TMP_DIR)/obj-coverage

# `-MMD -MP`: next to each object, the compiler writes the list of headers that
# the translation unit actually opened. The include at the bottom of this file
# reads them back, so no header is ever named by hand in a prerequisite list.
DEPFLAGS := -MMD -MP
ASAN_CFLAGS := -fsanitize=address -fno-omit-frame-pointer
UBSAN_CFLAGS := -fsanitize=undefined -fno-sanitize-recover=undefined
SANITIZE_OPT_CFLAGS := -O2
COVERAGE_CFLAGS := --coverage -O0

# `$(call objs,<sources>)` -> the paths of the matching object files in the
# tree concerned. A target names its sources once; its prerequisites and its
# link line follow from them.
objs = $(patsubst %.c,$(OBJ_DIR)/%.o,$(1))
asan_objs = $(patsubst %.c,$(OBJ_ASAN_DIR)/%.o,$(1))
ubsan_objs = $(patsubst %.c,$(OBJ_UBSAN_DIR)/%.o,$(1))
sanitize_objs = $(patsubst %.c,$(OBJ_SANITIZE_DIR)/%.o,$(1))
coverage_objs = $(patsubst %.c,$(OBJ_COVERAGE_DIR)/%.o,$(1))

# `mkdir -p $(@D)` in the recipe rather than a directory as a prerequisite:
# the mtime of a directory changes with every file written into it, which would
# rebuild everything.
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJ_ASAN_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJ_UBSAN_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(UBSAN_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJ_SANITIZE_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) $(UBSAN_CFLAGS) $(SANITIZE_OPT_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJ_COVERAGE_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

BINDINGS_CORE_UNIT := $(TMP_DIR)/bindings/core.c
DEMO_UNIT := $(TMP_DIR)/demo/demo.c

# The whole engine closure: all of src/ except the entry points of
# src/cmd/, src/p8/palette.c and the modules of src/devtools/ that only the
# tools link (fixed_test_double, replay_source, screencast, session_recorder,
# state), plus the default bindings table generated into
# tmp/bindings/core.c and the demo compiled into tmp/demo/demo.c. Used by the
# targets that call cart_load(), which pulls in the whole port.
ENGINE_SRCS := \
    $(BINDINGS_CORE_UNIT) \
    $(DEMO_UNIT) \
    src/p8/sprite_sheet.c \
    src/game/attract.c \
    src/game/background.c \
    src/game/row_image.c \
    src/game/text_image.c \
    src/game/button_image.c \
    src/game/bindings.c \
    src/engine/bootstrap.c \
    src/game/cheat.c \
    src/game/demo_overlay.c \
    src/game/loading_screen.c \
    src/game/input_step.c \
    src/game/pad_nav.c \
    src/game/pad_panel.c \
    src/game/pad_jump.c \
    src/game/pad_slipway.c \
    src/game/pad_target.c \
    src/game/pad_back.c \
    src/game/pad_select.c \
    src/game/pad_build.c \
    src/game/pad_cycle.c \
    src/game/cart.c \
    src/engine/collect.c \
    src/game/data_init.c \
    src/engine/dispatch_context.c \
    src/devtools/dump.c \
    src/game/economy.c \
    src/game/economy_formulas.c \
    src/game/economy_score.c \
    src/game/economy_tiers.c \
    src/engine/entity_registry.c \
    src/engine/entity_geom.c \
    src/engine/entity_id.c \
    src/engine/entity_id_list.c \
    src/engine/exit_queue.c \
    src/platform/fatal_pc.c \
    src/p8/fixed.c \
    src/game/galaxy.c \
    src/game/game_data.c \
    src/game/gen.c \
    src/game/visual_random.c \
    src/engine/geom.c \
    src/p8/gfx.c \
    src/p8/gfx_software.c \
    src/p8/image.c \
    src/p8/draw_cost.c \
    src/p8/draw_meter.c \
    src/game/help.c \
    src/game/hud.c \
    src/p8/input.c \
    src/devtools/input_json.c \
    src/engine/entity.c \
    src/game/layout.c \
    src/engine/entity_list.c \
    src/engine/value_list.c \
    src/engine/pointer_list.c \
    src/game/map_tables.c \
    src/p8/vram.c \
    src/game/menu.c \
    src/game/mouse_button.c \
    src/game/mouse.c \
    src/game/message_box.c \
    src/game/particle.c \
    src/game/draw_functions.c \
    src/game/planet.c \
    src/game/grab_arena.c \
    src/game/planet_display.c \
    src/game/planet_family.c \
    src/game/planet_family_trade.c \
    src/game/planet_trade.c \
    src/game/probe.c \
    src/game/research.c \
    src/game/research_display.c \
    src/game/structure_icon.c \
    src/game/score_panel.c \
    src/game/score_panel_display.c \
    src/game/scroll.c \
    src/game/slipway.c \
    src/game/slipway_display.c \
    src/p8/str.c \
    src/game/tech_gate.c \
    src/game/trade.c \
    src/game/ui.c \
    src/game/ui_button.c \
    src/engine/vec.c \
    src/game/wormhole.c \
    src/game/wormhole_display.c \
    src/game/save_state.c \
    src/game/save_load.c \
    src/devtools/save_storage_memory.c \
    src/engine/zone.c \
    src/engine/allocator.c \
    src/engine/slot_tally.c \
    src/engine/slot_group.c \
    src/engine/frame_zone.c \
    src/game/ui_zone.c \
    src/game/particle_zone.c \
    src/engine/game_zone.c

# The ported core, free of floating point: the engine closure minus the host
# tools, which keep the libc. constants-check, float-guard, alloc-guard and
# float-grep all read this list.
CORE_SRCS := $(filter-out $(BINDINGS_CORE_UNIT) $(DEMO_UNIT) src/devtools/dump.c src/platform/fatal_pc.c src/devtools/input_json.c,$(ENGINE_SRCS))

# A narrower closure: what data_init() reaches to build a real game state,
# without calling cart_load().
CART_ONLY_SRCS := \
    $(DEMO_UNIT) \
    src/game/attract.c \
    src/game/demo_overlay.c \
    src/game/loading_screen.c \
    src/game/cart.c \
    src/game/cheat.c \
    src/game/input_step.c \
    src/game/pad_nav.c \
    src/game/pad_panel.c \
    src/game/pad_jump.c \
    src/game/pad_slipway.c \
    src/game/pad_target.c \
    src/game/pad_back.c \
    src/game/pad_select.c \
    src/game/pad_build.c \
    src/game/pad_cycle.c \
    src/game/galaxy.c \
    src/game/help.c \
    src/game/menu.c \
    src/game/planet_family_trade.c \
    src/game/score_panel.c \
    src/game/score_panel_display.c \
    src/game/save_state.c \
    src/game/save_load.c \
    src/game/structure_icon.c
DATA_ENGINE_SRCS := $(filter-out $(CART_ONLY_SRCS),$(ENGINE_SRCS))

UNAME_S := $(shell uname -s)

$(TMP_DIR):
	mkdir -p $(TMP_DIR)

# The .json prerequisites are the fixtures that the test reads at run time;
# `$(filter %.o,$^)` keeps them off the link line.
$(TMP_DIR)/test_vram: $(call objs,tests/test_vram.c src/p8/vram.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_sprite_sheet: $(call objs,tests/test_sprite_sheet.c src/p8/sprite_sheet.c src/p8/vram.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_gfx: $(call objs,tests/test_gfx.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/p8/sprite_sheet.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_draw_backend: $(call objs,tests/test_draw_backend.c src/p8/draw_cost.c src/p8/image.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_image: $(call objs,tests/test_image.c src/p8/image.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_draw_cost: $(call objs,tests/test_draw_cost.c src/p8/draw_cost.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_draw_meter: $(call objs,tests/test_draw_meter.c src/p8/draw_meter.c src/p8/draw_cost.c src/p8/image.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_draw_filter: $(call objs,tests/test_draw_filter.c src/devtools/draw_filter.c src/p8/draw_cost.c src/p8/image.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_capture_list: $(call objs,tests/test_capture_list.c src/devtools/capture_list.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)
$(TMP_DIR)/test_input: $(call objs,tests/test_input.c src/p8/input.c src/devtools/input_json.c src/game/bindings.c) tests/fixtures/demo_events.json tests/fixtures/events_valid_ox.json tests/fixtures/events_mouse_recording_refused.json $(wildcard tests/fixtures/events_pad_*.json) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_dump: $(call objs,tests/test_dump.c src/devtools/dump.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_fixed: $(call objs,tests/test_fixed.c src/p8/fixed.c src/devtools/fixed_test_double.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

# The same test as above, under UBSan: p8/fixed.c reinterprets bits between
# signed and unsigned, which would give the right numbers while being undefined
# behavior. `-fno-sanitize-recover` makes the binary fail instead of printing a
# warning.
$(TMP_DIR)/test_fixed_ubsan: $(call ubsan_objs,tests/test_fixed.c src/p8/fixed.c src/devtools/fixed_test_double.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(UBSAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

BINDINGS_DEFAULT_UNIT := $(TMP_DIR)/bindings/default.c

$(TMP_DIR)/bindings_generate: $(call objs,src/cmd/bindings_generate.c src/game/bindings.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(BINDINGS_DEFAULT_UNIT): bindings.default.cfg $(TMP_DIR)/bindings_generate
	@mkdir -p $(@D)
	$(TMP_DIR)/bindings_generate $< $@

$(BINDINGS_CORE_UNIT): bindings.default.cfg $(TMP_DIR)/bindings_generate
	@mkdir -p $(@D)
	$(TMP_DIR)/bindings_generate --default $< $@

bindings-lua: bindings.default.cfg $(TMP_DIR)/bindings_generate ## regenerates game/game/bindings_default.lua from bindings.default.cfg
	$(TMP_DIR)/bindings_generate --lua $< game/game/bindings_default.lua

.PHONY: bindings-lua

$(TMP_DIR)/test_bindings: $(call objs,tests/test_bindings.c src/game/bindings.c $(BINDINGS_DEFAULT_UNIT) $(BINDINGS_CORE_UNIT)) bindings.default.cfg | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_replay_source: $(call objs,tests/test_replay_source.c src/devtools/save_storage_memory.c src/devtools/replay_source.c src/p8/input.c src/devtools/input_json.c src/game/bindings.c) tests/fixtures/events_valid.json tests/fixtures/events_pad_missing_buttons.json tests/fixtures/events_mouse_recording_refused.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_recording_continuity: $(call objs,tests/test_recording_continuity.c src/devtools/save_storage_memory.c src/devtools/session_recorder.c src/devtools/replay_source.c src/p8/input.c src/devtools/input_json.c src/game/bindings.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_marker_coherence: $(call objs,tests/test_marker_coherence.c src/devtools/save_storage_memory.c src/devtools/session_recorder.c src/devtools/replay_source.c src/p8/input.c src/devtools/input_json.c src/game/bindings.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_screencast: $(call objs,tests/test_screencast.c src/devtools/screencast.c src/p8/vram.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

# This test runs the real replay driver, hence $(TMP_DIR)/replay as a
# prerequisite: it replays the same fixture with and without a capture and
# compares the two dumps, so that the capture argument has no effect on the
# rest.
$(TMP_DIR)/test_headless_capture: $(call objs,tests/test_headless_capture.c src/devtools/screencast.c src/devtools/dump.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/p8/input.c src/devtools/input_json.c src/game/bindings.c) $(TMP_DIR)/replay tests/fixtures/screencast_capture_events.json tests/e2e/pad_title.json bindings.default.cfg | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_replay_from_title: $(call objs,tests/test_replay_from_title.c src/devtools/replay_source.c $(ENGINE_SRCS)) $(TMP_DIR)/replay tests/fixtures/title_pick_forgiving.json tests/fixtures/title_pick_reasonable.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_replay_usage: $(call objs,tests/test_replay_usage.c) $(TMP_DIR)/replay tests/fixtures/title_pick_forgiving.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_replay_from_resume: $(call objs,tests/test_replay_from_resume.c) $(TMP_DIR)/replay demo/demo_record.hex bindings.default.cfg tools/end_state.sh tests/fixtures/title_pick_reasonable.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/frames_tool: $(call objs,src/cmd/frames.c src/devtools/screencast.c src/devtools/dump.c src/p8/vram.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

# Runs the replay driver, then the frame emission tool.
$(TMP_DIR)/test_frames_tool: $(call objs,tests/test_frames_tool.c src/devtools/screencast.c src/p8/vram.c) $(TMP_DIR)/replay $(TMP_DIR)/frames_tool tests/fixtures/screencast_capture_events.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_replay_objs := $(call objs,tests/test_fake_love_replay.c tools/fake_love/replay.c)
$(TMP_DIR)/test_fake_love_replay: $(test_fake_love_replay_objs) $(wildcard tests/e2e/*.json) | $(TMP_DIR)
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
$(TMP_DIR)/test_love_e2e: $(test_love_e2e_objs) $(TMP_DIR)/fake_love $(FAKE_LOVE_FIXTURES) tools/love_e2e_run.sh tools/love_e2e_report.sh tools/frames_check.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_love_runtime_objs := $(call objs,tests/test_love_runtime.c tests/fake_love_cli.c src/p8/gfx.c src/p8/gfx_software.c src/p8/image.c src/p8/vram.c src/p8/sprite_sheet.c src/p8/draw_cost.c src/p8/draw_meter.c)
$(TMP_DIR)/test_love_runtime: $(test_love_runtime_objs) $(TMP_DIR)/fake_love $(wildcard game/p8/*.lua game/love/*.lua tests/fixtures/love_runtime*/*.lua tests/fixtures/love_runtime*/*.txt) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_frames_check: $(call objs,tests/test_frames_check.c) tools/frames_check.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_e2e_limits: $(call objs,tests/test_e2e_limits.c) tools/e2e_limits.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_e2e_order: $(call objs,tests/test_e2e_order.c) tools/e2e_order.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_state_peaks: $(call objs,tests/test_state_peaks.c) tools/state_peaks.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_end_state: $(call objs,tests/test_end_state.c) tools/end_state.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_float_guard: $(call objs,tests/test_float_guard.c) tools/float_guard.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_gen_quarter_wave: $(call objs,tests/test_gen_quarter_wave.c) $(TMP_DIR)/gen_quarter_wave src/p8/quarter_wave.h | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_alloc_guard: $(call objs,tests/test_alloc_guard.c) tools/alloc_guard.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_pointer_guard: $(call objs,tests/test_pointer_guard.c) tools/pointer_guard.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_float_grep: $(call objs,tests/test_float_grep.c) tools/float_grep.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_random_guard: $(call objs,tests/test_random_guard.c) tools/random_guard.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_button_guard: $(call objs,tests/test_button_guard.c) tools/button_guard.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_checklist: $(call objs,tests/test_checklist.c) tools/checklist.lua | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_lua_guard: $(call objs,tests/test_lua_guard.c) tools/lua_guard.lua | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_lua_modules: $(call objs,tests/test_lua_modules.c) tests/lua/test_modules.lua | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

test_fake_love_guard_objs := $(call objs,tests/test_fake_love_guard.c)
$(TMP_DIR)/test_fake_love_guard: $(test_fake_love_guard_objs) tools/fake_love_guard.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_float_link: $(call objs,tests/test_float_link.c) tools/float_link.sh | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_constants_check: $(call objs,tests/test_constants_check.c) tools/constants_compare.sh tools/constants_check.sh src/p8/fixed.c src/p8/fixed.h | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_palette: $(call objs,tests/test_palette.c src/p8/palette.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_hud: $(call objs,tests/test_hud.c src/game/hud.c src/game/text_image.c src/p8/image.c src/platform/fatal_pc.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/p8/fixed.c src/p8/sprite_sheet.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_vec: $(call objs,tests/test_vec.c src/engine/vec.c src/p8/fixed.c src/devtools/fixed_test_double.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_geom: $(call objs,tests/test_geom.c src/engine/geom.c src/engine/vec.c src/p8/fixed.c src/devtools/fixed_test_double.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pointer_list: $(call objs,tests/test_pointer_list.c tests/heap_allocator.c src/engine/pointer_list.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_arena: $(call objs,tests/test_arena.c tests/heap_allocator.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_entity_id: $(call objs,tests/test_entity_id.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_entity_registry: $(call objs,tests/test_entity_registry.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_entity_id_list: $(call objs,tests/test_entity_id_list.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_value_list: $(call objs,tests/test_value_list.c tests/heap_allocator.c src/engine/value_list.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_entity_list: $(call objs,tests/test_entity_list.c tests/heap_allocator.c src/engine/entity_list.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)


$(TMP_DIR)/test_str: $(call objs,tests/test_str.c tests/heap_allocator.c src/p8/str.c src/p8/fixed.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_data_init: $(call objs,tests/test_data_init.c $(DATA_ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_map_tables: $(call objs,tests/test_map_tables.c $(DATA_ENGINE_SRCS) \
    src/game/planet_family_trade.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_tooltip_draw_at: $(call objs,tests/test_tooltip_draw_at.c tests/heap_allocator.c $(DATA_ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_toast: $(call asan_objs,tests/test_toast.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_message_box: $(call asan_objs,tests/test_message_box.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_live_slots: $(call asan_objs,tests/test_live_slots.c src/devtools/replay_source.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) $(TMP_DIR)/replay | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_modal_backdrop: $(call objs,tests/test_modal_backdrop.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_mouse: $(call objs,tests/test_mouse.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_main_mouse: $(call objs,tests/test_main_mouse.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_main_camera: $(call objs,tests/test_main_camera.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_camera: $(call objs,tests/test_camera.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_wormhole: $(call objs,tests/test_wormhole.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_background: $(call objs,tests/test_background.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_slipway: $(call objs,tests/test_slipway.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_structure_icon: $(call objs,tests/test_structure_icon.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_button: $(call objs,tests/test_button.c tests/heap_allocator.c src/devtools/largest_slots.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_planet: $(call objs,tests/test_planet.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_visual_random: $(call objs,tests/test_visual_random.c src/game/visual_random.c src/p8/fixed.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_gen: $(call objs,tests/test_gen.c tests/heap_allocator.c src/game/gen.c src/game/game_data.c src/p8/fixed.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_save_state: $(call objs,tests/test_save_state.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_save_storage_memory: $(call objs,tests/test_save_storage_memory.c tests/save_storage_contract.c src/devtools/save_storage_memory.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_structure: $(call objs,tests/test_structure.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_entity_kind: $(call objs,tests/test_entity_kind.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_spec_defaults: $(call objs,tests/test_spec_defaults.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_entity_header: $(call objs,tests/test_entity_header.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_planet_spec_defaults: $(call objs,tests/test_planet_spec_defaults.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_trading_hub: $(call objs,tests/test_trading_hub.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_processor: $(call objs,tests/test_processor.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_ascension: $(call objs,tests/test_ascension.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_protostar: $(call objs,tests/test_protostar.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_synthesizer: $(call objs,tests/test_synthesizer.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_slipgate: $(call objs,tests/test_slipgate.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_infraspace_gate: $(call objs,tests/test_infraspace_gate.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_burst: $(call objs,tests/test_burst.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

STATE_SRCS := src/devtools/state.c src/devtools/state_format.c

$(TMP_DIR)/test_probe: $(call objs,tests/test_probe.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_help_screen: $(call objs,tests/test_help_screen.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_score_panel: $(call objs,tests/test_score_panel.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_ui: $(call objs,tests/test_ui.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_trade: $(call asan_objs,tests/test_trade.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_research_project: $(call objs,tests/test_research_project.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_layout_row: $(call objs,tests/test_layout_row.c tests/heap_allocator.c src/devtools/largest_slots.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_row_image: $(call objs,tests/test_row_image.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_text_image: $(call objs,tests/test_text_image.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_button_image: $(call objs,tests/test_button_image.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_economy: $(call objs,tests/test_economy.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_main_economy: $(call objs,tests/test_main_economy.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_loading_screen: $(call objs,tests/test_loading_screen.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_input_step: $(call objs,tests/test_input_step.c src/devtools/replay_source.c src/devtools/session_recorder.c $(STATE_SRCS) $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_nav: $(call objs,tests/test_pad_nav.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_panel: $(call objs,tests/test_pad_panel.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_entity_geom: $(call objs,tests/test_entity_geom.c tests/heap_allocator.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_target: $(call objs,tests/test_pad_target.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_jump: $(call objs,tests/test_pad_jump.c tests/heap_allocator.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_slipway: $(call objs,tests/test_pad_slipway.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_back: $(call objs,tests/test_pad_back.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_select: $(call objs,tests/test_pad_select.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_metrics: $(call objs,tests/test_pad_metrics.c src/devtools/pad_metrics.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_caption: $(call objs,tests/test_pad_caption.c src/devtools/pad_caption.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/p8/sprite_sheet.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_demo_source: $(call objs,tests/test_demo_source.c src/devtools/demo_source.c src/devtools/pad_caption.c src/devtools/replay_source.c src/devtools/save_storage_memory.c src/devtools/input_json.c src/p8/input.c src/game/bindings.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/p8/sprite_sheet.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/demo_generate: $(call objs,src/cmd/demo_generate.c src/devtools/demo_source.c src/devtools/pad_caption.c src/devtools/replay_source.c src/devtools/save_storage_memory.c src/devtools/input_json.c src/p8/input.c src/game/bindings.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c src/p8/sprite_sheet.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(DEMO_UNIT): demo/demo.json $(TMP_DIR)/demo_generate
	@mkdir -p $(@D)
	$(TMP_DIR)/demo_generate $< $@

$(TMP_DIR)/test_demo_data: $(call objs,tests/test_demo_data.c src/devtools/replay_source.c src/devtools/pad_caption.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) demo/demo.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_build: $(call objs,tests/test_pad_build.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_pad_cycle: $(call objs,tests/test_pad_cycle.c src/game/pad_cycle.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_attract: $(call objs,tests/test_attract.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_demo_overlay: $(call objs,tests/test_demo_overlay.c src/game/demo_overlay.c src/p8/gfx.c src/p8/gfx_software.c src/p8/vram.c $(DEMO_UNIT)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_draw_layers_camera: $(call objs,tests/test_draw_layers_camera.c $(DATA_ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_demo: $(call objs,tests/test_demo.c src/p8/vram.c src/p8/sprite_sheet.c src/p8/gfx.c src/p8/gfx_software.c src/p8/input.c src/devtools/input_json.c src/game/bindings.c src/devtools/dump.c) tests/fixtures/demo_events.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

REPLAY_SRCS := src/cmd/replay.c src/devtools/record_start.c src/devtools/pad_metrics.c src/devtools/screencast.c src/devtools/vram_digest.c src/devtools/draw_filter.c src/devtools/capture_list.c $(STATE_SRCS) src/devtools/replay_source.c $(BINDINGS_DEFAULT_UNIT) $(ENGINE_SRCS)

$(TMP_DIR)/replay: $(call objs,$(REPLAY_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_vram_digest: $(call objs,tests/test_vram_digest.c src/devtools/vram_digest.c src/devtools/screencast.c src/p8/sprite_sheet.c src/p8/vram.c) $(TMP_DIR)/replay tests/e2e/pad_title.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_format: $(call objs,tests/test_format.c src/p8/format.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_png_writer: $(call objs,tests/test_png_writer.c tests/png_reader.c src/devtools/png_writer.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_asset_image: $(call objs,tests/test_asset_image.c tests/png_reader.c src/devtools/asset_image.c src/devtools/png_writer.c src/p8/palette.c src/p8/sprite_sheet.c src/p8/vram.c src/p8/gfx.c src/p8/gfx_software.c) game/sprites.png game/font.png game/p8/sprite_sheet_packed.lua | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_state: $(call objs,tests/test_state.c $(STATE_SRCS) $(ENGINE_SRCS)) $(TMP_DIR)/replay | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_state_format: $(call objs,tests/test_state_format.c $(STATE_SRCS) src/devtools/replay_source.c $(ENGINE_SRCS)) tests/e2e/pad_slipway.json | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_save_check: $(call objs,tests/test_save_check.c src/devtools/save_check.c $(STATE_SRCS) $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_save_load: $(call objs,tests/test_save_load.c src/devtools/save_check.c $(STATE_SRCS) $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_cart: $(call objs,tests/test_cart.c src/devtools/save_check.c $(STATE_SRCS) $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) demo/demo_record.hex | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_record_start: $(call objs,tests/test_record_start.c src/devtools/record_start.c $(ENGINE_SRCS) $(BINDINGS_DEFAULT_UNIT)) demo/demo_record.hex | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_background_draw: $(call objs,tests/test_background_draw.c $(DATA_ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

# Under ASan, with no uninstrumented twin: the crash it targets depends on the
# layout of the allocator (the same replay exits 139 at `-O0` and 0 at `-O1`),
# so only the overflow itself is a reliable oracle.
$(TMP_DIR)/test_planet_exports_row: $(call asan_objs,tests/test_planet_exports_row.c $(DATA_ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_zone: $(call asan_objs,tests/test_zone.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_allocator: $(call asan_objs,tests/test_allocator.c tests/heap_allocator.c src/engine/entity_list.c src/engine/pointer_list.c src/p8/str.c src/engine/collect.c src/engine/geom.c src/engine/vec.c src/p8/fixed.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_particle_zone: $(call asan_objs,tests/test_particle_zone.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_ui_zone: $(call asan_objs,tests/test_ui_zone.c src/game/ui_zone.c src/engine/slot_group.c src/engine/slot_tally.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_slot_tally: $(call asan_objs,tests/test_slot_tally.c src/engine/slot_tally.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/test_exit_queue: $(call asan_objs,tests/test_exit_queue.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_frame_zone: $(call asan_objs,tests/test_frame_zone.c tests/heap_allocator.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_game_zone: $(call asan_objs,tests/test_game_zone.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_burst_exports_row: $(call objs,tests/test_burst_exports_row.c $(DATA_ENGINE_SRCS) src/game/planet_family.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

$(TMP_DIR)/test_probe_spawn_particle_order: $(call objs,tests/test_probe_spawn_particle_order.c $(ENGINE_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

# A reduced source list rather than $(ENGINE_SRCS): this test never calls
# cart_load(). Measured by linking, not guessed.
$(TMP_DIR)/test_layout_nil_item_draws_nothing: $(call objs,tests/test_layout_nil_item_draws_nothing.c tests/heap_allocator.c src/p8/sprite_sheet.c src/engine/collect.c src/engine/dispatch_context.c src/p8/fixed.c src/engine/geom.c src/p8/gfx.c src/p8/gfx_software.c src/game/hud.c src/game/layout.c src/game/text_image.c src/p8/image.c src/engine/pointer_list.c src/game/map_tables.c src/p8/vram.c src/p8/str.c src/engine/vec.c src/engine/allocator.c src/engine/zone.c src/platform/fatal_pc.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) -lm

# The longest first: under `-j`, make starts it first.
UNIT_TESTS := \
    test_vram \
    test_sprite_sheet \
    test_gfx \
    test_draw_backend \
    test_image \
    test_draw_cost \
    test_draw_meter \
    test_draw_filter \
    test_capture_list \
    test_input \
    test_dump \
    test_fixed \
    test_fixed_ubsan \
    test_vec \
    test_geom \
    test_pointer_list \
    test_entity_list \
    test_entity_id \
    test_entity_registry \
    test_entity_id_list \
    test_value_list \
    test_str \
    test_data_init \
    test_map_tables \
    test_draw_layers_camera \
    test_demo \
    test_bindings \
    test_replay_source \
    test_recording_continuity \
    test_marker_coherence \
    test_screencast \
    test_headless_capture \
    test_replay_from_title \
    test_replay_usage \
    test_replay_from_resume \
    test_palette \
    test_hud \
    test_frames_tool \
    test_frames_check \
    test_format \
    test_vram_digest \
    test_png_writer \
    test_asset_image \
    test_e2e_limits \
    test_e2e_order \
    test_state_peaks \
    test_end_state \
    test_float_guard \
    test_alloc_guard \
    test_pointer_guard \
    test_float_grep \
    test_random_guard \
    test_button_guard \
    test_fake_love_guard \
    test_lua_guard \
    test_lua_modules \
    test_float_link \
    test_constants_check \
    test_background_draw \
    test_probe_spawn_particle_order \
    test_planet_exports_row \
    test_zone \
    test_allocator \
    test_arena \
    test_frame_zone \
    test_ui_zone \
    test_slot_tally \
    test_particle_zone \
    test_game_zone \
    test_exit_queue \
    test_burst_exports_row \
    test_layout_nil_item_draws_nothing \
    test_tooltip_draw_at \
    test_toast \
    test_message_box \
    test_state \
    test_state_format \
    test_modal_backdrop \
    test_mouse \
    test_main_mouse \
    test_camera \
    test_main_camera \
    test_wormhole \
    test_background \
    test_slipway \
    test_structure_icon \
    test_button \
    test_planet \
    test_gen \
    test_gen_quarter_wave \
    test_visual_random \
    test_save_state \
    test_save_storage_memory \
    test_save_check \
    test_save_load \
    test_cart \
    test_record_start \
    test_structure \
    test_trading_hub \
    test_entity_kind \
    test_spec_defaults \
    test_entity_header \
    test_planet_spec_defaults \
    test_processor \
    test_ascension \
    test_protostar \
    test_synthesizer \
    test_slipgate \
    test_infraspace_gate \
    test_burst \
    test_probe \
    test_help_screen \
    test_score_panel \
    test_ui \
    test_trade \
    test_research_project \
    test_layout_row \
    test_row_image \
    test_text_image \
    test_button_image \
    test_economy \
    test_main_economy \
    test_input_step \
    test_loading_screen \
    test_pad_nav \
    test_pad_panel \
    test_entity_geom \
    test_pad_target \
    test_pad_jump \
    test_pad_slipway \
    test_pad_back \
    test_pad_select \
    test_pad_build \
    test_pad_cycle \
    test_pad_metrics \
    test_pad_caption \
    test_demo_source \
    test_demo_data \
    test_attract \
    test_demo_overlay \
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

UNIT_RUNS_SPECIAL := unit-run-test_zone unit-run-test_allocator unit-run-test_frame_zone unit-run-test_ui_zone unit-run-test_particle_zone unit-run-test_game_zone unit-run-test_exit_queue unit-run-test_toast unit-run-test_message_box unit-run-test_live_slots unit-run-test_trade unit-run-test_love_runtime

unit: $(UNIT_RUNS) ## builds and runs the binaries of tests/

$(filter-out $(UNIT_RUNS_SPECIAL),$(UNIT_RUNS)): unit-run-%: $(TMP_DIR)/%
	./$<

unit-run-test_zone: $(TMP_DIR)/test_zone
	ASAN_OPTIONS=detect_leaks=0 ./$<

# Arguments: the fixture game directory, then the frame count of its scene.txt.
unit-run-test_love_runtime: $(TMP_DIR)/test_love_runtime
	./$< tests/fixtures/love_runtime 16

unit-run-test_love_runtime_slots: $(TMP_DIR)/test_love_runtime
	./$< tests/fixtures/love_runtime_slots 3

unit-run-test_allocator: $(TMP_DIR)/test_allocator
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_frame_zone: $(TMP_DIR)/test_frame_zone
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_ui_zone: $(TMP_DIR)/test_ui_zone
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_particle_zone: $(TMP_DIR)/test_particle_zone
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_game_zone: $(TMP_DIR)/test_game_zone
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_exit_queue: $(TMP_DIR)/test_exit_queue
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_toast: $(TMP_DIR)/test_toast
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_message_box: $(TMP_DIR)/test_message_box
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_live_slots: $(TMP_DIR)/test_live_slots
	ASAN_OPTIONS=detect_leaks=0 ./$<

unit-run-test_trade: $(TMP_DIR)/test_trade
	ASAN_OPTIONS=detect_leaks=0 ./$<

# These two tests read tmp/replay_dump.txt, the default dump of tmp/replay,
# and test_replay_from_resume writes it: under `-j`, never at the same time.
unit-run-test_frames_tool: | unit-run-test_headless_capture
unit-run-test_replay_from_resume: | unit-run-test_frames_tool

.PHONY: $(UNIT_RUNS) unit-run-test_love_runtime_slots

# Fails before any compilation rather than replay a default file.
ifneq ($(filter replay,$(MAKECMDGOALS)),)
    ifeq ($(strip $(EVENTS)),)
        $(error EVENTS missing: make replay EVENTS=<events.json>)
    endif
endif

replay: $(TMP_DIR)/replay ## EVENTS=<events> [SAVE_HEX=<file>] [DIGEST=<file>]: replays one file headless, dump in tmp/replay_dump.txt
	$(if $(ATTRACT_STATE),ATTRACT_STATE=$(ATTRACT_STATE) )$(if $(REPLAY_STARTS),REPLAY_STARTS=$(REPLAY_STARTS) )$(if $(SAVE_HEX),SAVE_HEX=$(SAVE_HEX) )$(if $(DIGEST),DIGEST=$(DIGEST) )./$(TMP_DIR)/replay $(EVENTS)

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
FAKE_LOVE_CHECKS := $(LOVE_CAPTURE_CHECKS) unit-run-test_fake_love unit-run-test_fake_love_failures unit-run-test_fake_love_joystick unit-run-test_fake_love_files unit-run-test_fake_love_graphics unit-run-test_fake_love_points unit-run-test_fake_love_rectangle unit-run-test_fake_love_set_color unit-run-test_fake_love_image unit-run-test_fake_love_image_draw unit-run-test_fake_love_capture unit-run-test_fake_love_frame_capture unit-run-test_fake_love_frames unit-run-test_fake_love_function_list unit-run-test_fake_love_unlisted unit-run-test_love_e2e unit-run-test_love_runtime unit-run-test_love_runtime_slots

# Longest first: `-j` starts prerequisites in this order, and float-guard
# alone (~4 s, serial) sets the wall time when it starts behind the checks.
test: float-guard float-grep unit-run-test_constants_check unit-run-test_pad_nav alloc-guard constants-check unit $(FAKE_LOVE_CHECKS) lua-test-fixed lua-test-runtime lua-guard lua-test-modules pointer-guard random-guard button-guard data-guard p8-guard fake-love-guard ## fast loop: unit tests and static guards (~5 s)

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
# also covers their count. The `-O0` binary of make test: no second tree to
# build, and readable under gdb.
FRAMES_TOOL := $(TMP_DIR)/frames_tool
FRAMES_DIR := $(TMP_DIR)/frames

frames_capture = $(FRAMES_DIR)/$(1).bin
frames_current = $(FRAMES_DIR)/$(1).sha256
frames_reference = $(1).sha256

# Without .SECONDARY, make would delete these intermediate captures, which
# e2e-ref compares to the reference capture.
.SECONDARY: $(foreach e,$(E2E_EVENTS),$(call frames_capture,$(e)))

# Each capture has its own text dump: the default dump, tmp/replay_dump.txt,
# is read by two tests that run in parallel.
$(FRAMES_DIR)/%.bin: % $(TMP_DIR)/replay
	@mkdir -p $(@D)
	REPLAY_DEPS=$(@:.bin=.d) ./$(TMP_DIR)/replay $< $@ $(@:.bin=.dump.txt) > /dev/null

-include $(wildcard $(FRAMES_DIR)/$(E2E_DIR)/*.json.d)

# The assignment keeps the exit status of sha256, which a pipe into cut would
# lose.
frames_hash = @mkdir -p $(@D); h=$$($(SHA256_CMD) $<) && echo "$${h%% *}" > $@

$(FRAMES_DIR)/%.sha256: $(FRAMES_DIR)/%.bin
	$(frames_hash)

e2e: $(foreach e,$(E2E_EVENTS),$(call frames_current,$(e))) tools/e2e_limits.sh tools/frames_check.sh $(E2E_EXCEPTIONS) ## replays tests/e2e/ and checks each capture against its .json.sha256 (~1 s)
	@status=0; \
	sh tools/e2e_limits.sh $(E2E_EXCEPTIONS) $(E2E_EVENTS) || status=1; \
	sh tools/frames_check.sh $(foreach e,$(E2E_EVENTS),$(e) $(call frames_reference,$(e)) $(call frames_current,$(e))) || status=1; \
	echo "e2e: $(words $(E2E_EVENTS)) e2e in $(E2E_DIR)"; \
	exit $$status

e2e-baseline: $(foreach e,$(E2E_EVENTS),$(call frames_current,$(e))) ## sets or resets the .json.sha256 of tests/e2e/ from the current code
	@$(foreach e,$(E2E_EVENTS),cp $(call frames_current,$(e)) $(call frames_reference,$(e)) && echo "e2e: reference set in $(call frames_reference,$(e))" && ) true

e2e-ref: $(foreach e,$(E2E_EVENTS),$(if $(wildcard $(call frames_reference,$(e))),$(call frames_current,$(e)))) $(FRAMES_TOOL) tools/frames_ref.sh ## for each diverging e2e, replays the commit that set its reference and gives the first diverging frame
	@sh tools/frames_ref.sh ./$(FRAMES_TOOL) $(FRAMES_DIR) $(TMP_DIR)/frames-ref $(foreach e,$(E2E_EVENTS),$(e) $(call frames_reference,$(e)) $(call frames_current,$(e)) $(call frames_capture,$(e)))

# A `%` in LOVE_E2E_GAME becomes the replay's file name: one game directory
# per replay. Forced: make does not see a Lua edit.
LOVE_E2E_GAME ?= game
LOVE_E2E_DIR := $(TMP_DIR)/love-e2e
LOVE_E2E_HASHES := $(foreach e,$(E2E_EVENTS),$(LOVE_E2E_DIR)/$(e).sha256)

$(LOVE_E2E_HASHES): $(LOVE_E2E_DIR)/%.sha256: % $(TMP_DIR)/fake_love tools/love_e2e_run.sh FORCE
	@mkdir -p $(@D)
	@sh tools/love_e2e_run.sh "$(SHA256_CMD)" ./$(TMP_DIR)/fake_love $(subst %,$(notdir $*),$(LOVE_E2E_GAME)) $* $(@:.sha256=.bin) $@ $(@:.sha256=.error)

love-e2e: $(LOVE_E2E_HASHES) tools/love_e2e_report.sh tools/frames_check.sh ## runs tmp/fake_love on every tests/e2e/ replay in parallel and checks each capture's sha256 (LOVE_E2E_GAME=game)
	@sh tools/love_e2e_report.sh $(foreach e,$(E2E_EVENTS),$(e) $(call frames_reference,$(e)) $(LOVE_E2E_DIR)/$(e).sha256 $(LOVE_E2E_DIR)/$(e).error)

FORCE:

.PHONY: love-e2e FORCE

# offsetof, void *self, a field read by name, a spec or defaults table: the
# model the composition migration removed, which a port of the cart brings
# back first.
DATA_GUARD_PATTERN := \boffsetof\b|\bvoid[[:space:]]*\*[[:space:]]*(const[[:space:]]+)?self|\b[a-z0-9_]*field_truthy\(|(->|\.)(set_)?field\b|\b[a-z0-9_]+_take_[a-z0-9_]+\(|\boverrides\b|\bg_[a-z0-9_]+_(spec|defaults)\b|\b[a-z0-9_]+_default_(fixed|table|vec|string|box|list|bool|hf)\b

# grep returns 1 when nothing matches; anything above is an error.
data-guard: ## fails on a pre-composition model pattern in src/ or tests/
	@status=0; grep -rnE '$(DATA_GUARD_PATTERN)' --include='*.c' --include='*.h' src tests || status=$$?; \
	if [ $$status -eq 0 ]; then echo "data-guard: pre-composition pattern above"; exit 1; fi; \
	[ $$status -eq 1 ] || exit $$status; \
	echo "data-guard: clean"

.PHONY: data-guard

P8_GUARD_PATTERN := \bp8_(stat|sfx|music|play_sfx|peek|poke|memcpy|memory_t)\b|\bP8_ADDR_[A-Z_]+|\b0x(5[fF]80|3101|5[fF]2[dD])\b|\b(entities_by_tag|entities_by_message|draw_layers|entity_set_state|entity_init_instance)\("|\bentity_state_of\b|map\.json

p8-guard: ## fails on a PICO-8-only API in src/ or tests/
	@status=0; grep -rnE '$(P8_GUARD_PATTERN)' --include='*.c' --include='*.h' src tests || status=$$?; \
	if [ $$status -eq 0 ]; then echo "p8-guard: PICO-8-only pattern above"; exit 1; fi; \
	[ $$status -eq 1 ] || exit $$status; \
	echo "p8-guard: clean"

.PHONY: p8-guard

$(TMP_DIR)/replay_sanitize: $(call sanitize_objs,$(REPLAY_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) $(UBSAN_CFLAGS) -o $@ $(filter %.o,$^)

SANITIZE_DIR := $(TMP_DIR)/sanitize
SANITIZE_RUNS := $(addprefix sanitize-run-,$(E2E_EVENTS))

$(SANITIZE_RUNS): sanitize-run-%: % $(TMP_DIR)/replay_sanitize
	@mkdir -p $(SANITIZE_DIR)/$(<D)
	@./$(TMP_DIR)/replay_sanitize $< /dev/null $(SANITIZE_DIR)/$<.dump.txt > $(SANITIZE_DIR)/$<.log 2>&1 || { cat $(SANITIZE_DIR)/$<.log; echo "sanitize: $< dirty"; exit 1; }

sanitize-net: $(SANITIZE_RUNS) unit-run-test_live_slots ## replays tests/e2e/ under ASan+UBSan, then test_live_slots (~17 s)
	@echo "sanitize: $(words $(SANITIZE_RUNS)) clean"

.PHONY: sanitize-net $(SANITIZE_RUNS)

FULL_GAME_EVENTS := worst-case/full_game.json worst-case/full_game_new_game.json worst-case/full_game_pause_restart.json worst-case/full_game_pause_idle.json worst-case/title_idle.json
FULL_GAME_DIR := $(TMP_DIR)/full-game

full-game: $(FULL_GAME_EVENTS) $(TMP_DIR)/replay ## replays the worst-case/ games to their last frame, fails on any p8_fatal or full zone
	@mkdir -p $(FULL_GAME_DIR)
	@$(foreach e,$(FULL_GAME_EVENTS),./$(TMP_DIR)/replay $(e) /dev/null $(FULL_GAME_DIR)/$(notdir $(e)).dump.txt > $(FULL_GAME_DIR)/$(notdir $(e)).log 2>&1 || { cat $(FULL_GAME_DIR)/$(notdir $(e)).log; echo "full-game: $(e) stopped"; exit 1; }; echo "full-game: $(e) clean"; ) true

.PHONY: full-game

$(TMP_DIR)/replay_coverage: $(call coverage_objs,$(REPLAY_SRCS)) | $(TMP_DIR)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -o $@ $(filter %.o,$^)

COVERAGE_EVENTS ?= $(E2E_EVENTS)

coverage: $(TMP_DIR)/replay_coverage tools/coverage.sh $(COVERAGE_EVENTS) ## [COVERAGE_EVENTS=<files>]: line coverage of src/game/ and src/engine/ in tmp/coverage/lines.txt
	@sh tools/coverage.sh ./$(TMP_DIR)/replay_coverage $(OBJ_COVERAGE_DIR) $(TMP_DIR)/coverage "$(REPLAY_SRCS)" $(COVERAGE_EVENTS)

.PHONY: coverage

E2E_ORDER_DIR := $(TMP_DIR)/e2e-order
e2e_order = sh tools/e2e_order.sh tools/coverage.sh ./$(TMP_DIR)/replay_coverage $(OBJ_COVERAGE_DIR) $(E2E_ORDER_DIR) "$(REPLAY_SRCS)" $(1) $(E2E_EVENTS)

e2e-order: $(TMP_DIR)/replay_coverage tools/coverage.sh tools/e2e_order.sh $(E2E_EVENTS) ## writes tests/e2e/order.txt: the replays ranked by C lines of src/game/ and src/engine/ executed
	@$(call e2e_order,$(E2E_DIR)/order.txt)
	@echo "e2e-order: $(words $(E2E_EVENTS)) replays ranked in $(E2E_DIR)/order.txt"

e2e-order-check: $(TMP_DIR)/replay_coverage tools/coverage.sh tools/e2e_order.sh $(E2E_EVENTS) ## re-measures the replay order and fails when it differs from tests/e2e/order.txt
	@$(call e2e_order,$(E2E_ORDER_DIR)/order.txt)
	@diff -u $(E2E_DIR)/order.txt $(E2E_ORDER_DIR)/order.txt || { echo "e2e-order-check: $(E2E_DIR)/order.txt differs from the measurement"; exit 1; }
	@echo "e2e-order-check: $(E2E_DIR)/order.txt reproduced"

.PHONY: e2e-order e2e-order-check

STATE_DIR := $(TMP_DIR)/state
STATE_EVENTS := $(FULL_GAME_EVENTS) $(E2E_EVENTS)
STATE_RUNS := $(addprefix state-run-,$(STATE_EVENTS))
state_file = $(STATE_DIR)/$(basename $(notdir $(1))).txt

# A stopped replay does not stop the others: tools/state_peaks.sh judges each
# from its state file, removed first so that a stale one never counts.
$(STATE_RUNS): state-run-%: % $(TMP_DIR)/replay
	@mkdir -p $(STATE_DIR)
	@rm -f $(call state_file,$<)
	@STATE=$(call state_file,$<) ./$(TMP_DIR)/replay $< /dev/null $(basename $(call state_file,$<)).dump.txt > $(basename $(call state_file,$<)).log 2>&1 || true

.PHONY: $(STATE_RUNS)

ifneq ($(strip $(EVENTS)),)
state: $(TMP_DIR)/replay ## [EVENTS=<events>]: last-frame state in tmp/state.txt; without EVENTS, zone and cap peaks over every replay
	$(if $(ATTRACT_STATE),ATTRACT_STATE=$(ATTRACT_STATE) )$(if $(REPLAY_STARTS),REPLAY_STARTS=$(REPLAY_STARTS) )$(if $(SAVE_HEX),SAVE_HEX=$(SAVE_HEX) )$(if $(DIGEST),DIGEST=$(DIGEST) )STATE=$(TMP_DIR)/state.txt ./$(TMP_DIR)/replay $(EVENTS)
else
state: $(STATE_RUNS) tools/state_peaks.sh
	@sh tools/state_peaks.sh $(TMP_DIR)/state.txt $(foreach e,$(STATE_EVENTS),$(e) $(call state_file,$(e)))
	@echo "state: $(words $(STATE_EVENTS)) replays, peaks in $(TMP_DIR)/state.txt"
endif

END_STATE_DIR := $(TMP_DIR)/end-state
end_state_triples = $(foreach e,$(1),$(e) $(call state_file,$(e)) $(e).end-state)

ifneq ($(strip $(EVENTS)),)
end-state: $(TMP_DIR)/replay tools/end_state.sh ## [EVENTS=<events> [REFERENCE=<file>]]: compares last-frame states with the committed .end-state files
	@mkdir -p $(END_STATE_DIR)
	@rm -f $(END_STATE_DIR)/state.txt
	@STATE=$(END_STATE_DIR)/state.txt ./$(TMP_DIR)/replay $(EVENTS) /dev/null $(END_STATE_DIR)/dump.txt > $(END_STATE_DIR)/log.txt 2>&1 || true
	@sh tools/end_state.sh check $(EVENTS) $(END_STATE_DIR)/state.txt $(or $(REFERENCE),$(EVENTS).end-state)
else
end-state: $(STATE_RUNS) tools/end_state.sh
	@sh tools/end_state.sh check $(call end_state_triples,$(STATE_EVENTS))
endif

end-state-baseline: $(STATE_RUNS) tools/end_state.sh ## writes each missing .end-state, never overwrites one
	@sh tools/end_state.sh baseline $(call end_state_triples,$(STATE_EVENTS))

.PHONY: end-state end-state-baseline

# The core sources, where the comparator reads the P8_FIXED_LIT calls.
constants-check: tools/constants_check.sh tools/constants_compare.sh tools/constants_expected_gaps.txt tools/pico8_reference_probe_out.txt src/p8/fixed.c src/p8/fixed.h $(CORE_SRCS) | $(TMP_DIR) ## compares each P8_FIXED_LIT with its recorded PICO-8 value
	@sh tools/constants_check.sh tools/constants_expected_gaps.txt tools/pico8_reference_probe_out.txt src/p8/fixed.c $(TMP_DIR)/constants_check \
	    $(CORE_SRCS)

.PHONY: constants-check

FLOAT_GUARD := tools/float_guard.txt

# The report is rewritten even when the guard fails; the exit status of the
# script is returned after the move.
float-guard: tools/float_guard.sh | $(TMP_DIR) ## fails if a core file needs floating point
	@status=0; sh tools/float_guard.sh "$(CC)" "$(CFLAGS)" $(TMP_DIR)/float-guard $(CORE_SRCS) > $(TMP_DIR)/float_guard.txt || status=$$?; \
	mv $(TMP_DIR)/float_guard.txt $(FLOAT_GUARD); \
	grep '^float-guard ' $(FLOAT_GUARD); \
	exit $$status

.PHONY: float-guard

ALLOC_GUARD_EXCEPTIONS := tools/alloc_guard_exceptions.txt

alloc-guard: tools/alloc_guard.sh $(ALLOC_GUARD_EXCEPTIONS) $(call objs,$(CORE_SRCS)) ## ratchet on the malloc/free calls of the core (tools/alloc_guard_exceptions.txt)
	@sh tools/alloc_guard.sh $(ALLOC_GUARD_EXCEPTIONS) $(OBJ_DIR) $(call objs,$(CORE_SRCS))

.PHONY: alloc-guard

POINTER_GUARD_EXCEPTIONS := tools/pointer_guard_exceptions.txt
POINTER_GUARD_SRCS := $(wildcard src/game/*.[ch]) src/engine/entity_class.h

pointer-guard: tools/pointer_guard.sh $(POINTER_GUARD_EXCEPTIONS) $(POINTER_GUARD_SRCS) ## ratchet on data pointers in src/game/ (tools/pointer_guard_exceptions.txt)
	@sh tools/pointer_guard.sh $(POINTER_GUARD_EXCEPTIONS) $(POINTER_GUARD_SRCS)

.PHONY: pointer-guard

FLOAT_GREP_EXCLUSIONS := tools/float_grep_exclusions.txt

float-grep: tools/float_grep.sh $(FLOAT_GREP_EXCLUSIONS) ## fails on any floating-point type or call in the core
	@sh tools/float_grep.sh "$(CC)" "$(CFLAGS)" $(FLOAT_GREP_EXCLUSIONS) src $(CORE_SRCS)

.PHONY: float-grep

RANDOM_GUARD_EXCEPTIONS := tools/random_guard_exceptions.txt

random-guard: tools/random_guard.sh $(RANDOM_GUARD_EXCEPTIONS) ## ratchet on the random stream names per file (tools/random_guard_exceptions.txt)
	@sh tools/random_guard.sh $(RANDOM_GUARD_EXCEPTIONS) src

.PHONY: random-guard

# `<binary> <objects>` per binary of the fake side: each name below has its
# `<name>_objs` beside its link rule. tmp/luajit_version has no object: the
# include scan of tools/love_capture/ covers it.
FAKE_SIDE_BINARIES := fake_love love_capture_check test_fake_love_replay test_fake_love_pad test_fake_love_canvas test_fake_love_image_data test_fake_love_image_rules test_fake_love_point test_fake_love_transform test_fake_love_disk test_fake_love_png_file test_fake_love test_fake_love_failures test_fake_love_joystick test_fake_love_files test_fake_love_graphics test_fake_love_points test_fake_love_rectangle test_fake_love_set_color test_fake_love_image test_fake_love_image_draw test_fake_love_capture test_fake_love_frame_capture test_fake_love_frames test_fake_love_function_list test_fake_love_unlisted test_fake_love_guard test_love_e2e
FAKE_SIDE_LINKS := $(foreach binary,$(FAKE_SIDE_BINARIES),$(TMP_DIR)/$(binary) $($(binary)_objs))
FAKE_SIDE_SRCS := tools/fake_love tools/love_capture $(wildcard tests/test_fake_love*.c tests/fake_love_*.[ch]) tests/test_love_e2e.c

fake-love-guard: tools/fake_love_guard.sh $(filter %.o,$(FAKE_SIDE_LINKS)) ## fails when the fake side includes a header or links an object from src/
	@sh tools/fake_love_guard.sh $(OBJ_DIR) $(FAKE_SIDE_LINKS) -- $(FAKE_SIDE_SRCS)

.PHONY: fake-love-guard

button-guard: tools/button_guard.sh ## fails when a src/game/ file other than cheat.c and the bindings names a P8_PAD_BUTTON_*
	@sh tools/button_guard.sh src/game

.PHONY: button-guard

float-link: $(call objs,$(REPLAY_SRCS)) tools/float_link.sh ## links the replay objects without -lm, never run
	@sh tools/float_link.sh "$(CC)" "$(CFLAGS)" $(TMP_DIR)/float-link/replay $(call objs,$(REPLAY_SRCS))

.PHONY: float-link

$(TMP_DIR)/gen_quarter_wave: tools/gen_quarter_wave.c | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $< -lm

sin-table: $(TMP_DIR)/gen_quarter_wave ## regenerates src/p8/quarter_wave.h and its Lua copy game/p8/quarter_wave.lua
	./$(TMP_DIR)/gen_quarter_wave > $(TMP_DIR)/quarter_wave.h
	./$(TMP_DIR)/gen_quarter_wave --lua > $(TMP_DIR)/quarter_wave.lua
	mv $(TMP_DIR)/quarter_wave.h src/p8/quarter_wave.h
	mv $(TMP_DIR)/quarter_wave.lua game/p8/quarter_wave.lua

.PHONY: sin-table

FIXED_SWEEP_COUNT ?= 400000
FIXED_SWEEP_SEED ?= 0x2545F491

$(TMP_DIR)/fixed_reference: $(call objs,tools/fixed_reference.c src/p8/fixed.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TMP_DIR)/fixed_reference.bin: $(TMP_DIR)/fixed_reference
	./$(TMP_DIR)/fixed_reference $@.part
	mv $@.part $@

lua-test-fixed: tests/lua/test_fixed.lua $(wildcard game/p8/*.lua) $(TMP_DIR)/fixed_reference.bin ## compares game/p8/fixed.lua with the C fixed-point library on edge and sampled values
	luajit tests/lua/test_fixed.lua $(TMP_DIR)/fixed_reference.bin

lua-sweep-fixed: tests/lua/test_fixed.lua $(wildcard game/p8/*.lua) $(TMP_DIR)/fixed_reference ## compares game/p8/fixed.lua with the C library on millions of sampled values (slow)
	./$(TMP_DIR)/fixed_reference --sweep $(FIXED_SWEEP_COUNT) $(FIXED_SWEEP_SEED) | luajit tests/lua/test_fixed.lua -

lua-test-runtime: tests/lua/test_runtime.lua $(wildcard game/p8/*.lua) ## checks the Lua-only behaviour of game/p8/ modules
	luajit tests/lua/test_runtime.lua

.PHONY: lua-test-fixed lua-sweep-fixed lua-test-runtime

# wildcard, not find: the guard stays a single process.
LUA_GUARD_FILES := $(wildcard game/*.lua game/*/*.lua game/*/*/*.lua)

lua-guard: tools/lua_guard.lua ## fails when a game/ Lua file does not compile, uses an undeclared global or an unlisted LÖVE name
	@luajit tools/lua_guard.lua game $(LUA_GUARD_FILES)

.PHONY: lua-guard

# One unit of tools/table_reference/ per source holding compared tables: the
# unit includes the source to reach its statics, so the source's own object
# leaves the link. The unit list reaches table_reference.o as a -D flag, hence
# its rebuild on a Makefile edit.
TABLE_REFERENCE_SOURCES := src/p8/quarter_wave.h src/p8/sprite_sheet_packed.h src/p8/palette.c src/p8/draw_cost.c src/p8/draw_meter.c src/p8/gfx_software.c src/p8/image.c src/engine/zone.h src/engine/zone_sizes.h src/engine/allocator.h src/engine/slot_tally.h src/engine/pointer_list.h src/engine/geom.h src/engine/entity_id_type.h src/engine/entity_class.h src/engine/entity_id_list.h src/engine/entity_list.h src/engine/exit_queue.h src/game/map_tables.c src/game/game_data.c src/engine/entity_registry.c src/game/gen.h src/game/grab_arena.c src/game/targeting.h src/game/bindings.c src/game/image_slot.h src/game/button_image.c src/game/mouse.c src/game/main_economy.h src/game/economy_formulas.h src/game/trade.h src/game/research.c src/game/hud.c src/game/draw_functions.c src/game/layout.c src/game/ui_zone.c src/game/planet_display.h
TABLE_REFERENCE_UNITS := $(foreach source,$(TABLE_REFERENCE_SOURCES),$(subst /,_,$(basename $(patsubst src/%,%,$(source)))))
table_reference_objs := $(call objs,tools/table_reference.c $(patsubst %,tools/table_reference/%.c,$(TABLE_REFERENCE_UNITS)) $(filter-out $(TABLE_REFERENCE_SOURCES),$(ENGINE_SRCS)))

$(call objs,tools/table_reference.c): override CFLAGS += -DTABLE_REFERENCE_UNITS='$(foreach unit,$(TABLE_REFERENCE_UNITS),X($(unit)))'
$(call objs,tools/table_reference.c): Makefile

$(TMP_DIR)/table_reference: $(table_reference_objs) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

$(TMP_DIR)/table_reference.txt: $(TMP_DIR)/table_reference
	./$< $@.part
	mv $@.part $@

lua-test-modules: tests/lua/test_modules.lua $(TMP_DIR)/table_reference.txt ## requires every game/ module under plain LuaJIT, compares its data tables and constants with the C ones
	@luajit tests/lua/test_modules.lua game $(TMP_DIR)/table_reference.txt $(LUA_GUARD_FILES)

.PHONY: lua-test-modules

$(TMP_DIR)/asset_export: $(call objs,src/cmd/asset_export.c src/devtools/asset_image.c src/devtools/png_writer.c src/p8/palette.c src/p8/sprite_sheet.c src/p8/vram.c src/p8/gfx_software.c src/p8/gfx.c) | $(TMP_DIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^)

asset-export: $(TMP_DIR)/asset_export ## writes game/sprites.png, game/font.png and game/p8/sprite_sheet_packed.lua from the C sprite sheet and font
	mkdir -p game
	./$(TMP_DIR)/asset_export game

.PHONY: asset-export

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

checklist: $(BINDINGS_CORE_UNIT) $(DEMO_UNIT) love-check $(TMP_DIR)/test_checklist tools/checklist.txt $(TMP_DIR)/table_reference.txt ## tests the checklist script, lists each C function's Lua copy; STEP=<id> fails on missing or uncompared ones up to it
	./$(TMP_DIR)/test_checklist
	luajit tools/checklist.lua --listing $(TMP_DIR)/checklist.txt --reference $(TMP_DIR)/table_reference.txt$(if $(STEP), --step $(STEP))

.PHONY: checklist

# The dependency fragments written by the compiler, one per object. The
# wildcard tolerates their absence: on a fresh clone, everything is compiled.
-include $(wildcard $(OBJ_DIR)/src/*/*.d $(OBJ_DIR)/tests/*.d $(OBJ_DIR)/tools/*.d $(OBJ_DIR)/tools/*/*.d $(OBJ_DIR)/tmp/bindings/*.d $(OBJ_DIR)/tmp/demo/*.d)
-include $(wildcard $(OBJ_ASAN_DIR)/src/*/*.d $(OBJ_ASAN_DIR)/tests/*.d $(OBJ_ASAN_DIR)/tmp/bindings/*.d $(OBJ_ASAN_DIR)/tmp/demo/*.d)
-include $(wildcard $(OBJ_UBSAN_DIR)/src/*/*.d $(OBJ_UBSAN_DIR)/tests/*.d)
-include $(wildcard $(OBJ_SANITIZE_DIR)/src/*/*.d $(OBJ_SANITIZE_DIR)/tests/*.d $(OBJ_SANITIZE_DIR)/tmp/bindings/*.d $(OBJ_SANITIZE_DIR)/tmp/demo/*.d)
-include $(wildcard $(OBJ_COVERAGE_DIR)/src/*/*.d $(OBJ_COVERAGE_DIR)/tmp/bindings/*.d $(OBJ_COVERAGE_DIR)/tmp/demo/*.d)

endif
