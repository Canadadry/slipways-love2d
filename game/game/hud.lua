-- Copy of src/game/hud.h then src/game/hud.c.
-- palette_blocks is 0-indexed, palette_blocks[block][colour].
local bit = require("bit")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")
local text_image = require("game.text_image")

local M = {}

local function to_int(x)
  return fixed.p8_fixed_flr_int(x)
end

M.palette_blocks = {
  [0] = { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  { [0] = 0, 1, 2, 131, 2, 5, 6, 7, 8, 4, 9, 11, 12, 5, 14, 9 },
  { [0] = 0, 1, 1, 131, 2, 1, 13, 6, 2, 2, 4, 3, 13, 5, 8, 4 },
  { [0] = 0, 0, 1, 129, 1, 1, 5, 13, 2, 2, 2, 3, 5, 1, 2, 2 },
  { [0] = 0, 0, 0, 129, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
  { [0] = 0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { [0] = 0, 2, 2, 130, 2, 2, 8, 8, 2, 2, 8, 8, 2, 2, 8, 8 },
  { [0] = 0, 1, 1, 129, 1, 1, 5, 13, 1, 1, 5, 5, 1, 1, 5, 5 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 10, 9, 0, 4, 2, 13, 0, 0 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 0, 10, 9, 0, 4, 13, 4, 0 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 0, 0, 10, 4, 0, 13, 9, 4 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 4, 0, 0, 9, 4, 13, 0, 9 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 9, 4, 0, 0, 9, 13, 9, 0 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 0, 9, 4, 0, 0, 13, 10, 9 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 7, 9, 0, 9, 2, 0, 13, 0, 10 },
  { [0] = 0, 1, 2, 131, 4, 5, 6, 6, 0, 0, 0, 0, 0, 13, 0, 0 },
}
local palette_blocks = M.palette_blocks

function M.hud_select_palette_block(mem, no)
  local block = fixed.p8_fixed_flr_int(no)
  assert(block >= 0 and block < 16,
    "hud_select_palette_block: only blocks 0..15 exist; the cart would read past its table")
  for i = 0, 15 do
    mem.draw_palette[i] = palette_blocks[block][i]
  end
end

function M.hud_select_palette_block_default(mem)
  M.hud_select_palette_block(mem, fixed.p8_fixed_from_int(0))
end

function M.hud_palette_block_default_in_force(mem)
  for i = 0, 15 do
    if mem.draw_palette[i] ~= palette_blocks[0][i] then
      return false
    end
  end
  return true
end

function M.hud_draw_bar(gfx, mem, y1, y2, col)
  assert(gfx ~= nil, "hud_draw_bar: p8_rectfill dereferences the graphics state, so a NULL one is not a no-op")
  if gfx ~= nil then
    gfx.pen_color = col
  end
  gfx_module.p8_rectfill(gfx, mem, 0, to_int(y1), vram.P8_SCREEN_WIDTH - 1, to_int(y2), col)
end

function M.hud_draw_bar_y1(gfx, mem, y1, col)
  M.hud_draw_bar(gfx, mem, y1, y1, col)
end

function M.hud_draw_bar_default(gfx, mem, y1, y2)
  local col = (gfx ~= nil) and gfx.pen_color or 6
  M.hud_draw_bar(gfx, mem, y1, y2, col)
end

function M.hud_draw_help_frame(gfx, mem, x1, y1, x2)
  local galaxy_menu_button1 = to_int(x1)
  local top_pixel = to_int(y1)
  local galaxy_menu_button2 = to_int(x2)

  gfx_module.p8_rect(gfx, mem, galaxy_menu_button1, top_pixel, galaxy_menu_button2 + 1, top_pixel + 12, 0)
  gfx_module.p8_rectfill(gfx, mem, galaxy_menu_button1 + 1, top_pixel + 1, galaxy_menu_button2, top_pixel + 11, 5)
  gfx_module.p8_spr(gfx, mem, 70, galaxy_menu_button1, top_pixel, 1, 2)
  gfx_module.p8_spr(gfx, mem, 71, galaxy_menu_button2 - 2, top_pixel, 1, 2)
end

-- strlen: a NUL byte ends the text.
local function aligned_x(t, x, a)
  local strlen = (string.find(t, "\0", 1, true) or #t + 1) - 1
  return to_int(x) - to_int(fixed.p8_fixed_mul(fixed.p8_fixed_mul(a, fixed.p8_fixed_from_int(4)),
    fixed.p8_fixed_from_int(strlen)))
end

function M.hud_print_outlined(gfx, mem, t, x, y, c, a)
  gfx_module.p8_print_outlined(gfx, mem, t, aligned_x(t, x, a), to_int(y), bit.band(c, 0x0F))
end

-- hud_print_outlined's contract (same pixels, same draw palette after), through
-- text_image_print_outlined. gfx's camera moves during a build and is restored.
function M.hud_print_outlined_text_image(gfx, mem, t, x, y, c, a)
  text_image.text_image_print_outlined(gfx, mem, t, aligned_x(t, x, a), to_int(y), bit.band(c, 0x0F))
end

function M.hud_print_outlined_default(gfx, mem, t, x, y, c)
  M.hud_print_outlined(gfx, mem, t, x, y, c, fixed.p8_fixed_from_int(0))
end

return M
