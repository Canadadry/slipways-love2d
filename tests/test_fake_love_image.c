#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <lauxlib.h>
#include <lualib.h>

#include "../tools/fake_love/image.h"
#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_image_capture.bin"
#define STATE "tmp/test_fake_love_image_state.txt"
#define STDERR_LOG "tmp/test_fake_love_image_stderr.txt"

static void test_image_data_values_and_misuse_are_logged_as_measured_in_love(void) {
    int status = fake_love_cli_run("", FIXTURES "image_data_log " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                   CAPTURE, STATE, STDERR_LOG);
    assert(status == 0 && "the image data log game must run");
    fake_love_cli_assert_text(
        STATE,
        "conf image nil\n"
        "image newImageData\n"
        "type userdata\n"
        "methods getPixel setPixel\n"
        "fresh 0 0 0 0\n"
        "count 4\n"
        "set 0.0039215686274509803 0.50196078431372548 0.011764705882352941 "
        "0.30196078431372547\n"
        "exact true\n"
        "clamped 1 0 1 1\n"
        "floored 0.20000000000000001 0.40000000000000002 0.59999999999999998 0\n"
        "half up in double 0 0 0 1\n"
        "fractional read 0.0039215686274509803 0.50196078431372548 "
        "0.011764705882352941 0.30196078431372547\n"
        "copied 0.0039215686274509803 0.50196078431372548 0.011764705882352941 "
        "0.30196078431372547\n"
        "independent 0 0 0 0\n"
        "large ok\n"
        "collected true\n"
        "new none: love.image.newImageData: takes 2 numbers, width and height, not 0 arguments\n"
        "new one: love.image.newImageData: takes 2 numbers, width and height, not 1 arguments\n"
        "new format: love.image.newImageData: takes 2 numbers, width and height, not 3 arguments\n"
        "new string: love.image.newImageData: takes 2 numbers, width and height, or 1 file name, not 2 arguments\n"
        "new file: love.image.newImageData: Could not open file sprites.png. Does not exist.\n"
        "new zero: love.image.newImageData: size 0x3 is not two integers from 1 to 1024\n"
        "new negative: love.image.newImageData: size 3x-1 is not two integers from 1 to 1024\n"
        "new fractional: love.image.newImageData: size 1.5x2 is not two integers from 1 to 1024\n"
        "new wide: love.image.newImageData: size 1025x1 is not two integers from 1 to 1024\n"
        "new tall: love.image.newImageData: size 1x1025 is not two integers from 1 to 1024\n"
        "new nan: love.image.newImageData: size nanx1 is not two integers from 1 to 1024\n"
        "set dot: ImageData:setPixel: argument self is a number, not an ImageData (call it with a colon)\n"
        "set four: ImageData:setPixel: takes x, y and 3 or 4 components, not 4 arguments\n"
        "set seven: ImageData:setPixel: takes x, y and 3 or 4 components, not 7 arguments\n"
        "set table: ImageData:setPixel: argument 3 is a table, not a number (the table form is not supported)\n"
        "set string component: ImageData:setPixel: argument 3 is a string, not a number\n"
        "set string coordinate: ImageData:setPixel: argument 1 is a string, not a number\n"
        "set nil: ImageData:setPixel: argument 4 is a nil, not a number\n"
        "set boolean: ImageData:setPixel: argument 5 is a boolean, not a number\n"
        "set nan: ImageData:setPixel: argument 3 is not finite\n"
        "set infinity: ImageData:setPixel: argument 4 is not finite\n"
        "set nan alpha: ImageData:setPixel: argument 6 is not finite\n"
        "set width: ImageData:setPixel: out-of-range pixel (4, 0) on a 4x3 ImageData\n"
        "set height: ImageData:setPixel: out-of-range pixel (0, 3) on a 4x3 ImageData\n"
        "set negative: ImageData:setPixel: out-of-range pixel (-1, 0) on a 4x3 ImageData\n"
        "set negative half: ImageData:setPixel: out-of-range pixel (-0.5, 0) on a 4x3 ImageData\n"
        "set nan x: ImageData:setPixel: out-of-range pixel (nan, 0) on a 4x3 ImageData\n"
        "set infinite y: ImageData:setPixel: out-of-range pixel (0, inf) on a 4x3 ImageData\n"
        "get dot: ImageData:getPixel: argument self is a number, not an ImageData (call it with a colon)\n"
        "get one: ImageData:getPixel: takes x and y, not 1 arguments\n"
        "get three: ImageData:getPixel: takes x and y, not 3 arguments\n"
        "get string: ImageData:getPixel: argument 1 is a string, not a number\n"
        "get width: ImageData:getPixel: out-of-range pixel (4, 0) on a 4x3 ImageData\n"
        "get negative half: ImageData:getPixel: out-of-range pixel (0, -0.5) on a 4x3 ImageData\n"
        "get nan: ImageData:getPixel: out-of-range pixel (nan, 0) on a 4x3 ImageData\n"
        "after refusals 0.0039215686274509803 0.50196078431372548 0.011764705882352941 "
        "0.30196078431372547\n"
        "draw 0.0039215686274509803 0.50196078431372548 0.011764705882352941 "
        "0.30196078431372547\n"
        "draw 0.0039215686274509803 0.50196078431372548 0.011764705882352941 "
        "0.30196078431372547\n"
        "draw 0.0039215686274509803 0.50196078431372548 0.011764705882352941 "
        "0.30196078431372547\n");
}

static void test_png_files_load_through_the_disk_as_measured_in_love(void) {
    int status = fake_love_cli_run("", FIXTURES "image_files_log " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                   CAPTURE, STATE, STDERR_LOG);
    assert(status == 0 && "the image files log game must run");
    fake_love_cli_assert_text(STATE,
        "conf image nil\n"
        "image newImageData\n"
        "rgb (0, 0) 1 0.50196078431372548 0 1\n"
        "rgb (1, 0) 0.0039215686274509803 0.0078431372549019607 0.011764705882352941 1\n"
        "rgb (0, 1) 0.78431372549019607 0.39215686274509803 0.19607843137254902 1\n"
        "rgb (1, 1) 0 0 0 1\n"
        "rgba (0, 0) 1 0 0 0\n"
        "rgba (1, 0) 0 1 0 0.50196078431372548\n"
        "rgba (2, 0) 0 0 1 1\n"
        "rgba (0, 1) 0.039215686274509803 0.078431372549019607 0.11764705882352941 1\n"
        "rgba (1, 1) 0.15686274509803921 0.19607843137254902 0.23529411764705882 "
        "0.50196078431372548\n"
        "rgba (2, 1) 0.27450980392156865 0.31372549019607843 0.35294117647058826 0\n"
        "tile (0, 0) 0.066666666666666666 0.13333333333333333 0.20000000000000001 1\n"
        "tile (1, 0) 0.26666666666666666 0.33333333333333331 0.40000000000000002 1\n"
        "edited 0 0 1 0\n"
        "reloaded 1 0.50196078431372548 0 1\n"
        "image of rgb true\n"
        "image of rgba: love.graphics.newImage: pixel (1, 0) has alpha 128, not 0 or 255 (translucent texels are "
        "not modelled)\n"
        "none: love.image.newImageData: takes 2 numbers, width and height, not 0 arguments\n"
        "missing: love.image.newImageData: Could not open file missing.png. Does not exist.\n"
        "directory: love.image.newImageData: Could not open file art. Does not exist.\n"
        "conf: love.image.newImageData: \"conf.lua\" is not a PNG the fake decodes as LÖVE 11.5 does: "
        "not a PNG signature\n"
        "text: love.image.newImageData: \"text.png\" is not a PNG the fake decodes as LÖVE 11.5 does: "
        "not a PNG signature\n"
        "empty: love.image.newImageData: invalid path \"\": empty path\n"
        "rooted: love.image.newImageData: invalid path \"/rgb.png\": absolute path\n"
        "parent: love.image.newImageData: invalid path \"../rgb.png\": '.' or '..' path element\n"
        "backslash: love.image.newImageData: invalid path \"a\\b\": backslash or colon in path\n"
        "table: love.image.newImageData: argument 1 is a table, not a file name\n"
        "boolean: love.image.newImageData: argument 1 is a boolean, not a file name\n"
        "number: love.image.newImageData: takes 2 numbers, width and height, not 1 arguments\n"
        "format: love.image.newImageData: takes 2 numbers, width and height, or 1 file name, not 2 arguments\n"
        "trailing nil: love.image.newImageData: takes 2 numbers, width and height, or 1 file name, not 2 "
        "arguments\n"
        "shadow (0, 0) 1 0.50196078431372548 0 1\n"
        "shadow (1, 0) 0.0039215686274509803 0.0078431372549019607 0.011764705882352941 1\n"
        "shadow (0, 1) 0.78431372549019607 0.39215686274509803 0.19607843137254902 1\n"
        "shadow (1, 1) 0 0 0 1\n");
}

static void test_an_uncaught_out_of_range_pixel_is_fatal_and_names_set_pixel_and_the_frame(void) {
    const char *const needles[] = {"love.draw f0: ", "main.lua:8: ImageData:setPixel: ", "out-of-range pixel (160, 0)",
                                   NULL};
    fake_love_cli_assert_failure("", FIXTURES "image_data_uncaught " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
}

static void test_the_accessor_gives_an_image_data_size_and_bytes_and_null_for_anything_else(void) {
    lua_State *lua = luaL_newstate();
    assert(lua != NULL);
    luaL_openlibs(lua);
    lua_newtable(lua);
    lua_setglobal(lua, "love");
    disk_t *disk = disk_create(NULL);
    assert(disk != NULL);
    image_install(lua, disk);
    int status = luaL_dostring(lua, "local data = love.image.newImageData(3, 2)\n"
                                    "data:setPixel(2, 1, 1, 0.5, 0, 0)\n"
                                    "return data, 7, io.stdout");
    assert(status == 0 && "the chunk must run");
    const image_data_t *data = image_find_data(lua, -3);
    assert(data != NULL && "an ImageData must be found");
    assert(data->width == 3 && data->height == 2);
    static const unsigned char set[4] = {0xFF, 0x80, 0x00, 0x00};
    static const unsigned char fresh[4] = {0, 0, 0, 0};
    assert(memcmp(data->pixels + 4 * (1 * 3 + 2), set, 4) == 0 && "row-major RGBA8, as setPixel stored it");
    assert(memcmp(data->pixels, fresh, 4) == 0);
    assert(image_find_data(lua, -2) == NULL && "a number is not an ImageData");
    assert(image_find_data(lua, -1) == NULL && "another userdata is not an ImageData");
    lua_close(lua);
    disk_free(disk);
}

static void test_an_uncaught_missing_file_is_fatal_and_names_love_load_and_the_path(void) {
    const char *const needles[] = {"love.load: ", "love.image.newImageData: ", "missing.png", NULL};
    fake_love_cli_assert_failure("", FIXTURES "image_files_uncaught " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
}

int main(void) {
    test_the_accessor_gives_an_image_data_size_and_bytes_and_null_for_anything_else();
    test_image_data_values_and_misuse_are_logged_as_measured_in_love();
    test_an_uncaught_out_of_range_pixel_is_fatal_and_names_set_pixel_and_the_frame();
    test_png_files_load_through_the_disk_as_measured_in_love();
    test_an_uncaught_missing_file_is_fatal_and_names_love_load_and_the_path();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_image: all tests passed\n");
    return 0;
}
