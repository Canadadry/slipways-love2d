-- Copy of src/game/planet_display.h then src/game/planet_display.c.
--
-- Include cycle: game.planet, game.planet_family, game.economy, game.message_box,
-- game.research_display, game.slipway, game.ui, game.ui_button and game.probe (home of
-- main_economy_typed and targeting_of) are required at call time, inside the functions that need
-- them, so that this module loads before they exist.
--
-- down_t and layout_row_tuple_t zeroes are local.
--
-- A planet is one table with its entity header: the planet's own `has` (PLANET_HAS_* bits) is
-- `has_`, since the header's `has` holds the ENTITY_HAS_* bits.
local bit = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local vec = require("engine.vec")
local economy_formulas = require("game.economy_formulas")
local game_data = require("game.game_data")
local hud = require("game.hud")
local image_slot = require("game.image_slot")
local layout = require("game.layout")
local map_tables = require("game.map_tables")
local research = require("game.research")
local row_image = require("game.row_image")
local tech_gate = require("game.tech_gate")
local trade = require("game.trade")
local visual_random = require("game.visual_random")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local image = require("p8.image")
local str = require("p8.str")

local M = {}

local function down_zero()
  return { stop = 0, held = nil }
end

-- layout_row_tuple_t tuple = {0}.
local function tuple_zero()
  local scalars = {}
  for i = 0, layout.LAYOUT_ROW_TUPLE_CAP - 1 do
    scalars[i] = layout.layout_row_scalar_nothing()
  end
  return { count = 0, scalars = scalars }
end

local function lineage_has(self, l)
  return bit.band(self.class.lineage, entity_class.LINEAGE(l)) ~= 0
end

local function planet_production_cell(kw)
  local tuple = tuple_zero()

  if kw.has_resource ~= 0 then
    tuple.count = 2
    tuple.scalars[0] = layout.layout_row_scalar_fixed(kw.resource)
    tuple.scalars[1] = layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int(5))
  end
  return layout.layout_row_cell_tuple(tuple)
end

local ROW_SIGNATURE_VALID = 1
local ROW_SIGNATURE_HAS_OUTPUT = 2
local ROW_NEED_HAS_RESOURCE = 1
local ROW_NEED_SATISFIED_ZERO = 2

local function planet_row_signature(needs_list, has_output, output, amount, route_count)
  local planet = require("game.planet")
  local signature

  assert(route_count >= 0 and route_count <= 0xFFFF, "planet_row_signature: a wider route count would alias in the signature")
  signature = planet.planet_row_signature_zero()
  signature.flags = (has_output ~= 0) and ROW_SIGNATURE_HAS_OUTPUT or 0
  signature.output = output
  signature.amount = amount
  signature.route_count = route_count
  if needs_list ~= nil and #needs_list > planet.PLANET_ROW_SIGNATURE_NEEDS then
    return signature
  end
  if needs_list ~= nil then
    for index = 1, #needs_list do
      local entry = needs_list[index]
      if entry ~= nil then
        local need = entry
        local resource = research.need_resource(need)

        if resource.kind == trade.RESOURCE_FIXED then
          signature.need_flags[signature.needs_count] =
            bit.bor(signature.need_flags[signature.needs_count], ROW_NEED_HAS_RESOURCE)
          signature.need_resources[signature.needs_count] = resource.number
        end
        if need.has_satisfied ~= 0 and need.satisfied == 0 then
          signature.need_flags[signature.needs_count] =
            bit.bor(signature.need_flags[signature.needs_count], ROW_NEED_SATISFIED_ZERO)
        end
      end
      signature.needs_count = signature.needs_count + 1
    end
  end
  signature.flags = bit.bor(signature.flags, ROW_SIGNATURE_VALID)
  return signature
end

local function planet_row_signature_equal(left, right)
  if left.output ~= right.output or left.amount ~= right.amount or left.route_count ~= right.route_count
    or left.needs_count ~= right.needs_count or left.flags ~= right.flags then
    return 0
  end
  for i = 0, left.needs_count - 1 do
    if left.need_flags[i] ~= right.need_flags[i] or left.need_resources[i] ~= right.need_resources[i] then
      return 0
    end
  end
  return 1
end

function M.planet_refresh(self)
  local planet = require("game.planet")
  local research_display = require("game.research_display")
  local needs_list
  local by_list
  local has_output
  local output
  local mu
  local keys
  local layout_row_result = nil
  local route_count
  local signature
  local found

  if self == nil then
    return
  end

  needs_list = (bit.band(self.has_, planet.PLANET_HAS_NEEDS) ~= 0) and self.needs or nil
  do
    local kw = planet.planet_production_read(self)
    has_output = kw.has_resource
    output = kw.resource
    mu = kw.amount
  end
  by_list = (bit.band(self.has_, planet.PLANET_HAS_OUTGOING_ROUTES) ~= 0) and self.outgoing_routes or nil
  route_count = (by_list ~= nil) and #by_list or 0
  signature = planet_row_signature(needs_list, has_output, output, mu, route_count)
  if bit.band(signature.flags, ROW_SIGNATURE_VALID) ~= 0
    and planet_row_signature_equal(signature, self.layout_row_signature) ~= 0 then
    return
  end

  found, keys = research_display.research_display_planet_resource_row_keys(needs_list, has_output, output, mu,
    fixed.p8_fixed_from_int(route_count), fixed.p8_fixed_from_int(0), layout.layout_row_keys_zero())
  if found ~= 0 then
    layout_row_result = layout.layout_row_new(keys)
  end
  self.layout_row = layout_row_result
  self.layout_row_signature = signature
end

function M.planet_release_row(self)
  local jo = self

  jo.layout_row = nil
  jo.layout_row_signature.flags = 0
end

function M.planet_release_method(self)
  M.planet_release_row(self)
end

-- planet_inspection_panel_ctx_t: { received_needs }.
local function planet_inspection_panel_filter_step(e, ctx_ptr)
  local ctx = ctx_ptr
  local gc = fixed.p8_fixed_from_int(0)
  local count

  if e ~= nil then
    local i = e
    if i.has_satisfied ~= 0 then
      gc = i.satisfied
    end
  end
  count = fixed.p8_fixed_flr_int(gc)
  for _ = 0, count - 1 do
    ctx.received_needs[#ctx.received_needs + 1] = e
  end
  return (gc == fixed.p8_fixed_from_int(0)) and 1 or 0
end

local PLANET_INSPECTION_PANEL_ROWS = 4

local function planet_dollar(s)
  return str.p8_concat2(s, "$")
end

local function planet_open_box_frame(self)
  local message_box = require("game.message_box")
  local frame = message_box.open_box_frame_zero()

  if self.description ~= nil then
    message_box.open_box_frame_set_description(frame, self.description)
  end
  frame.has_back_color = 1
  frame.back_color = self.archetype.back_color
  frame.has_row_step = 1
  frame.row_step = vec.vec_make(self.archetype.row_step.x, self.archetype.row_step.y)
  frame.has_pos = entity_class.entity_has_pos(self)
  frame.pos = vec.vec_make(self.pos.x, self.pos.y)
  return frame
end

-- A need_t's first member is its row: the C passes the need where a data_row_t is read.
local function need_row(need)
  if need == nil then
    return nil
  end
  return need.row
end

local gap_items = {
  [0] = map_tables.map_tables_make_data_item({ is_false = 1 }),
  map_tables.map_tables_make_data_item({ number = 1 }),
}
local gap = map_tables.map_tables_make_data_row({ count = 2, items = gap_items })

function M.planet_inspection_panel(self)
  local planet = require("game.planet")
  local message_box = require("game.message_box")
  local ui_button = require("game.ui_button")
  local buttons = {}
  local row_count = 0
  local icon_row = nil
  local filter_context = {}
  local needs
  local unmet_needs
  local frame

  if self == nil then
    return
  end

  for i = 0, PLANET_INSPECTION_PANEL_ROWS - 1 do
    buttons[i] = ui_button.button_keys_zero({})
  end
  if lineage_has(self, entity_class.LINEAGE_STRUCTURE) and self.archetype.name ~= nil then
    icon_row = self.archetype.name
  elseif self.has_development_tier ~= 0 then
    local mi = fixed.p8_fixed_flr_int(self.development_tier)

    if mi >= 1 and mi <= game_data.PLANET_MOOD_ROWS_COUNT then
      icon_row = planet.planet_mood_rows()[mi - 1]
    end
  end
  if icon_row ~= nil then
    buttons[0].layout_row_keys = layout.layout_row_keys_of_row(icon_row)
  end
  buttons[0].has_side_tooltip = 1
  if self.has_net_income ~= 0 then
    local discard_color = 0
    local h_keys = buttons[0].side_tooltip_row
    local signed

    signed, discard_color = economy_formulas.economy_formulas_format_signed(self.net_income, 0, nil, discard_color)
    layout.layout_row_keys_append(h_keys, layout.layout_row_cell_string(planet_dollar(signed)))
    h_keys.has_background = 1
    h_keys.background_is_bool = (self.net_income < fixed.p8_fixed_from_int(0)) and 1 or 0
    h_keys.background = (self.net_income >= fixed.p8_fixed_from_int(0)) and fixed.p8_fixed_from_int(1) or 0
    buttons[0].has_side_tooltip_row = 1
  end
  row_count = 1

  filter_context.received_needs = {}

  needs = (bit.band(self.has_, planet.PLANET_HAS_NEEDS) ~= 0) and self.needs or nil
  unmet_needs = {}
  if needs ~= nil then
    for index = 1, #needs do
      local need = needs[index]
      if planet_inspection_panel_filter_step(need, filter_context) ~= 0 then
        unmet_needs[#unmet_needs + 1] = need
      end
    end
  end

  if #unmet_needs > 0 then
    layout.layout_row_keys_append(buttons[row_count].layout_row_keys, layout.layout_row_cell_string("needs:"))
    for index = 1, #unmet_needs do
      layout.layout_row_keys_append(buttons[row_count].layout_row_keys,
        layout.layout_row_cell_row(need_row(unmet_needs[index])))
    end
    row_count = row_count + 1
  else
    local development_tier_fixed = (self.has_development_tier ~= 0) and self.development_tier or fixed.p8_fixed_from_int(0)
    local kw = planet.planet_production_read(self)
    local r_w = 0
    local resource_is_r_w = false

    if kw.has_resource ~= 0 then
      local found
      found, r_w = map_tables.resource_code_lookup("w", r_w)
      resource_is_r_w = found ~= 0 and kw.resource == fixed.p8_fixed_from_int(r_w)
    end

    if development_tier_fixed < fixed.p8_fixed_from_int(4) and needs ~= nil and #needs > 0 and not resource_is_r_w then
      local last_only = lineage_has(self, entity_class.LINEAGE_STRUCTURE) and self.archetype.is_structure ~= 0

      layout.layout_row_keys_append(buttons[row_count].layout_row_keys, layout.layout_row_cell_string("wants more:"))
      if last_only then
        layout.layout_row_keys_append(buttons[row_count].layout_row_keys,
          layout.layout_row_cell_row(need_row(needs[#needs])))
      else
        for index = 1, #needs do
          layout.layout_row_keys_append(buttons[row_count].layout_row_keys,
            layout.layout_row_cell_row(need_row(needs[index])))
        end
      end
      row_count = row_count + 1
    end
  end

  if #filter_context.received_needs >= 1 then
    layout.layout_row_keys_append(buttons[row_count].layout_row_keys, layout.layout_row_cell_string("receives:"))
    for index = 1, #filter_context.received_needs do
      layout.layout_row_keys_append(buttons[row_count].layout_row_keys,
        layout.layout_row_cell_row(need_row(filter_context.received_needs[index])))
    end
    row_count = row_count + 1
  end

  do
    local kw = planet.planet_production_read(self)
    local mu = kw.amount

    if mu > fixed.p8_fixed_from_int(0) then
      local makes = kw.has_resource ~= 0 and game_data.int_seq_has_fixed(planet.planet_made_resources(), kw.resource) ~= 0

      if makes then
        local amount_str = str.p8_tostr_fixed(mu)
        layout.layout_row_keys_append(buttons[row_count].layout_row_keys, layout.layout_row_cell_string("makes:"))
        layout.layout_row_keys_append(buttons[row_count].layout_row_keys, layout.layout_row_cell_string(amount_str))
      else
        local by = (bit.band(self.has_, planet.PLANET_HAS_OUTGOING_ROUTES) ~= 0) and self.outgoing_routes or nil
        local by_count = (by ~= nil) and #by or 0
        local count_str = str.p8_tostr_fixed(fixed.p8_fixed_from_int(by_count))
        local amount_str = str.p8_tostr_fixed(mu)
        local combined = str.p8_concat2(str.p8_concat2(count_str, "/"),
          amount_str)
        layout.layout_row_keys_append(buttons[row_count].layout_row_keys, layout.layout_row_cell_string("exports:"))
        layout.layout_row_keys_append(buttons[row_count].layout_row_keys, layout.layout_row_cell_string(combined))
      end
      layout.layout_row_keys_append(buttons[row_count].layout_row_keys, planet_production_cell(kw))
      row_count = row_count + 1
    end
  end

  -- Starts at 1: row 0's layout row is a single icon, not a list.
  for index = 1, row_count - 1 do
    layout.layout_row_keys_append(buttons[index].layout_row_keys, layout.layout_row_cell_row(gap))
  end

  frame = planet_open_box_frame(self)
  message_box.message_box_open_box(frame, buttons, row_count)
end

function M.planet_accept_drop(self, s)
  local slipway = require("game.slipway")
  local out = down_zero()

  if self == nil then
    return out
  end
  if self.state ~= entity_class.ENTITY_STATE_SETTLED then
    return out
  end
  if s ~= nil and s.class == slipway.slipway_class() and slipway.slipway_start(s) == self then
    M.planet_inspection_panel(self)
    return out
  end
  out.stop = 1
  return out
end

local function planet_projects_panel_on_click_method(self)
  local planet = require("game.planet")
  local economy = require("game.economy")
  local probe = require("game.probe")
  local pt
  local b
  local od = fixed.p8_fixed_from_int(0)

  if self == nil then
    return
  end
  pt = entity_id.entity_get(self.project_planet)
  b = research.research_project_get(self.project)
  if self.has_project_cost ~= 0 then
    od = self.project_cost
  end

  assert(pt ~= nil and planet.planet_is_family(pt) ~= 0, "the projects panel button lost its captured planet")
  if economy.economy_spend(probe.main_economy_typed(), od) ~= 0 then
    M.planet_apply_project(pt, b)
  end
end

local function planet_projects_panel_keys(self, b, keys)
  local research_display = require("game.research_display")
  local ui_button = require("game.ui_button")
  local od = research.research_project_cost(b)

  ui_button.button_keys_zero(keys)
  if b.has_icon_sprite ~= 0 then
    keys.layout_row_keys = research_display.research_project_icon_row_keys(b)
  else
    local _
    _, keys.layout_row_keys = research_display.research_project_layout_row_keys(b, keys.layout_row_keys)
  end

  keys.has_side_tooltip = 1
  keys.side_tooltip_is_false = (od <= fixed.p8_fixed_from_int(0)) and 1 or 0
  if od > fixed.p8_fixed_from_int(0) then
    layout.layout_row_keys_append(keys.side_tooltip_row,
      layout.layout_row_cell_string(planet_dollar(str.p8_tostr_fixed(od))))
    keys.side_tooltip_row.has_color = 1
    keys.side_tooltip_row.color = fixed.p8_fixed_from_int(13)
    keys.has_side_tooltip_row = 1
  end

  keys.project_planet = entity_id.entity_id_link(self)
  keys.project = research.research_project_id_of(b)
  keys.has_project_cost = 1
  keys.project_cost = od
  keys.has_on_click = 1
  keys.on_click = planet_projects_panel_on_click_method
  keys.has_tooltip = 1
  ui_button.button_keys_set_tooltip_row(keys, research_display.research_project_tooltip(b))
end

function M.planet_projects_panel(self)
  local research_display = require("game.research_display")
  local message_box = require("game.message_box")
  local ui_button = require("game.ui_button")
  local available_projects_list
  local bs
  local frame
  local cap
  local n = 0

  if self == nil then
    return
  end

  available_projects_list = research_display.research_display_available_projects(self)
  cap = (available_projects_list.count > 0) and available_projects_list.count or 1
  bs = {}
  for i = 0, cap - 1 do
    bs[i] = ui_button.button_keys_zero({})
  end
  for i = 0, available_projects_list.count - 1 do
    planet_projects_panel_keys(self, research.research_project_get(available_projects_list.items[i]), bs[n])
    n = n + 1
  end

  frame = planet_open_box_frame(self)
  message_box.message_box_open_box(frame, bs, n)
end

local planet_draw_on_screen

local PLANET_DISC_RADIUS = 7
local PLANET_RING_RADIUS = 6

local planet_images_built = 0

local function planet_draw_disc_image(gfx, mem, context)
  gfx_module.p8_circfill(gfx, mem, PLANET_DISC_RADIUS, PLANET_DISC_RADIUS, PLANET_DISC_RADIUS, 0)
end

-- context is the ring colour itself, where the C passes a pointer to it.
local function planet_draw_ring_image(gfx, mem, context)
  gfx_module.p8_circ(gfx, mem, PLANET_RING_RADIUS, PLANET_RING_RADIUS, PLANET_RING_RADIUS, context)
end

-- Draws no visual random number and reads no game state: the image cache's rule.
local function planet_build_images(gfx, mem)
  local ring_colors = require("game.planet").planet_ring_colors()

  assert(gfx.fillp_pattern == 0, "planet images are built with no fill pattern")
  if planet_images_built ~= 0 then
    return
  end
  image.p8_image_build(gfx, mem, image_slot.IMAGE_SLOT_PLANET_DISC, 2 * PLANET_DISC_RADIUS + 1, 2 * PLANET_DISC_RADIUS + 1,
    planet_draw_disc_image, nil)
  assert(ring_colors.count == game_data.PLANET_RING_COLORS_COUNT)
  for index = 0, game_data.PLANET_RING_COLORS_COUNT - 1 do
    image.p8_image_build(gfx, mem, image_slot.IMAGE_SLOT_PLANET_RING_FIRST + index, 2 * PLANET_RING_RADIUS + 1,
      2 * PLANET_RING_RADIUS + 1, planet_draw_ring_image, ring_colors.items[index])
  end
  planet_images_built = 1
end

function M.planet_draw(graphics, memory, self, screen_pos)
  if self == nil or entity_geom.entity_geom_is_on_screen(self) == 0 then
    return
  end
  planet_draw_on_screen(graphics, memory, self, screen_pos)
end

planet_draw_on_screen = function(graphics, memory, self, screen_pos)
  local planet = require("game.planet")
  local ui = require("game.ui")
  local shaken_pos
  local screen_x
  local screen_y
  local spr_index = 43

  -- Everything below draws at shaken_pos, never at screen_pos.
  shaken_pos = entity_geom.entity_geom_shake(screen_pos, self)

  if self.state == entity_class.ENTITY_STATE_VANISHING then
    local t = self.state_time
    if visual_random.visual_random_draw(fixed.p8_fixed_from_int(30)) < fixed.p8_fixed_sub(t, fixed.p8_fixed_from_int(30)) then
      return
    end
  end

  screen_x = fixed.p8_fixed_flr_int(shaken_pos.x)
  screen_y = fixed.p8_fixed_flr_int(shaken_pos.y)

  if graphics.fillp_pattern == 0 then
    planet_build_images(graphics, memory)
    image.p8_image_draw(graphics, memory, image_slot.IMAGE_SLOT_PLANET_DISC, screen_x - PLANET_DISC_RADIUS,
      screen_y - PLANET_DISC_RADIUS)
  else
    gfx_module.p8_circfill(graphics, memory, screen_x, screen_y, PLANET_DISC_RADIUS, 0)
  end

  hud.hud_select_palette_block(memory,
    lineage_has(self, entity_class.LINEAGE_TRADING_HUB) and self.palette_block or fixed.p8_fixed_from_int(0))
  gfx_module.p8_palt(memory, 3, 0)
  gfx_module.p8_palt(memory, 0, 1)

  if self.planet_type ~= nil then
    spr_index = self.planet_type.sprite
  elseif lineage_has(self, entity_class.LINEAGE_STRUCTURE) then
    spr_index = fixed.p8_fixed_flr_int(self.sprite)
  end
  gfx_module.p8_spr(graphics, memory, spr_index, screen_x - 4, screen_y - 4, 2, 2)
  hud.hud_select_palette_block_default(memory)

  if planet.planet_is_colonized(self) ~= 0 then
    local mi = fixed.p8_fixed_flr_int((self.has_development_tier ~= 0) and self.development_tier or fixed.p8_fixed_from_int(0))
    local color = 0
    local found
    found, color = game_data.int_seq_at(planet.planet_ring_colors().items, planet.planet_ring_colors().count, mi, color)
    if found ~= 0 and graphics.fillp_pattern == 0 then
      planet_build_images(graphics, memory)
      image.p8_image_draw(graphics, memory, image_slot.IMAGE_SLOT_PLANET_RING_FIRST + mi - 1, screen_x - PLANET_RING_RADIUS,
        screen_y - PLANET_RING_RADIUS)
    else
      gfx_module.p8_circ(graphics, memory, screen_x, screen_y, PLANET_RING_RADIUS, color)
    end
    gfx_module.p8_spr_default(graphics, memory, mi + 1, screen_x - 3, screen_y - 6)
  end

  if ui.main_ui() ~= nil and ui.ui_pad_circle(ui.main_ui()) == self then
    gfx_module.p8_circ(graphics, memory, screen_x, screen_y, 9, 7)
  end

  do
    if self.layout_row ~= nil then
      local offset = vec.vec_add(shaken_pos, vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(7)))
      row_image.row_image_draw_at(graphics, memory, self.layout_row, offset, fixed.P8_FIXED_LIT(0x00008000, 0.5))
    end
  end

  do
    local targeted_by = entity_id.entity_get(self.targeted_by)

    if targeted_by ~= nil then
      local probe = require("game.probe")
      local dw = fixed.p8_fixed_from_int(0)
      local radius_f
      local radius

      if probe.targeting_of(targeted_by) ~= nil then
        dw = probe.targeting_of(targeted_by).archetype.attach_distance
      end
      radius_f = fixed.p8_fixed_mul(fixed.p8_fixed_sqrt(dw), fixed.p8_fixed_from_int(128))
      radius = fixed.p8_fixed_flr_int(radius_f)

      -- The cart's fractional .1 pattern is transparent: p8_fillp_transparent, not p8_fillp.
      gfx_module.p8_fillp_transparent(graphics, 0xAAAA)
      gfx_module.p8_circfill(graphics, memory, screen_x, screen_y, radius, 2)
      gfx_module.p8_fillp_reset(graphics)
      self.targeted_by = nil
    end
  end
end

function M.planet_refresh_method(self)
  M.planet_refresh(self)
end

function M.planet_accept_drop_method(self, s)
  return M.planet_accept_drop(self, s)
end

function M.planet_draw_method(self, screen_pos)
  assert(self.class.skip_draw_off_screen ~= 0 and entity_geom.entity_geom_is_on_screen(self) ~= 0,
    "planet_draw_method trusts draw_layers' cull: only a class with skip_draw_off_screen may use it")
  planet_draw_on_screen(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self, screen_pos)
end

function M.planet_left_press_method(self, mouse)
  return M.planet_left_press(self)
end

function M.planet_left_press(self)
  local out = down_zero()

  if self == nil then
    return out
  end
  if self.state == entity_class.ENTITY_STATE_KNOWN then
    M.planet_projects_panel(self)
    out.stop = 1
    return out
  end
  if self.state == entity_class.ENTITY_STATE_SETTLED then
    local slipway = require("game.slipway")
    local keys = slipway.slipway_keys_zero()
    local made

    assert(slipway.slipway_ready() ~= 0, "slipway_init() must run before planet_left_press builds a slipway")
    keys.start = entity_id.entity_id_link(self)
    made = slipway.slipway_new(keys)
    out.held = entity_id.entity_id_link(made)
    out.stop = (made ~= nil) and 1 or 0
  end
  return out
end

function M.planet_apply_project(self, research_project_instance)
  local planet = require("game.planet")
  local result_planet
  local fp = fixed.p8_fixed_from_int(2)

  research.research_project_assert(research_project_instance)
  if self == nil then
    return
  end
  result_planet = self

  if research_project_instance ~= nil and research_project_instance.class_name ~= nil then
    local target_kind = nil
    local keys = planet.planet_keys_zero()

    assert(#self.slipways == 0,
      "a linked planet never dies: the routes kept in outgoing_routes would name it, and the cart never drops them")
    self.is_dead = 1

    target_kind = require("game.planet_family").planet_kind_named(research_project_instance.class_name)

    keys.map_pos = vec.vec_make(self.map_pos.x, self.map_pos.y)
    keys.has_map_pos = entity_class.entity_has_map_pos(self)
    keys.pos = vec.vec_make(self.pos.x, self.pos.y)
    keys.has_pos = entity_class.entity_has_pos(self)
    if bit.band(self.has_, planet.PLANET_HAS_ID) ~= 0 then
      planet.planet_keys_set_id(keys, self.planet_id)
    end

    result_planet = (target_kind ~= nil) and target_kind.new_entity(keys) or nil
  elseif research_project_instance ~= nil and research_project_instance.next_type ~= nil then
    self.planet_type = game_data.planet_types_lookup(research_project_instance.next_type)
  else
    entity.entity_set_state(self, entity_class.ENTITY_STATE_SETTLED)
    planet.planet_install_project(self, research_project_instance)
    self.fold_anchor = fixed.p8_fixed_from_int(1)
    fp = tech_gate.tech_gate_if_unlocked("autoassemblers", fixed.p8_fixed_from_int(3), fixed.p8_fixed_from_int(2))
  end

  require("game.economy").economy_advance_clock(require("game.probe").main_economy_typed(), fp)

  planet.planet_restart_shake(result_planet)
end

return M
