#include "graphics.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>

#include "function_list.h"
#include "image.h"
#include "image_rules.h"
#include "point.h"
#include "transform.h"

#define METATABLE "Canvas"
#define IMAGE_METATABLE "Image"
#define QUAD_METATABLE "Quad"

typedef struct quad_object {
    canvas_area_t area;
    int reference_width;
    int reference_height;
} quad_object_t;

typedef struct {
    int handle;
} canvas_object_t;

static canvas_store_t *upvalue_store(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

static transform_t *upvalue_transform(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(2));
}

static const window_size_t *upvalue_window(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(3));
}

static int get_dimensions(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 0)
        return luaL_error(lua, "love.graphics.getDimensions: takes no argument, not %d", top);
    const window_size_t *window = upvalue_window(lua);
    lua_pushnumber(lua, window->width);
    lua_pushnumber(lua, window->height);
    return 2;
}

static int new_canvas(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 2)
        return luaL_error(lua, "love.graphics.newCanvas: takes 2 arguments, 160 and 120, not %d", top);
    for (int index = 1; index <= 2; index++)
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.newCanvas: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
    if (lua_tonumber(lua, 1) != CANVAS_WIDTH || lua_tonumber(lua, 2) != CANVAS_HEIGHT)
        return luaL_error(lua, "love.graphics.newCanvas: the canvas is %dx%d, not %fx%f", CANVAS_WIDTH, CANVAS_HEIGHT,
                          lua_tonumber(lua, 1), lua_tonumber(lua, 2));
    int handle = canvas_create(upvalue_store(lua));
    if (handle == CANVAS_NONE)
        return luaL_error(lua, "love.graphics.newCanvas: more than %d canvases", CANVAS_MAX_COUNT);
    canvas_object_t *object = lua_newuserdata(lua, sizeof *object);
    object->handle = handle;
    luaL_getmetatable(lua, METATABLE);
    lua_setmetatable(lua, -2);
    return 1;
}

static int set_canvas(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top > 1)
        return luaL_error(lua, "love.graphics.setCanvas: takes a canvas or nothing, not %d arguments", top);
    if (top == 0 || lua_isnil(lua, 1)) {
        canvas_select(upvalue_store(lua), CANVAS_NONE);
        return 0;
    }
    canvas_object_t *object = luaL_testudata(lua, 1, METATABLE);
    if (object == NULL)
        return luaL_error(lua, "love.graphics.setCanvas: the argument is a %s, not a canvas", luaL_typename(lua, 1));
    canvas_select(upvalue_store(lua), object->handle);
    return 0;
}

static int clear(lua_State *lua) {
    int top = lua_gettop(lua);
    for (int index = 1; index <= top; index++) {
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.clear: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
        if (!isfinite(lua_tonumber(lua, index)))
            return luaL_error(lua, "love.graphics.clear: argument %d is not a finite number", index);
    }
    if (top != 0 && top != 3 && top != 4)
        return luaL_error(lua, "love.graphics.clear: takes 0, 3 or 4 numbers, not %d arguments", top);
    unsigned char rgba[4] = {0, 0, 0, top == 3 ? 255 : 0};
    for (int index = 1; index <= top; index++)
        rgba[index - 1] = canvas_convert_clear_component(lua_tonumber(lua, index));
    canvas_clear(upvalue_store(lua), rgba);
    return 0;
}

static int set_color(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top >= 1 && lua_type(lua, 1) == LUA_TTABLE)
        return luaL_error(lua, "love.graphics.setColor: argument 1 is a table, not a number (the table form is not "
                               "supported)");
    if (top != 3 && top != 4)
        return luaL_error(lua, "love.graphics.setColor: takes 3 or 4 numbers, not %d arguments", top);
    for (int index = 1; index <= top; index++)
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.setColor: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
    for (int index = 1; index <= top; index++)
        if (!isfinite(lua_tonumber(lua, index)))
            return luaL_error(lua, "love.graphics.setColor: argument %d is not finite", index);
    unsigned char rgba[4] = {0, 0, 0, 255};
    for (int index = 1; index <= top; index++)
        rgba[index - 1] = canvas_convert_draw_component(lua_tonumber(lua, index));
    canvas_set_color(upvalue_store(lua), rgba);
    return 0;
}

/* luaL_error does not return: it raises, or the draw may proceed. */
static void refuse_translucent_on_canvas(lua_State *lua, const char *function) {
    canvas_store_t *store = upvalue_store(lua);
    if (canvas_get_target(store) == CANVAS_NONE)
        return;
    unsigned char rgba[4];
    canvas_get_color(store, rgba);
    if (rgba[3] != 0xFF)
        luaL_error(lua, "love.graphics.%s: the colour's alpha is %02X, not FF (a translucent colour is drawn only "
                        "onto the screen: blending on a canvas is not modelled)",
                   function, rgba[3]);
}

static void push_coordinate(lua_State *lua, bool table_form, int index) {
    if (table_form)
        lua_rawgeti(lua, 1, index);
    else
        lua_pushvalue(lua, index);
}

static int points(lua_State *lua) {
    int top = lua_gettop(lua);
    bool table_form = top >= 1 && lua_type(lua, 1) == LUA_TTABLE;
    if (table_form && top > 1)
        return luaL_error(lua, "love.graphics.points: a table must be the only argument, not one of %d", top);
    int count = table_form ? (int)lua_objlen(lua, 1) : top;
    const char *kind = table_form ? "element" : "argument";
    transform_offset_t offset = transform_get_offset(upvalue_transform(lua));
    for (int index = 1; index <= count; index++) {
        push_coordinate(lua, table_form, index);
        if (lua_type(lua, -1) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.points: %s %d is a %s, not a number", kind, index,
                              luaL_typename(lua, -1));
        double coordinate = lua_tonumber(lua, -1);
        lua_pop(lua, 1);
        if (!isfinite(coordinate))
            return luaL_error(lua, "love.graphics.points: %s %d is not finite", kind, index);
        if (!point_accepts_coordinate(coordinate))
            return luaL_error(lua, "love.graphics.points: %s %d is out of range (|v| >= 2^20)", kind, index);
        if (!point_accepts_translated(coordinate, index % 2 != 0 ? offset.x : offset.y))
            return luaL_error(lua, "love.graphics.points: %s %d is out of range after translate (|v| >= 2^20)", kind,
                              index);
    }
    if (count % 2 != 0)
        return luaL_error(lua, "love.graphics.points: odd number of coordinates (%d)", count);
    refuse_translucent_on_canvas(lua, "points");
    canvas_store_t *store = upvalue_store(lua);
    for (int index = 1; index <= count; index += 2) {
        push_coordinate(lua, table_form, index);
        push_coordinate(lua, table_form, index + 1);
        int x = point_find_pixel(lua_tonumber(lua, -2), offset.x, CANVAS_WIDTH);
        int y = point_find_pixel(lua_tonumber(lua, -1), offset.y, CANVAS_HEIGHT);
        lua_pop(lua, 2);
        if (x != POINT_OFF && y != POINT_OFF)
            canvas_plot(store, x, y);
    }
    return 0;
}

static int rectangle(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 5)
        return luaL_error(lua, "love.graphics.rectangle: takes 5 arguments, not %d", top);
    if (lua_type(lua, 1) != LUA_TSTRING)
        return luaL_error(lua, "love.graphics.rectangle: argument 1 is a %s, not a string", luaL_typename(lua, 1));
    const char *mode = lua_tostring(lua, 1);
    if (strcmp(mode, "fill") != 0)
        return luaL_error(lua, "love.graphics.rectangle: mode '%s' is not supported, only 'fill'", mode);
    for (int index = 2; index <= 5; index++) {
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.rectangle: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
        double value = lua_tonumber(lua, index);
        if (!isfinite(value))
            return luaL_error(lua, "love.graphics.rectangle: argument %d is not finite", index);
        if (!point_accepts_coordinate(value))
            return luaL_error(lua, "love.graphics.rectangle: argument %d is out of range (|v| >= 2^20)", index);
    }
    transform_offset_t offset = transform_get_offset(upvalue_transform(lua));
    for (int index = 2; index <= 3; index++)
        if (!point_accepts_translated(lua_tonumber(lua, index), index == 2 ? offset.x : offset.y))
            return luaL_error(lua, "love.graphics.rectangle: argument %d is out of range after translate (|v| >= 2^20)",
                              index);
    refuse_translucent_on_canvas(lua, "rectangle");
    int first_column, last_column, first_row, last_row;
    if (point_find_span(lua_tonumber(lua, 2), lua_tonumber(lua, 4), offset.x, CANVAS_WIDTH, &first_column,
                        &last_column) &&
        point_find_span(lua_tonumber(lua, 3), lua_tonumber(lua, 5), offset.y, CANVAS_HEIGHT, &first_row, &last_row))
        canvas_fill_range(upvalue_store(lua), first_column, first_row, last_column, last_row);
    return 0;
}

/* "a number", "an ImageData", "a Canvas"...: the value at `index` as the
 * refusals name it. The string may live on the Lua stack: only for an
 * error raised at once. */
static const char *describe(lua_State *lua, int index) {
    if (image_find_data(lua, index) != NULL)
        return "an ImageData";
    if (luaL_testudata(lua, index, IMAGE_METATABLE) != NULL)
        return "an Image";
    if (luaL_testudata(lua, index, QUAD_METATABLE) != NULL)
        return "a Quad";
    if (luaL_testudata(lua, index, METATABLE) != NULL)
        return "a Canvas";
    return lua_pushfstring(lua, "a %s", luaL_typename(lua, index));
}

static int new_image(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 1)
        return luaL_error(lua, "love.graphics.newImage: takes 1 argument, an ImageData, not %d arguments", top);
    const image_data_t *data = image_find_data(lua, 1);
    if (data == NULL)
        return luaL_error(lua, "love.graphics.newImage: argument 1 is %s, not an ImageData", describe(lua, 1));
    int translucent = image_rules_find_translucent_pixel(data->pixels, data->width * data->height);
    if (translucent != IMAGE_RULES_NONE)
        return luaL_error(lua, "love.graphics.newImage: pixel (%d, %d) has alpha %d, not 0 or 255 (translucent "
                               "texels are not modelled)",
                          translucent % data->width, translucent / data->width, data->pixels[4 * translucent + 3]);
    size_t size = image_data_byte_count(data->width, data->height);
    image_data_t *image = lua_newuserdata(lua, size);
    memcpy(image, data, size);
    luaL_getmetatable(lua, IMAGE_METATABLE);
    lua_setmetatable(lua, -2);
    return 1;
}

/* luaL_error does not return: it raises, or the verdict was accepted. */
static void refuse_verdict(lua_State *lua, const char *function, int argument, image_rules_verdict_t verdict,
                           const char *range) {
    if (verdict == IMAGE_RULES_NOT_FINITE)
        luaL_error(lua, "love.graphics.%s: argument %d is not finite", function, argument);
    if (verdict == IMAGE_RULES_NOT_INTEGER)
        luaL_error(lua, "love.graphics.%s: argument %d is not an integer", function, argument);
    if (verdict == IMAGE_RULES_OUT_OF_RANGE)
        luaL_error(lua, "love.graphics.%s: argument %d is out of range (%s)", function, argument, range);
}

static int new_quad(lua_State *lua) {
    static const char *const ranges[IMAGE_RULES_QUAD_VALUES] = {
        "x from 0 to sw - 1", "y from 0 to sh - 1", "w from 1 to sw - x",
        "h from 1 to sh - y", "sw from 1 to 1024", "sh from 1 to 1024",
    };
    int top = lua_gettop(lua);
    if (top != IMAGE_RULES_QUAD_VALUES)
        return luaL_error(lua, "love.graphics.newQuad: takes 6 numbers, x, y, w, h, sw and sh, not %d arguments",
                          top);
    double values[IMAGE_RULES_QUAD_VALUES];
    for (int index = 1; index <= IMAGE_RULES_QUAD_VALUES; index++) {
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.newQuad: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
        values[index - 1] = lua_tonumber(lua, index);
    }
    int refused = 0;
    image_rules_verdict_t verdict = image_rules_check_quad(values, &refused);
    refuse_verdict(lua, "newQuad", refused + 1, verdict, ranges[refused]);
    quad_object_t *quad = lua_newuserdata(lua, sizeof *quad);
    quad->area = (canvas_area_t){(int)values[0], (int)values[1], (int)values[2], (int)values[3]};
    quad->reference_width = (int)values[4];
    quad->reference_height = (int)values[5];
    luaL_getmetatable(lua, QUAD_METATABLE);
    lua_setmetatable(lua, -2);
    return 1;
}

/* draw(canvas, x, y, 0, s, s): models nothing, as every draw on the screen,
 * so it is accepted only where it cannot hide a change to a canvas or a
 * tinted, shifted, blurred or distorted window. */
static int draw_canvas(lua_State *lua) {
    if (luaL_testudata(lua, 1, METATABLE) == NULL)
        return luaL_error(lua, "love.graphics.draw: argument 1 is %s, not a Canvas", describe(lua, 1));
    for (int index = 2; index <= 6; index++)
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.draw: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
    for (int index = 2; index <= 3; index++)
        refuse_verdict(lua, "draw", index, image_rules_check_position(lua_tonumber(lua, index)), "|v| >= 2^20");
    double rotation = lua_tonumber(lua, 4);
    if (!isfinite(rotation))
        return luaL_error(lua, "love.graphics.draw: argument 4 is not finite");
    if (rotation != 0)
        return luaL_error(lua, "love.graphics.draw: argument 4 is %g, only 0 is supported (rotation is not modelled)",
                          rotation);
    for (int index = 5; index <= 6; index++)
        refuse_verdict(lua, "draw", index, image_rules_check_scale(lua_tonumber(lua, index)), "s from 1 to 16");
    if (lua_tonumber(lua, 5) != lua_tonumber(lua, 6))
        return luaL_error(lua, "love.graphics.draw: the scales %d and %d differ (unequal scales distort the window)",
                          (int)lua_tonumber(lua, 5), (int)lua_tonumber(lua, 6));
    canvas_store_t *store = upvalue_store(lua);
    if (canvas_get_target(store) != CANVAS_NONE)
        return luaL_error(lua, "love.graphics.draw: a canvas is selected, a Canvas is drawn only onto the screen "
                               "(draws between canvases are not modelled)");
    unsigned char rgba[4];
    canvas_get_color(store, rgba);
    if (rgba[0] != 0xFF || rgba[1] != 0xFF || rgba[2] != 0xFF || rgba[3] != 0xFF)
        return luaL_error(lua, "love.graphics.draw: the colour is %02X%02X%02X%02X, not white FFFFFFFF (it would tint "
                               "the window)",
                          rgba[0], rgba[1], rgba[2], rgba[3]);
    transform_offset_t offset = transform_get_offset(upvalue_transform(lua));
    if (offset.x != 0 || offset.y != 0)
        return luaL_error(lua, "love.graphics.draw: the offset is (%d, %d), not (0, 0) (it would shift the window)",
                          offset.x, offset.y);
    return 0;
}

static int draw(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top == 6)
        return draw_canvas(lua);
    if (top != 4)
        return luaL_error(lua, "love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, or 6, a Canvas, x, "
                               "y, 0, s and s, not %d",
                          top);
    const image_data_t *image = luaL_testudata(lua, 1, IMAGE_METATABLE);
    if (image == NULL)
        return luaL_error(lua, "love.graphics.draw: argument 1 is %s, not an Image", describe(lua, 1));
    const quad_object_t *quad = luaL_testudata(lua, 2, QUAD_METATABLE);
    if (quad == NULL)
        return luaL_error(lua, "love.graphics.draw: argument 2 is %s, not a Quad", describe(lua, 2));
    for (int index = 3; index <= 4; index++) {
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.draw: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
        refuse_verdict(lua, "draw", index, image_rules_check_position(lua_tonumber(lua, index)), "|v| >= 2^20");
    }
    transform_offset_t offset = transform_get_offset(upvalue_transform(lua));
    int x = (int)lua_tonumber(lua, 3) + offset.x;
    int y = (int)lua_tonumber(lua, 4) + offset.y;
    for (int index = 3; index <= 4; index++)
        if (image_rules_check_position(index == 3 ? x : y) != IMAGE_RULES_ACCEPTED)
            return luaL_error(lua, "love.graphics.draw: argument %d is out of range after translate (|v| >= 2^20)",
                              index);
    if (quad->reference_width != image->width || quad->reference_height != image->height)
        return luaL_error(lua, "love.graphics.draw: the quad's reference size %dx%d differs from the image's %dx%d",
                          quad->reference_width, quad->reference_height, image->width, image->height);
    refuse_translucent_on_canvas(lua, "draw");
    canvas_source_t source = {image->pixels, image->width, image->height};
    canvas_draw_texels(upvalue_store(lua), &source, &quad->area, x, y);
    return 0;
}

static int push(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 0)
        return luaL_error(lua, "love.graphics.push: takes no arguments, not %d (push(\"all\") and "
                               "push(\"transform\") are not supported)",
                          top);
    if (!transform_push(upvalue_transform(lua)))
        return luaL_error(lua, "love.graphics.push: Maximum stack depth reached (more pushes than pops?)");
    return 0;
}

static int pop(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 0)
        return luaL_error(lua, "love.graphics.pop: takes no arguments, not %d", top);
    if (!transform_pop(upvalue_transform(lua)))
        return luaL_error(lua, "love.graphics.pop: Minimum stack depth reached (more pops than pushes?)");
    return 0;
}

static int translate(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 2)
        return luaL_error(lua, "love.graphics.translate: takes 2 numbers, dx and dy, not %d arguments", top);
    for (int index = 1; index <= 2; index++) {
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.translate: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
        refuse_verdict(lua, "translate", index, image_rules_check_position(lua_tonumber(lua, index)), "|v| >= 2^20");
    }
    transform_t *transform = upvalue_transform(lua);
    int dx = (int)lua_tonumber(lua, 1);
    int dy = (int)lua_tonumber(lua, 2);
    if (!transform_translate(transform, dx, dy)) {
        transform_offset_t offset = transform_get_offset(transform);
        bool on_x = abs(offset.x + dx) >= TRANSFORM_OFFSET_LIMIT;
        return luaL_error(lua, "love.graphics.translate: the offset would reach 2^20 (%s: %d)", on_x ? "x" : "y",
                          on_x ? offset.x + dx : offset.y + dy);
    }
    return 0;
}

static int set_default_filter(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 2)
        return luaL_error(lua, "love.graphics.setDefaultFilter: takes 2 arguments, \"nearest\" and \"nearest\", not %d",
                          top);
    for (int index = 1; index <= 2; index++) {
        if (lua_type(lua, index) != LUA_TSTRING)
            return luaL_error(lua, "love.graphics.setDefaultFilter: argument %d is a %s, not a string", index,
                              luaL_typename(lua, index));
        const char *name = lua_tostring(lua, index);
        if (strcmp(name, "none") != 0 && strcmp(name, "linear") != 0 && strcmp(name, "nearest") != 0)
            return luaL_error(lua, "love.graphics.setDefaultFilter: Invalid filter mode '%s', expected one of: "
                                   "'none', 'linear', 'nearest'",
                              name);
        if (strcmp(name, "nearest") != 0)
            return luaL_error(lua, "love.graphics.setDefaultFilter: argument %d is \"%s\", only \"nearest\" is "
                                   "supported (the filter is not modelled)",
                              index, name);
    }
    return 0;
}

static void new_metatable_without_methods(lua_State *lua, const char *name) {
    luaL_newmetatable(lua, name);
    lua_newtable(lua);
    function_list_protect_methods(lua, -1, name);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);
}

typedef struct {
    canvas_store_t *store;
    transform_t *transform;
    const window_size_t *window;
} upvalues_t;

static void set_function(lua_State *lua, const upvalues_t *upvalues, lua_CFunction function, const char *name) {
    lua_pushlightuserdata(lua, upvalues->store);
    lua_pushlightuserdata(lua, upvalues->transform);
    lua_pushlightuserdata(lua, (void *)upvalues->window);
    lua_pushcclosure(lua, function, 3);
    lua_setfield(lua, -2, name);
}

void graphics_install(lua_State *lua, canvas_store_t *store, transform_t *transform, const window_size_t *window) {
    const upvalues_t upvalues = {store, transform, window};
    new_metatable_without_methods(lua, METATABLE);
    new_metatable_without_methods(lua, IMAGE_METATABLE);
    new_metatable_without_methods(lua, QUAD_METATABLE);
    lua_getglobal(lua, "love");
    lua_pushliteral(lua, "graphics");
    lua_createtable(lua, 0, 14);
    set_function(lua, &upvalues, new_canvas, "newCanvas");
    set_function(lua, &upvalues, set_canvas, "setCanvas");
    set_function(lua, &upvalues, clear, "clear");
    set_function(lua, &upvalues, points, "points");
    set_function(lua, &upvalues, rectangle, "rectangle");
    set_function(lua, &upvalues, set_color, "setColor");
    set_function(lua, &upvalues, set_default_filter, "setDefaultFilter");
    set_function(lua, &upvalues, new_image, "newImage");
    set_function(lua, &upvalues, new_quad, "newQuad");
    set_function(lua, &upvalues, draw, "draw");
    set_function(lua, &upvalues, get_dimensions, "getDimensions");
    set_function(lua, &upvalues, push, "push");
    set_function(lua, &upvalues, pop, "pop");
    set_function(lua, &upvalues, translate, "translate");
    function_list_protect_module(lua, -1, "love.graphics");
    lua_rawset(lua, -3);
    lua_pop(lua, 1);
}
