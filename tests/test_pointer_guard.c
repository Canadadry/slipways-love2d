#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_pointer_guard_work"
#define LIST DIR "/exceptions.txt"
#define OUT DIR "/out.txt"
#define SRC DIR "/src"
#define GUARD "sh tools/pointer_guard.sh "

/* Static buffer, NUL-terminated, overwritten by each call. */
static const char *read_file(const char *path) {
    static char buf[1 << 16];
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
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

/* The exit code of the script, read back from a file: the raw status of
 * system() cannot be decoded without <sys/wait.h>, outside C99. */
static int run_guard_on_list(const char *list, const char *sources) {
    static char cmd[1 << 13];
    snprintf(cmd, sizeof cmd, GUARD "%s %s > " OUT " 2>&1; echo $? > " DIR "/code", list, sources);
    assert(system(cmd) == 0);
    return atoi(read_file(DIR "/code"));
}

static int run_guard(const char *sources) {
    return run_guard_on_list(LIST, sources);
}

/* Each guard run costs a few processes, so the counting cases share two
 * runs: one with every count listed exactly, one with every count one too
 * high. */
#define MAX_CASES 32

static struct {
    char path[128];
    int pointers;
    int lists;
    int arrays;
} count_cases[MAX_CASES];
static int count_case_total;

static void add_count_case_with_arrays(const char *name, const char *text, int pointers, int lists, int arrays) {
    assert(count_case_total < MAX_CASES);
    snprintf(count_cases[count_case_total].path, sizeof count_cases[0].path, SRC "/%s", name);
    write_file(count_cases[count_case_total].path, text);
    count_cases[count_case_total].pointers = pointers;
    count_cases[count_case_total].lists = lists;
    count_cases[count_case_total].arrays = arrays;
    count_case_total++;
}

static void add_count_case(const char *name, const char *text, int pointers, int lists) {
    add_count_case_with_arrays(name, text, pointers, lists, 0);
}

static void write_count_list(int excess) {
    FILE *file = fopen(LIST, "w");
    assert(file != NULL);
    for (int i = 0; i < count_case_total; i++) {
        if (count_cases[i].pointers + excess > 0)
            fprintf(file, "%s %d\n", count_cases[i].path, count_cases[i].pointers + excess);
        if (count_cases[i].lists + excess > 0)
            fprintf(file, "entity-lists %s %d\n", count_cases[i].path, count_cases[i].lists + excess);
        if (count_cases[i].arrays + excess > 0)
            fprintf(file, "entity-arrays %s %d\n", count_cases[i].path, count_cases[i].arrays + excess);
    }
    fclose(file);
}

static void assert_line(const char *out, const char *verb, const char *prefix, const char *path, int count) {
    char expected[256];
    snprintf(expected, sizeof expected, "line to %s: %s%s %d\n", verb, prefix, path, count);
    if (strstr(out, expected) == NULL) {
        fprintf(stderr, "test_pointer_guard: missing \"%s\" in:\n%s", expected, out);
        assert(0);
    }
}

static void check_count_cases(void) {
    static char sources[1 << 12];
    sources[0] = '\0';
    for (int i = 0; i < count_case_total; i++) {
        strcat(sources, " ");
        strcat(sources, count_cases[i].path);
    }
    write_count_list(0);
    if (run_guard(sources) != 0) {
        fprintf(stderr, "test_pointer_guard: counts differ:\n%s", read_file(OUT));
        assert(0);
    }
    write_count_list(1);
    assert(run_guard(sources) == 1);
    const char *out = read_file(OUT);
    for (int i = 0; i < count_case_total; i++) {
        int pointers = count_cases[i].pointers, lists = count_cases[i].lists, arrays = count_cases[i].arrays;
        assert_line(out, pointers > 0 ? "write" : "remove", "", count_cases[i].path, pointers > 0 ? pointers : 1);
        assert_line(out, lists > 0 ? "write" : "remove", "entity-lists ", count_cases[i].path, lists > 0 ? lists : 1);
        assert_line(out, arrays > 0 ? "write" : "remove", "entity-arrays ", count_cases[i].path, arrays > 0 ? arrays : 1);
    }
}

static void case_plain_pointer_field_is_counted(void) {
    add_count_case("plain.c", "typedef struct {\n    int x;\n    entity_t *held;\n} down_t;\n", 1, 0);
}

#define CMP DIR "/cmp"
#define HELD CMP "/held.h"
#define CLEAN CMP "/clean.c"
#define LISTS CMP "/lists.h"
#define EMPTY CMP "/empty.c"
#define CMP_SOURCES HELD " " CLEAN " " LISTS " " EMPTY

static void write_comparison_sources(void) {
    write_file(HELD, "typedef struct {\n    entity_t *held;\n    const char *name;\n} down_t;\n");
    write_file(CLEAN, "typedef struct {\n    int x;\n} plain_t;\n");
    write_file(LISTS, "typedef struct {\n    entity_list_t *a;\n    entity_list_t b;\n} l_t;\n");
    write_file(EMPTY, "");
}

static void test_exact_counts_pass_and_print_the_summary(void) {
    write_file(LIST, "# comment\n" HELD " 2\n" LISTS " 1  # trailing comment\nentity-lists " LISTS " 2\n");
    assert(run_guard(CMP_SOURCES) == 0);
    assert(strcmp(read_file(OUT), "pointer-guard files 4 pointers 3 entity-lists 2 entity-arrays 0\n") == 0);
}

static void test_every_difference_fails_with_its_line(void) {
    write_file(LIST, LISTS " 2\n" CLEAN " 1\n" EMPTY " 6\nentity-lists " LISTS " 1\nentity-lists " HELD " 3\n" CMP "/gone.c 4\n"
                     "entity-lists " CMP "/gone.c 5\n");
    assert(run_guard(CMP_SOURCES) == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, "pointer_guard: if intended, edit " LIST " by hand\n") != NULL);
    assert_line(out, "add", "", HELD, 2);
    assert_line(out, "write", "", LISTS, 1);
    assert_line(out, "remove", "", CLEAN, 1);
    assert_line(out, "remove", "", EMPTY, 6);
    assert_line(out, "write", "entity-lists ", LISTS, 2);
    assert_line(out, "remove", "entity-lists ", HELD, 3);
    assert(strstr(out, "not among the files given; line to remove: " CMP "/gone.c 4\n") != NULL);
    assert(strstr(out, "not among the files given; line to remove: entity-lists " CMP "/gone.c 5\n") != NULL);
    assert(strstr(out, "clean.c has") == NULL);
}

static void test_listed_count_below_actual_fails(void) {
    write_file(LIST, HELD " 1\n" LISTS " 1\nentity-lists " LISTS " 2\n");
    assert(run_guard(CMP_SOURCES) == 1);
    assert_line(read_file(OUT), "write", "", HELD, 2);
}

#define ARRAYS CMP "/arrays.c"

static void test_entity_arrays_listed_below_actual_or_missing_fail(void) {
    write_file(ARRAYS, "void run(void) {\n    entity_t *storage[8];\n    entity_t *others[8];\n}\n");
    write_file(LIST, "entity-arrays " ARRAYS " 1\n");
    assert(run_guard(ARRAYS) == 1);
    assert_line(read_file(OUT), "write", "entity-arrays ", ARRAYS, 2);
    write_file(LIST, "");
    assert(run_guard(ARRAYS) == 1);
    assert_line(read_file(OUT), "add", "entity-arrays ", ARRAYS, 2);
}

static void test_no_source_file_is_an_error_not_a_pass(void) {
    write_file(LIST, "");
    assert(run_guard("") == 2);
}

static void test_unreadable_list_is_an_error_not_a_pass(void) {
    assert(run_guard_on_list(DIR "/missing.txt", HELD) == 2);
}

/* `entity_list_t *` is both a pointer and a list. */
static void case_entity_list_fields_are_counted_by_value_by_pointer_and_as_array(void) {
    add_count_case("lists.h",
                  "typedef struct {\n    entity_list_t members;\n    entity_list_t *owned;\n"
                  "    entity_list_t layers[4];\n    int count;\n} group_t;\n",
                  1, 3);
}

static void case_const_pointer_field_is_counted(void) {
    add_count_case("const.h", "typedef struct {\n    const char *const name;\n    const int *row;\n} t_t;\n", 2, 0);
}

static void case_pointer_to_pointer_counts_once(void) {
    add_count_case("double.h", "typedef struct {\n    const vec_t *const *pos;\n    int **grid;\n} t_t;\n", 2, 0);
}

static void case_array_of_pointers_counts_once(void) {
    add_count_case("array.h", "struct table {\n    const char *names[PERIOD_COUNT];\n    int *cells[2][3];\n};\n", 2,
                  0);
}

static void case_comma_declarators_count_one_each(void) {
    add_count_case("comma.h", "typedef struct {\n    int *a, *b, c;\n    char d, *e;\n} t_t;\n", 3, 0);
}

static void case_fields_of_nested_structs_and_unions_are_counted(void) {
    add_count_case("nested.h",
                  "typedef struct {\n    struct {\n        int *inner;\n    } box;\n"
                  "    union {\n        char *text;\n        int value;\n    } u;\n"
                  "    struct link {\n        struct link *next;\n    } *chain;\n} outer_t;\n",
                  4, 0);
}

static void case_fields_of_a_struct_defined_inside_a_function_are_counted(void) {
    add_count_case("local_struct.c",
                  "void run(int *param) {\n    int *local = param;\n"
                  "    struct pair {\n        int *left;\n        int *right;\n    } pair = {local, local};\n"
                  "    if (local) {\n        *local = 1;\n    }\n}\n",
                  2, 0);
}

static void case_file_scope_static_and_global_pointers_are_counted(void) {
    add_count_case("globals.c", "static entity_t *g_selected;\nentity_t *g_hovered;\nint g_count;\n", 2, 0);
}

static void case_file_scope_pointer_with_an_initializer_is_counted(void) {
    add_count_case("initialized.c",
                  "static entity_t *g_main_mouse = NULL;\n"
                  "const char *const period_names_table[PERIOD_COUNT] = {\"JAN\", \"FEB\"};\n"
                  "static const row_t *const rows[] = {\n    &(row_t){.a = {1, 2}},\n    NULL,\n};\n"
                  "static int g_total = 3;\nstatic const int g_area = WIDTH * HEIGHT;\n",
                  3, 0);
}

static void case_file_scope_entity_lists_are_counted(void) {
    add_count_case("list_globals.c", "static entity_list_t g_all;\nentity_list_t *g_owned = NULL;\n", 1, 2);
}

/* Each left-out construct is followed by one counted field, which proves
 * that the parse survived it. */
#define KEPT "struct kept {\n    int *kept;\n};\n"

static void case_local_entity_arrays_are_counted_const_and_static_included(void) {
    add_count_case_with_arrays("local_arrays.c",
                               "void run(int n) {\n    entity_t *storage[CAP];\n    if (n) {\n"
                               "        const entity_t *seen[4], *more[2];\n        static entity_t *cache[3];\n    }\n"
                               "    if (n != 0 && n <= 2) {\n        return;\n    }\n    entity_t *after[2];\n}\n",
                               0, 0, 5);
}

static void case_local_entity_pointers_that_are_no_arrays_are_left_out(void) {
    add_count_case_with_arrays("local_pointers.c",
                               "void run(entity_list_t *list) {\n    entity_t *e = list->items[2];\n"
                               "    entity_t *f;\n    const entity_t *g = (const entity_t *)list->items[0];\n"
                               "    planet_t *rows[4];\n    entity_kind_t *kinds[2];\n}\n" KEPT,
                               1, 0, 0);
}

static void case_entity_arrays_outside_a_function_are_no_local_arrays(void) {
    add_count_case_with_arrays("scoped_arrays.c",
                               "static entity_t *g_table[4];\nstruct slots {\n    entity_t *slots[2];\n};\n"
                               "void run(entity_t *params[]);\n",
                               2, 0, 0);
}

static void case_function_pointer_fields_and_typedefs_are_left_out(void) {
    add_count_case("function_pointers.h",
                  "typedef void (*state_fn_t)(entity_t *self, p8_fixed_t t);\n"
                  "struct entity_class {\n    void (*draw)(entity_t *self, const vec_t *screen_pos);\n"
                  "    struct particle *(*spawn_particle)(entity_t *self);\n    state_fn_t states[4];\n"
                  "    void (*handlers[3])(int *x);\n};\n" KEPT,
                  1, 0);
}

static void case_prototypes_and_function_definitions_are_left_out(void) {
    add_count_case("functions.c",
                  "entity_t *planet_find(const char *id);\nstatic const vec_t *pos_of(entity_t *e, int *n);\n"
                  "static inline entity_t *first(entity_list_t *list) {\n    return list->items[0];\n}\n" KEPT,
                  1, 0);
}

static void case_locals_and_parameters_are_left_out(void) {
    add_count_case("locals.c",
                  "void run(entity_t *self, entity_list_t *all) {\n    entity_t *other = self;\n"
                  "    entity_list_t seen;\n    static int *cache;\n    for (int *p = 0; p; p++) {\n"
                  "        char *s = \"x\";\n    }\n}\n" KEPT,
                  1, 0);
}

static void case_extern_declarations_are_left_out(void) {
    add_count_case("externs.h", "extern const char *const period_names_table[12];\nextern entity_list_t g_all;\n" KEPT,
                  1, 0);
}

static void case_runtime_handles_are_left_out(void) {
    add_count_case("handles.c",
                  "static allocator_t *g_allocator;\nconst p8_graphics_t *g_graphics = NULL;\n"
                  "struct ctx {\n    p8_vram_t *vram;\n    const allocator_t *const arena;\n};\n" KEPT,
                  1, 0);
}

static void case_stars_in_comments_strings_and_preprocessor_lines_are_left_out(void) {
    add_count_case("text.c",
                  "/* entity_t *a; {\n   int *b; */\n// int *c;\n#define PTR(x) (*(x))\n"
                  "#define LONG(x) \\\n    int *d;\n"
                  "static const char g_name[] = \"int *e; {\";\nstatic const char g_brace = '{';\n"
                  "static const char g_quote = '\\'';\n" KEPT,
                  1, 0);
}

static void case_struct_variable_that_is_not_a_pointer_is_left_out(void) {
    add_count_case("rows.c",
                  "static const data_row_t row = {.name = \"x\", .next = NULL};\nstatic entity_t g_root;\n" KEPT, 1,
                  0);
}

static void case_typedefs_outside_a_struct_body_are_left_out(void) {
    add_count_case("typedefs.h",
                  "typedef struct entity_class entity_class_t;\ntypedef struct {\n    int *inside;\n} *handle_t;\n"
                  "typedef enum {\n    A,\n    B\n} *enum_ptr_t;\n" KEPT,
                  2, 0);
}

int main(void) {
    assert(system("rm -rf " DIR " && mkdir -p " SRC " " CMP) == 0);
    write_comparison_sources();
    test_exact_counts_pass_and_print_the_summary();
    test_every_difference_fails_with_its_line();
    test_listed_count_below_actual_fails();
    test_entity_arrays_listed_below_actual_or_missing_fail();
    test_no_source_file_is_an_error_not_a_pass();
    test_unreadable_list_is_an_error_not_a_pass();
    case_plain_pointer_field_is_counted();
    case_const_pointer_field_is_counted();
    case_pointer_to_pointer_counts_once();
    case_array_of_pointers_counts_once();
    case_comma_declarators_count_one_each();
    case_fields_of_nested_structs_and_unions_are_counted();
    case_fields_of_a_struct_defined_inside_a_function_are_counted();
    case_file_scope_static_and_global_pointers_are_counted();
    case_file_scope_pointer_with_an_initializer_is_counted();
    case_file_scope_entity_lists_are_counted();
    case_function_pointer_fields_and_typedefs_are_left_out();
    case_prototypes_and_function_definitions_are_left_out();
    case_locals_and_parameters_are_left_out();
    case_extern_declarations_are_left_out();
    case_runtime_handles_are_left_out();
    case_stars_in_comments_strings_and_preprocessor_lines_are_left_out();
    case_struct_variable_that_is_not_a_pointer_is_left_out();
    case_typedefs_outside_a_struct_body_are_left_out();
    case_entity_list_fields_are_counted_by_value_by_pointer_and_as_array();
    case_local_entity_arrays_are_counted_const_and_static_included();
    case_local_entity_pointers_that_are_no_arrays_are_left_out();
    case_entity_arrays_outside_a_function_are_no_local_arrays();
    check_count_cases();
    assert(system("rm -rf " DIR) == 0);
    printf("test_pointer_guard: all tests passed\n");
    return 0;
}
