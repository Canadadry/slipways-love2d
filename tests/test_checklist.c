#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_checklist_work"
#define ROOT DIR "/tree"
#define OUT DIR "/out.txt"
#define LISTING DIR "/listing.txt"
#define CHECKLIST "luajit tools/checklist.lua --root " ROOT " --listing " LISTING

/* Static buffer, NUL-terminated, overwritten by each call; "" when the file is absent. */
static const char *read_file(const char *path) {
    static char buf[1 << 16];
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        buf[0] = '\0';
        return buf;
    }
    size_t len = fread(buf, 1, sizeof(buf) - 1, file);
    fclose(file);
    buf[len] = '\0';
    return buf;
}

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

static int run(const char *command) {
    char line[512];
    snprintf(line, sizeof(line), "%s > " OUT " 2>&1; echo $? > " DIR "/code", command);
    assert(system(line) == 0);
    return atoi(read_file(DIR "/code"));
}

static int has(const char *path, const char *text) {
    return strstr(read_file(path), text) != NULL;
}

static void reset(void) {
    assert(system("rm -rf " ROOT " && mkdir -p " ROOT "/src/p8 " ROOT "/src/engine " ROOT "/src/game " ROOT
                  "/src/devtools " ROOT "/game/p8 " ROOT "/game/engine " ROOT "/game/game") == 0);
    remove(LISTING);
}

static void test_a_function_is_listed_missing_with_its_default_lua_mapping(void) {
    reset();
    write_file(ROOT "/src/game/planet.c", "#include \"game/planet.h\"\n\nint planet_count(void) {\n    return 3;\n}\n");
    assert(run(CHECKLIST) == 0);
    assert(has(LISTING, "- function MISSING src/game/planet.c:3 planet_count -> game/game/planet.lua planet_count\n"));
    assert(has(OUT, "checklist: 0 ported, 1 missing, 0 not ported, of 1 functions; "
                    "0 ported, 0 missing, 0 not ported, of 0 tables; 1 files\n"));
}

static void test_signatures_across_lines_static_and_attributes_are_functions(void) {
    reset();
    write_file(ROOT "/src/engine/allocator.c",
               "static void *\n"
               "allocator_grow(allocator_t *allocator,\n"
               "               size_t size) {\n"
               "    return 0;\n"
               "}\n"
               "__attribute__((noreturn)) P8_NORETURN void allocator_fail(const char *why) { for (;;) {} }\n");
    assert(run(CHECKLIST) == 0);
    assert(has(LISTING, "- function MISSING src/engine/allocator.c:2 allocator_grow -> game/engine/allocator.lua allocator_grow\n"));
    assert(has(LISTING, "- function MISSING src/engine/allocator.c:6 allocator_fail -> game/engine/allocator.lua allocator_fail\n"));
}

static void test_header_inlines_map_to_the_module_lua_file(void) {
    reset();
    write_file(ROOT "/src/engine/vec.h", "#ifndef VEC_H\n#define VEC_H\n"
                                         "static inline int vec_dot(int a, int b) { return a * b; }\n"
                                         "int vec_length(int a);\n#endif\n");
    write_file(ROOT "/src/engine/flags.h", "enum { FLAG_A = 1 };\nint flags_get(void);\n");
    assert(run(CHECKLIST) == 0);
    assert(has(LISTING, "- file MISSING src/engine/vec.h -> game/engine/vec.lua\n"));
    assert(has(LISTING, "- function MISSING src/engine/vec.h:3 vec_dot -> game/engine/vec.lua vec_dot\n"));
    assert(!has(LISTING, "vec_length"));
    assert(has(LISTING, "- file no-lua src/engine/flags.h -> game/engine/flags.lua\n"));
    assert(!has(LISTING, "flags_get"));
}

static void test_comments_strings_and_preprocessor_lines_hide_their_braces(void) {
    reset();
    write_file(ROOT "/src/game/hud.c",
               "/* int fake_block(void) { */\n"
               "// int fake_line(void) {\n"
               "#define BODY(x) \\\n"
               "    int fake_macro(void) { \\\n"
               "        return x; }\n"
               "static const char *hud_title = \"int fake_string(void) { \\\" }\";\n"
               "int hud_draw(char c) { return c == '{' || c == '\"'; }\n");
    assert(run(CHECKLIST) == 0);
    const char *listing = read_file(LISTING);
    assert(strstr(listing, "fake_") == NULL);
    assert(strstr(listing, "- table MISSING src/game/hud.c:6 hud_title -> game/game/hud.lua hud_title\n") != NULL);
    assert(strstr(listing, "- function MISSING src/game/hud.c:7 hud_draw -> game/game/hud.lua hud_draw\n") != NULL);
}

static void test_tables_are_brace_or_string_initializers_at_file_scope(void) {
    reset();
    write_file(ROOT "/src/game/menu.c",
               "typedef struct {\n    int x;\n} menu_item_t;\n"
               "struct menu_state { int open; } menu_state_value;\n"
               "struct menu_pair { int a, b; } menu_pair_value = {1, 2};\n"
               "static const int menu_widths[3] = {1, 2, 3};\n"
               "const char menu_name[] = \"menu\";\n"
               "static void (*const menu_actions[2])(int) = {0, 0};\n"
               "static int g_menu_up = 0;\n"
               "static int g_menu_count;\n"
               "extern const int menu_extern[] = {1};\n"
               "extern int menu_open(int x);\n"
               "int menu_open(int x) {\n    static const int menu_local[] = {1};\n    return menu_local[x];\n}\n");
    assert(run(CHECKLIST) == 0);
    const char *listing = read_file(LISTING);
    assert(strstr(listing, "- table MISSING src/game/menu.c:5 menu_pair_value -> game/game/menu.lua menu_pair_value\n") != NULL);
    assert(strstr(listing, "- table MISSING src/game/menu.c:6 menu_widths ->") != NULL);
    assert(strstr(listing, "- table MISSING src/game/menu.c:7 menu_name ->") != NULL);
    assert(strstr(listing, "- table MISSING src/game/menu.c:8 menu_actions ->") != NULL);
    assert(strstr(listing, "- function MISSING src/game/menu.c:13 menu_open ->") != NULL);
    assert(strstr(listing, "menu_item_t") == NULL && strstr(listing, "menu_state_value") == NULL);
    assert(strstr(listing, "g_menu") == NULL && strstr(listing, "menu_extern") == NULL);
    assert(strstr(listing, "menu_local") == NULL);
    assert(has(OUT, "of 1 functions; 0 ported, 4 missing, 0 not ported, of 4 tables; 1 files\n"));
}

static void test_an_unparseable_top_level_brace_exits_2_with_file_and_line(void) {
    reset();
    write_file(ROOT "/src/game/good.c", "int good(void) { return 1; }\n");
    write_file(ROOT "/src/game/bad.c", "int x;\n{ int y; }\n");
    write_file(ROOT "/src/p8/worse.c", "int z = 1;\nint w = 2 {\n");
    assert(run(CHECKLIST) == 2);
    assert(has(OUT, "checklist: src/game/bad.c:2: "));
    assert(has(OUT, "checklist: src/p8/worse.c:2: "));
    assert(!has(OUT, "checklist: 0 ported"));
}

static void test_each_lua_definition_form_counts_as_ported(void) {
    reset();
    write_file(ROOT "/src/game/ui.c",
               "int ui_a(void) { return 0; }\nint ui_b(void) { return 0; }\nint ui_c(void) { return 0; }\n"
               "int ui_d(void) { return 0; }\nint ui_e(void) { return 0; }\nint ui_f(void) { return 0; }\n"
               "int ui_g(void) { return 0; }\nint ui_h(void) { return 0; }\n"
               "const int ui_t1[] = {1};\nconst int ui_t2[] = {1};\nconst int ui_t3[] = {1};\nconst int ui_t4[] = {1};\n");
    write_file(ROOT "/game/game/ui.lua",
               "function ui_a(x)\nend\n"
               "  function M.ui_b (x) end\n"
               "function M:ui_c()\nend\n"
               "local function ui_d()\nend\n"
               "local ui_e = function() end\n"
               "M.ui_f = function() end\n"
               "ui_g = function() end\n"
               "  -- function ui_h()\n"
               "local ui_t1 = {}\nM.ui_t2 = {}\nui_t3 = {}\n"
               "-- ui_t4 = {}\nif ui_t4 == nil then end\n");
    assert(run(CHECKLIST) == 0);
    const char *listing = read_file(LISTING);
    assert(strstr(listing, "- file present src/game/ui.c -> game/game/ui.lua\n") != NULL);
    assert(strstr(listing, "function ported src/game/ui.c:1 ui_a ->") != NULL);
    assert(strstr(listing, "function ported src/game/ui.c:2 ui_b ->") != NULL);
    assert(strstr(listing, "function ported src/game/ui.c:3 ui_c ->") != NULL);
    assert(strstr(listing, "function ported src/game/ui.c:4 ui_d ->") != NULL);
    assert(strstr(listing, "function ported src/game/ui.c:5 ui_e ->") != NULL);
    assert(strstr(listing, "function ported src/game/ui.c:6 ui_f ->") != NULL);
    assert(strstr(listing, "function ported src/game/ui.c:7 ui_g ->") != NULL);
    assert(strstr(listing, "function MISSING src/game/ui.c:8 ui_h ->") != NULL);
    assert(strstr(listing, "table ported src/game/ui.c:9 ui_t1 ->") != NULL);
    assert(strstr(listing, "table ported src/game/ui.c:10 ui_t2 ->") != NULL);
    assert(strstr(listing, "table ported src/game/ui.c:11 ui_t3 ->") != NULL);
    assert(strstr(listing, "table MISSING src/game/ui.c:12 ui_t4 ->") != NULL);
    assert(has(OUT, "checklist: 7 ported, 1 missing, 0 not ported, of 8 functions; "
                    "3 ported, 1 missing, 0 not ported, of 4 tables; 1 files\n"));
}

static void test_state_format_is_scanned_and_other_devtools_are_not(void) {
    reset();
    write_file(ROOT "/src/devtools/state_format.c", "void state_format_print(void) {}\n");
    write_file(ROOT "/src/devtools/dump.c", "void dump_print(void) {}\n");
    assert(run(CHECKLIST) == 0);
    assert(has(LISTING, "- function MISSING src/devtools/state_format.c:1 state_format_print -> "
                        "game/devtools/state_format.lua state_format_print\n"));
    assert(!has(LISTING, "dump_print"));
}

static void test_listing_is_sorted_by_path_then_line_with_the_file_first(void) {
    reset();
    write_file(ROOT "/src/p8/zeta.c", "int zeta(void) { return 0; }\n");
    write_file(ROOT "/src/engine/alpha.c", "\nint alpha_two(void) { return 0; }\n\n\n\n\n\n\n\nint alpha_one(void) { return 0; }\n");
    assert(run(CHECKLIST) == 0);
    assert(strcmp(read_file(LISTING), "- file MISSING src/engine/alpha.c -> game/engine/alpha.lua\n"
                                      "- function MISSING src/engine/alpha.c:2 alpha_two -> game/engine/alpha.lua alpha_two\n"
                                      "- function MISSING src/engine/alpha.c:10 alpha_one -> game/engine/alpha.lua alpha_one\n"
                                      "- file MISSING src/p8/zeta.c -> game/p8/zeta.lua\n"
                                      "- function MISSING src/p8/zeta.c:1 zeta -> game/p8/zeta.lua zeta\n") == 0);
}

static void test_usage_errors_exit_2(void) {
    reset();
    write_file(ROOT "/src/game/a.c", "int a(void) { return 0; }\n");
    assert(run("luajit tools/checklist.lua --root " ROOT " --bogus") == 2);
    assert(has(OUT, "checklist: unknown argument: --bogus\n"));
    assert(run("luajit tools/checklist.lua --root " DIR "/none") == 2);
    assert(has(OUT, "checklist: no C file under " DIR "/none/src\n"));
}

int main(void) {
    test_a_function_is_listed_missing_with_its_default_lua_mapping();
    test_signatures_across_lines_static_and_attributes_are_functions();
    test_header_inlines_map_to_the_module_lua_file();
    test_comments_strings_and_preprocessor_lines_hide_their_braces();
    test_tables_are_brace_or_string_initializers_at_file_scope();
    test_an_unparseable_top_level_brace_exits_2_with_file_and_line();
    test_each_lua_definition_form_counts_as_ported();
    test_state_format_is_scanned_and_other_devtools_are_not();
    test_listing_is_sorted_by_path_then_line_with_the_file_first();
    test_usage_errors_exit_2();
    return 0;
}
