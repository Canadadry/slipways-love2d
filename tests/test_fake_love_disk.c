#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/disk.h"

static void test_an_empty_disk_has_no_save(void) {
    disk_t *disk = disk_create(NULL);
    size_t length = 99;
    assert(disk_read(disk, "save.bin", &length) == NULL);
    size_t size = 99;
    assert(disk_info(disk, "save.bin", &size) == DISK_MISSING);
    disk_free(disk);
}

static void assert_reads(const disk_t *disk, const char *name, const void *expected, size_t expected_length) {
    size_t length = 0;
    unsigned char *bytes = disk_read(disk, name, &length);
    assert(bytes != NULL);
    assert(length == expected_length);
    assert(memcmp(bytes, expected, length) == 0);
    free(bytes);
    size_t size = 0;
    assert(disk_info(disk, name, &size) == DISK_FILE);
    assert(size == expected_length);
}

static void test_a_put_file_reads_back_its_binary_bytes(void) {
    disk_t *disk = disk_create(NULL);
    static const unsigned char save[] = {0x53, 0x00, 0xff, 0x10, 0x00};
    assert(disk_put(disk, "save.bin", save, sizeof(save)) == NULL);
    assert_reads(disk, "save.bin", save, sizeof(save));
    disk_free(disk);
}

static void test_a_shorter_put_replaces_the_whole_file(void) {
    disk_t *disk = disk_create(NULL);
    assert(disk_put(disk, "save.bin", "abcdef", 6) == NULL);
    assert(disk_put(disk, "save.bin", "xy", 2) == NULL);
    assert_reads(disk, "save.bin", "xy", 2);
    disk_free(disk);
}

static void test_an_empty_put_gives_an_empty_file(void) {
    disk_t *disk = disk_create(NULL);
    assert(disk_put(disk, "note.txt", "", 0) == NULL);
    assert_reads(disk, "note.txt", "", 0);
    assert(disk_put(disk, "save.bin", "abc", 3) == NULL);
    assert(disk_put(disk, "save.bin", NULL, 0) == NULL);
    assert_reads(disk, "save.bin", "", 0);
    disk_free(disk);
}

static void test_files_of_different_names_stay_apart(void) {
    disk_t *disk = disk_create(NULL);
    assert(disk_put(disk, "save.bin", "s", 1) == NULL);
    assert(disk_put(disk, "bindings.cfg", "bb", 2) == NULL);
    assert(disk_put(disk, "Save.bin", "SSS", 3) == NULL);
    assert_reads(disk, "save.bin", "s", 1);
    assert_reads(disk, "bindings.cfg", "bb", 2);
    assert_reads(disk, "Save.bin", "SSS", 3);
    disk_free(disk);
}

static void test_plain_relative_names_are_valid(void) {
    const char *valid[] = {"save.bin", "dir/x.png", "a/b/c", ".hidden", "x..y", "...", "a.b/c.d"};
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        assert(disk_check_name(valid[i]) == NULL);
    }
}

static void test_names_that_could_leave_the_disk_are_invalid(void) {
    const char *invalid[] = {"", "/x", "./x", "a/../b", "a//b", "a\\b", "c:x", "..", ".", "x/", "a/.", "x/.."};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        const char *reason = disk_check_name(invalid[i]);
        assert(reason != NULL);
        assert(reason[0] != '\0');
    }
}

static void test_a_put_inside_a_directory_is_refused(void) {
    disk_t *disk = disk_create(NULL);
    const char *reason = disk_put(disk, "saves/a.bin", "x", 1);
    assert(reason != NULL && reason[0] != '\0');
    size_t size = 0;
    assert(disk_info(disk, "saves/a.bin", &size) == DISK_MISSING);
    assert(disk_info(disk, "saves", &size) == DISK_MISSING);
    disk_free(disk);
}

#define GAME_DIRECTORY "tests/fixtures/fake_love/disk_tree"
#define GAME_FILE_BYTES "game bytes\n"

static void assert_real_file_holds(const char *path, const char *expected) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    char content[64];
    size_t length = fread(content, 1, sizeof(content), file);
    fclose(file);
    assert(length == strlen(expected));
    assert(memcmp(content, expected, length) == 0);
}

static void test_the_game_directory_files_are_readable(void) {
    disk_t *disk = disk_create(GAME_DIRECTORY);
    assert_reads(disk, "game.txt", GAME_FILE_BYTES, strlen(GAME_FILE_BYTES));
    assert_reads(disk, "data/item.txt", "item\n", 5);
    size_t length = 99;
    assert(disk_read(disk, "absent.txt", &length) == NULL);
    assert(disk_read(disk, "data/absent.txt", &length) == NULL);
    size_t size = 99;
    assert(disk_info(disk, "absent.txt", &size) == DISK_MISSING);
    disk_free(disk);
}

static void test_a_game_sub_directory_is_a_directory_but_cannot_be_read(void) {
    disk_t *disk = disk_create(GAME_DIRECTORY);
    size_t size = 99;
    assert(disk_info(disk, "data", &size) == DISK_DIRECTORY);
    size_t length = 99;
    assert(disk_read(disk, "data", &length) == NULL);
    disk_free(disk);
}

static void test_a_put_shadows_the_game_file_and_leaves_it_unchanged(void) {
    disk_t *disk = disk_create(GAME_DIRECTORY);
    assert(disk_put(disk, "game.txt", "mine", 4) == NULL);
    assert_reads(disk, "game.txt", "mine", 4);
    disk_free(disk);
    assert_real_file_holds(GAME_DIRECTORY "/game.txt", GAME_FILE_BYTES);
}

static void test_a_put_never_reaches_the_game_directory(void) {
    disk_t *disk = disk_create(GAME_DIRECTORY);
    assert(disk_put(disk, "note.txt", "hello", 5) == NULL);
    assert_reads(disk, "note.txt", "hello", 5);
    disk_free(disk);
    assert(fopen(GAME_DIRECTORY "/note.txt", "rb") == NULL);
    assert(fopen("note.txt", "rb") == NULL);
}

int main(void) {
    test_an_empty_disk_has_no_save();
    test_a_put_file_reads_back_its_binary_bytes();
    test_a_shorter_put_replaces_the_whole_file();
    test_an_empty_put_gives_an_empty_file();
    test_files_of_different_names_stay_apart();
    test_plain_relative_names_are_valid();
    test_names_that_could_leave_the_disk_are_invalid();
    test_a_put_inside_a_directory_is_refused();
    test_the_game_directory_files_are_readable();
    test_a_game_sub_directory_is_a_directory_but_cannot_be_read();
    test_a_put_shadows_the_game_file_and_leaves_it_unchanged();
    test_a_put_never_reaches_the_game_directory();
    printf("test_fake_love_disk: ok\n");
    return 0;
}
