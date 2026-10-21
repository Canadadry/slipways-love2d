#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_fake_love_guard_work"
#define OBJ DIR "/obj"
#define FAKE DIR "/fake"
#define OUT DIR "/out.txt"
#define CLEAN_OBJECT OBJ "/" FAKE "/pad.o"
#define CLEAN_LINK DIR "/fake_love " CLEAN_OBJECT
#define GUARD "sh tools/fake_love_guard.sh " OBJ

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
static int run_guard(const char *arguments) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd, GUARD " %s > " OUT " 2>&1; echo $? > " DIR "/code", arguments);
    assert(system(cmd) == 0);
    return atoi(read_file(DIR "/code"));
}

/* The objects are empty files: the guard reads paths and dependency files,
 * never the machine code. */
static void setup(void) {
    assert(system("rm -rf " DIR " && mkdir -p " FAKE " " OBJ "/" FAKE " " OBJ "/src/p8") == 0);
    write_file(FAKE "/pad.h", "int pad_held(int mask);\n");
    write_file(FAKE "/pad.c", "#include <stdio.h>\n#include \"pad.h\"\nint pad_held(int mask) { return mask; }\n");
    write_file(CLEAN_OBJECT, "");
    write_file(OBJ "/" FAKE "/pad.d", CLEAN_OBJECT ": " FAKE "/pad.c " FAKE "/pad.h \\\n /usr/include/stdio.h\n" FAKE
                                      "/pad.h:\n/usr/include/stdio.h:\n");
}

static void test_clean_set_passes(void) {
    setup();
    assert(run_guard(CLEAN_LINK " -- " FAKE) == 0);
    assert(strstr(read_file(OUT), "fake-love-guard objects 1 sources 2 failing 0\n") != NULL);
}

static void test_object_from_src_fails_naming_binary_and_object(void) {
    setup();
    write_file(OBJ "/src/p8/palette.o", "");
    write_file(OBJ "/src/p8/palette.d", OBJ "/src/p8/palette.o: src/p8/palette.c\n");
    assert(run_guard(CLEAN_LINK " " DIR "/test_fake_love " OBJ "/src/p8/palette.o -- " FAKE) == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, "fake_love_guard: " DIR "/test_fake_love links " OBJ "/src/p8/palette.o, built from src/\n") !=
           NULL);
    assert(strstr(out, "fake_love_guard: " DIR "/fake_love") == NULL);
    assert(strstr(out, "fake-love-guard objects 2 sources 2 failing 1\n") != NULL);
}

static void test_dependency_on_a_src_header_fails_naming_the_object(void) {
    setup();
    write_file(OBJ "/" FAKE "/pad.d", CLEAN_OBJECT ": " FAKE "/pad.c src/p8/palette.h \\\n " FAKE
                                      "/../../../src/game/planet.h\nsrc/p8/palette.h:\n");
    assert(run_guard(CLEAN_LINK " -- " FAKE) == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, "fake_love_guard: " CLEAN_OBJECT " includes src/p8/palette.h\n") != NULL);
    assert(strstr(out, "fake_love_guard: " CLEAN_OBJECT " includes " FAKE "/../../../src/game/planet.h\n") != NULL);
    assert(strstr(out, "fake-love-guard objects 1 sources 2 failing 2\n") != NULL);
}

/* Includes are caught even in a file no object was built from. */
static void test_source_including_from_src_fails_naming_the_file(void) {
    setup();
    write_file(FAKE "/canvas.c", "#include \"../tools/fake_love/pad.h\"\n"
                                 "  #  include \"game/planet.h\"\n"
                                 "#include <p8/palette.h>\n"
                                 "#include \"../../src/engine/zone.h\"\n"
                                 "/* #include \"gameplay.h\" */\n");
    assert(run_guard(CLEAN_LINK " -- " FAKE) == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, "fake_love_guard: " FAKE "/canvas.c:2 includes game/planet.h\n") != NULL);
    assert(strstr(out, "fake_love_guard: " FAKE "/canvas.c:3 includes p8/palette.h\n") != NULL);
    assert(strstr(out, "fake_love_guard: " FAKE "/canvas.c:4 includes ../../src/engine/zone.h\n") != NULL);
    assert(strstr(out, "fake-love-guard objects 1 sources 3 failing 3\n") != NULL);
}

static void test_guard_over_nothing_is_an_error_not_a_pass(void) {
    setup();
    assert(run_guard("-- " FAKE) == 2);
    assert(run_guard(CLEAN_LINK " --") == 2);
    assert(run_guard(CLEAN_LINK " -- " OBJ "/src") == 2);
    assert(run_guard(CLEAN_LINK) == 2);
    assert(run_guard(CLEAN_OBJECT " -- " FAKE) == 2);
    assert(system("rm " OBJ "/" FAKE "/pad.d") == 0);
    assert(run_guard(CLEAN_LINK " -- " FAKE) == 2);
    assert(strstr(read_file(OUT), "fake_love_guard: dependency file not found: " OBJ "/" FAKE "/pad.d\n") != NULL);
}

int main(void) {
    test_clean_set_passes();
    test_object_from_src_fails_naming_binary_and_object();
    test_dependency_on_a_src_header_fails_naming_the_object();
    test_source_including_from_src_fails_naming_the_file();
    test_guard_over_nothing_is_an_error_not_a_pass();
    assert(system("rm -rf " DIR) == 0);
    printf("test_fake_love_guard: all tests passed\n");
    return 0;
}
