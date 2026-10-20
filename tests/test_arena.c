/* Linked without ASan on purpose: the guard that forbids resetting an arena
 * must bite in the build where BUMP_ZONE_HALVES is 1. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine/zone.h"
#include "engine/allocator.h"
#include "heap_allocator.h"

#define ARENA_SIZE 512

static void reset_arena(void) {
    const allocator_t *arena = arena_take(&heap_allocator, "test-arena", ARENA_SIZE);
    bump_zone_reset(arena->bump);
}

int main(void) {
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        reset_arena();
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
        fprintf(stderr, "test_arena: resetting an arena must stop on an assert\n");
        return 1;
    }
    printf("test_arena: all tests passed\n");
    return 0;
}
