#include "fake_love_cli.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int fake_love_cli_run(const char *environment, const char *args, const char *capture, const char *state,
                      const char *stderr_log) {
    char cmd[1024];
    remove(capture);
    remove(state);
    int length = snprintf(cmd, sizeof cmd, "%s ./tmp/fake_love %s > /dev/null 2> %s", environment, args, stderr_log);
    assert(length > 0 && (size_t)length < sizeof cmd && "the command must fit");
    int status = system(cmd);
    assert(status != -1 && WIFEXITED(status));
    return WEXITSTATUS(status);
}

char *fake_love_cli_read_file(const char *path, long *size_out) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return NULL;
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);
    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, file) == (size_t)size);
    buf[size] = '\0';
    fclose(file);
    if (size_out != NULL)
        *size_out = size;
    return buf;
}

bool fake_love_cli_file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    fclose(file);
    return true;
}
