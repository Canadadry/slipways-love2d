/* tools/gen_quarter_wave.c, the body of `make sin-table`: its output must be
 * the committed header byte for byte, or the next regeneration rewrites it. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GENERATED "tmp/test_gen_quarter_wave_out.h"
#define COMMITTED "src/p8/quarter_wave.h"

/* Caller frees the result; *len receives the byte count. */
static char *read_file(const char *path, size_t *len) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);
    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    *len = fread(buf, 1, (size_t)size, file);
    fclose(file);
    buf[*len] = '\0';
    return buf;
}

static void test_generator_output_is_the_committed_header(void) {
    assert(system("./tmp/gen_quarter_wave > " GENERATED) == 0);
    size_t generated_len;
    size_t committed_len;
    char *generated = read_file(GENERATED, &generated_len);
    char *committed = read_file(COMMITTED, &committed_len);
    if (generated_len != committed_len || memcmp(generated, committed, committed_len) != 0) {
        fprintf(stderr, "make sin-table would change " COMMITTED ": diff " GENERATED " " COMMITTED "\n");
        assert(0 && "generator output differs from the committed header");
    }
    free(generated);
    free(committed);
}

int main(void) {
    test_generator_output_is_the_committed_header();
    printf("test_gen_quarter_wave: ok\n");
    return 0;
}
