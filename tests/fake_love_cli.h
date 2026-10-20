#ifndef TESTS_FAKE_LOVE_CLI_H
#define TESTS_FAKE_LOVE_CLI_H

#include <stdbool.h>

#define FAKE_LOVE_CLI_FIXTURES "tests/fixtures/fake_love/"

/* Removes `capture` and `state`, runs `<environment> tmp/fake_love <args>`
 * with stderr into `stderr_log`, and returns its exit status. */
int fake_love_cli_run(const char *environment, const char *args, const char *capture, const char *state,
                      const char *stderr_log);

/* Allocated buffer, NUL-terminated, freed by the caller; NULL when the file
 * does not exist. `size_out` may be NULL. */
char *fake_love_cli_read_file(const char *path, long *size_out);

bool fake_love_cli_file_exists(const char *path);

#endif
