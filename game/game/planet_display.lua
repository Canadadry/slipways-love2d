-- Include cycle: game.planet, game.planet_family, game.economy, game.message_box,
-- game.research_display, game.slipway, game.ui, game.ui_button and game.probe (home of
-- main_economy_typed and targeting_of) are required at call time, inside the functions that need
-- them, so that this module loads before they exist.
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
  return { stop = false, held = nil }
end

local function tuple_zero()
  return { count = 0, scalars = {} }
end

local function lineage_has(self, l)
  return bit.band(self.class.lineage, entity_class.LINEAGE(l)) ~= 0
end

local function planet_production_cell(kw)
  local tuple = tuple_zero()

  if kw.resource ~= nil then
    tuple.count = 2
    tuple.scalars[1] = layout.layout_row_scalar_fixed(kw.resource)
    tuple.scalars[2] = layout.layout_row_scalar_fixed(fixed.from_int(5))
  end
  return layout.layout_row_cell_tuple(tuple)
end

local ROW_SIGNATURE_VALID = 1
local ROW_SIGNATURE_HAS_OUTPUT = 2
local ROW_NEED_HAS_RESOURCE = 1
local ROW_NEED_SATISFIED_ZERO = 2

local function planet_row_signature(needs_list, output, amount, route_count)
  local planet = require("game.planet")
  local signature

  signature = planet.planet_row_signature_zero()
  signature.flags = (output ~= nil) and ROW_SIGNATURE_HAS_OUTPUT or 0
  -- An absent output is keyed as 0; ROW_SIGNATURE_HAS_OUTPUT tells it from a present 0.
  signature.output = output or 0
  signature.amount = amount
  signature.route_count = route_count
  if needs_list ~= nil then
    for index = 1, #needs_list do
      local entry = needs_list[index]
      local n = signature.needs_count + 1

      signature.need_flags[n] = 0
      signature.need_resources[n] = 0
      if entry ~= nil then
        local need = entry
        local resource = research.need_resource(need)

        if resource.kind == trade.RESOURCE_FIXED then
          signature.need_flags[n] = bit.bor(signature.need_flags[n], ROW_NEED_HAS_RESOURCE)
          signature.need_resources[n] = resource.number
        end
        if need.satisfied ~= nil and need.satisfied == 0 then
          signature.need_flags[n] = bit.bor(signature.need_flags[n], ROW_NEED_SATISFIED_ZERO)
        end
      end
      signature.needs_count = n
    end
  end
  signature.flags = bit.bor(signature.flags, ROW_SIGNATURE_VALID)
  return signature
end

local function planet_row_signature_equal(left, right)
  if left.output ~= right.output or left.amount ~= right.amount or left.route_count ~= right.route_count
    or left.needs_count ~= right.needs_count or left.flags ~= right.flags then
    return false
  end
  for i = 1, left.needs_count do
    if left.need_flags[i] ~= right.need_flags[i] or left.need_resources[i] ~= right.need_resources[i] then
      return false
    end
  end
  return true
end

function M.planet_refresh(self)
  local planet = require("game.planet")
  local research_display = require("game.research_display")
  local needs_list
  local by_list
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

  needs_list = self.needs
  do
    local kw = planet.planet_production_read(self)
    output = kw.resource
    mu = kw.amount
  end
  by_list = self.outgoing_routes
  route_count = (by_list ~= nil) and #by_list or 0
  signature = planet_row_signature(needs_list, output, mu, route_count)
  if planet_row_signature_equal(signature, self.layout_row_signature) then
    return
  end

  found, keys = research_display.research_display_planet_resource_row_keys(needs_list, output, mu,
    fixed.from_int(route_count), fixed.from_int(0), layout.layout_row_keys_zero())
  if found then
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

local function planet_inspection_panel_filter_step(e, ctx_ptr)
  local ctx = ctx_ptr
  local gc = fixed.from_int(0)
  local count

  if e ~= nil then
    local i = e
    if i.satisfied ~= nil then
      gc = i.satisfied
    end
  end
  count = fixed.flr_int(gc)
  for _ = 1, count do
    ctx.received_needs[#ctx.received_needs + 1] = e
  end
  return gc == fixed.from_int(0)
end

local function planet_dollar(s)
  return str.concat2(s, "$")
end

local function planet_open_box_frame(self)
  local message_box = require("game.message_box")
  local frame = message_box.open_box_frame_zero()

  if self.description ~= nil then
    message_box.open_box_frame_set_description(frame, self.description)
  end
  frame.back_color = self.archetype.back_color
  frame.row_step = vec.make(self.archetype.row_step.x, self.archetype.row_step.y)
  frame.pos = entity_class.pos(self)
  return frame
end

local function need_row(need)
  if need == nil then
    return nil
  end
  return need.row
end

local gap_items = {
  map_tables.make_data_item({ is_false = true }),
  map_tables.make_data_item({ number = 1 }),
}
local gap = map_tables.make_data_row({ count = 2, items = gap_items })

-- Returns the new last button and the new count.
local function planet_append_button(buttons, count)
  local ui_button = require("game.ui_button")
  local n = count + 1

  buttons[n] = ui_button.button_keys_zero({})
  return buttons[n], n
end

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

  buttons[1] = ui_button.button_keys_zero({})
  if lineage_has(self, entity_class.LINEAGE_STRUCTURE) and self.archetype.name ~= nil then
    icon_row = self.archetype.name
  elseif self.development_tier ~= nil then
    local mi = fixed.flr_int(self.development_tier)

    if mi >= 1 and mi <= game_data.PLANET_MOOD_ROWS_COUNT then
      icon_row = planet.planet_mood_rows()[mi]
    end
  end
  if icon_row ~= nil then
    buttons[1].layout_row_keys = layout.layout_row_keys_of_row(icon_row)
  end
  if self.net_income ~= nil then
    local discard_color = 0
    local h_keys = layout.layout_row_keys_zero()
    local signed

    signed, discard_color = economy_formulas.economy_formulas_format_signed(self.net_income, 0, nil, discard_color)
    layout.layout_row_keys_append(h_keys, layout.layout_row_cell_string(planet_dollar(signed)))
    h_keys.background_is_bool = self.net_income < fixed.from_int(0)
    h_keys.background = (self.net_income >= fixed.from_int(0)) and fixed.from_int(1) or 0
    buttons[1].side_tooltip_row = h_keys
  end
  row_count = 1

  filter_context.received_needs = {}

  needs = self.needs
  unmet_needs = {}
  if needs ~= nil then
    for index = 1, #needs do
      local need = needs[index]
      if planet_inspection_panel_filter_step(need, filter_context) then
        unmet_needs[#unmet_needs + 1] = need
      end
    end
  end

  if #unmet_needs > 0 then
    local row

    row, row_count = planet_append_button(buttons, row_count)
    layout.layout_row_keys_append(row.layout_row_keys, layout.layout_row_cell_string("needs:"))
    for index = 1, #unmet_needs do
      layout.layout_row_keys_append(row.layout_row_keys,
        layout.layout_row_cell_row(need_row(unmet_needs[index])))
    end
  else
    local development_tier_fixed = self.development_tier or fixed.from_int(0)
    local kw = planet.planet_production_read(self)
    local r_w = 0
    local resource_is_r_w = false

    if kw.resource ~= nil then
      local found
      found, r_w = map_tables.resource_code_lookup("w", r_w)
      resource_is_r_w = found and kw.resource == fixed.from_int(r_w)
    end

    if development_tier_fixed < fixed.from_int(4) and needs ~= nil and #needs > 0 and not resource_is_r_w then
      local last_only = lineage_has(self, entity_class.LINEAGE_STRUCTURE) and self.archetype.is_structure
      local row

      row, row_count = planet_append_button(buttons, row_count)
      layout.layout_row_keys_append(row.layout_row_keys, layout.layout_row_cell_string("wants more:"))
      if last_only then
        layout.layout_row_keys_append(row.layout_row_keys,
          layout.layout_row_cell_row(need_row(needs[#needs])))
      else
        for index = 1, #needs do
          layout.layout_row_keys_append(row.layout_row_keys,
            layout.layout_row_cell_row(need_row(needs[index])))
        end
      end
    end
  end

  if #filter_context.received_needs >= 1 then
    local row

    row, row_count = planet_append_button(buttons, row_count)
    layout.layout_row_keys_append(row.layout_row_keys, layout.layout_row_cell_string("receives:"))
    for index = 1, #filter_context.received_needs do
      layout.layout_row_keys_append(row.layout_row_keys,
        layout.layout_row_cell_row(need_row(filter_context.received_needs[index])))
    end
  end

  do
    local kw = planet.planet_production_read(self)
    local mu = kw.amount

    if mu > fixed.from_int(0) then
      local makes = kw.resource ~= nil and game_data.int_seq_has_fixed(planet.planet_made_resources(), kw.resource)
      local row

      row, row_count = planet_append_button(buttons, row_count)
      if makes then
        local amount_str = str.tostr_fixed(mu)
        layout.layout_row_keys_append(row.layout_row_keys, layout.layout_row_cell_string("makes:"))
        layout.layout_row_keys_append(row.layout_row_keys, layout.layout_row_cell_string(amount_str))
      else
        local by = self.outgoing_routes
        local by_count = (by ~= nil) and #by or 0
        local count_str = str.tostr_fixed(fixed.from_int(by_count))
        local amount_str = str.tostr_fixed(mu)
        local combined = str.concat2(str.concat2(count_str, "/"),
          amount_str)
        layout.layout_row_keys_append(row.layout_row_keys, layout.layout_row_cell_string("exports:"))
        layout.layout_row_keys_append(row.layout_row_keys, layout.layout_row_cell_string(combined))
      end
      layout.layout_row_keys_append(row.layout_row_keys, planet_production_cell(kw))
    end
  end

  -- Starts at 2: row 1's layout row is a single icon, not a list.
  for index = 2, row_count do
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
  if self.state ~= entity_class.STATE_SETTLED then
    return out
  end
  if s ~= nil and s.class == slipway.slipway_class() and slipway.slipway_start(s) == self then
    M.planet_inspection_panel(self)
    return out
  end
  out.stop = true
  return out
end

local function planet_projects_panel_on_click_method(self)
  local planet = require("game.planet")
  local economy = require("game.economy")
  local probe = require("game.probe")
  local pt
  local b
  local od = fixed.from_int(0)

  if self == nil then
    return
  end
  pt = entity_id.get(self.project_planet)
  b = research.research_project_get(self.project)
  if self.project_cost ~= nil then
    od = self.project_cost
  end

  assert(pt ~= nil and planet.planet_is_family(pt), "the projects panel button lost its captured planet")
  if economy.economy_spend(probe.main_economy_typed(), od) then
    M.planet_apply_project(pt, b)
  end
end

local function planet_projects_panel_keys(self, b, keys)
  local research_display = require("game.research_display")
  local ui_button = require("game.ui_button")
  local od = research.research_project_cost(b)

  ui_button.button_keys_zero(keys)
  if b.icon_sprite ~= nil then
    keys.layout_row_keys = research_display.research_project_icon_row_keys(b)
  else
    local _
    _, keys.layout_row_keys = research_display.research_project_layout_row_keys(b, keys.layout_row_keys)
  end

  if od > fixed.from_int(0) then
    keys.side_tooltip_row = layout.layout_row_keys_zero()
    layout.layout_row_keys_append(keys.side_tooltip_row,
      layout.layout_row_cell_string(planet_dollar(str.tostr_fixed(od))))
    keys.side_tooltip_row.color = fixed.from_int(13)
  end

  keys.project_planet = entity_id.link(self)
  keys.project = research.research_project_id_of(b)
  keys.project_cost = od
  keys.on_click = planet_projects_panel_on_click_method
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
  for i = 1, cap do
    bs[i] = ui_button.button_keys_zero({})
  end
  for i = 1, available_projects_list.count do
    n = n + 1
    planet_projects_panel_keys(self, research.research_project_get(available_projects_list.items[i]), bs[n])
  end

  frame = planet_open_box_frame(self)
  message_box.message_box_open_box(frame, bs, n)
end

local planet_draw_on_screen

local PLANET_DISC_RADIUS = 7
local PLANET_RING_RADIUS = 6

local planet_images_built = false

local function planet_draw_disc_image(gfx, mem, context)
  gfx_module.circfill(gfx, mem, PLANET_DISC_RADIUS, PLANET_DISC_RADIUS, PLANET_DISC_RADIUS, 0)
end

-- context is the ring colour itself.
local function planet_draw_ring_image(gfx, mem, context)
  gfx_module.circ(gfx, mem, PLANET_RING_RADIUS, PLANET_RING_RADIUS, PLANET_RING_RADIUS, context)
end

-- Draws no visual random number and reads no game state: the image cache's rule.
local function planet_build_images(gfx, mem)
  local ring_colors = require("game.planet").planet_ring_colors()

  assert(gfx.fillp_pattern == 0, "planet images are built with no fill pattern")
  if planet_images_built then
    return
  end
  image.build(gfx, mem, image_slot.IMAGE_SLOT_PLANET_DISC, 2 * PLANET_DISC_RADIUS + 1, 2 * PLANET_DISC_RADIUS + 1,
    planet_draw_disc_image, nil)
  assert(ring_colors.count == game_data.PLANET_RING_COLORS_COUNT)
  for index = 1, game_data.PLANET_RING_COLORS_COUNT do
    image.build(gfx, mem, image_slot.IMAGE_SLOT_PLANET_RING_FIRST + index - 1, 2 * PLANET_RING_RADIUS + 1,
      2 * PLANET_RING_RADIUS + 1, planet_draw_ring_image, ring_colors.items[index])
  end
  planet_images_built = true
end

function M.planet_draw(graphics, memory, self, screen_pos)
  if self == nil or not entity_geom.is_on_screen(self) then
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
  shaken_pos = entity_geom.shake(screen_pos, self)

  if self.state == entity_class.STATE_VANISHING then
    local t = self.state_time
    if visual_random.draw(fixed.from_int(30)) < fixed.sub(t, fixed.from_int(30)) then
      return
    end
  end

  screen_x = fixed.flr_int(shaken_pos.x)
  screen_y = fixed.flr_int(shaken_pos.y)

  if graphics.fillp_pattern == 0 then
    planet_build_images(graphics, memory)
    image.draw(graphics, memory, image_slot.IMAGE_SLOT_PLANET_DISC, screen_x - PLANET_DISC_RADIUS,
      screen_y - PLANET_DISC_RADIUS)
  else
    gfx_module.circfill(graphics, memory, screen_x, screen_y, PLANET_DISC_RADIUS, 0)
  end

  hud.hud_select_palette_block(memory,
    lineage_has(self, entity_class.LINEAGE_TRADING_HUB) and self.palette_block or fixed.from_int(0))
  gfx_module.palt(memory, 3, false)
  gfx_module.palt(memory, 0, true)

  if self.planet_type ~= nil then
    spr_index = self.planet_type.sprite
  elseif lineage_has(self, entity_class.LINEAGE_STRUCTURE) then
    spr_index = fixed.flr_int(self.sprite)
  end
  gfx_module.spr(graphics, memory, spr_index, screen_x - 4, screen_y - 4, 2, 2)
  hud.hud_select_palette_block_default(memory)

  if planet.planet_is_colonized(self) then
    local mi = fixed.flr_int(self.development_tier or fixed.from_int(0))
    local color = 0
    local found
    found, color = game_data.int_seq_at(planet.planet_ring_colors().items, planet.planet_ring_colors().count, mi, color)
    if found and graphics.fillp_pattern == 0 then
      planet_build_images(graphics, memory)
      image.draw(graphics, memory, image_slot.IMAGE_SLOT_PLANET_RING_FIRST + mi - 1, screen_x - PLANET_RING_RADIUS,
        screen_y - PLANET_RING_RADIUS)
    else
      gfx_module.circ(graphics, memory, screen_x, screen_y, PLANET_RING_RADIUS, color)
    end
    gfx_module.spr_default(graphics, memory, mi + 1, screen_x - 3, screen_y - 6)
  end

  if ui.main_ui() ~= nil and ui.ui_pad_circle(ui.main_ui()) == self then
    gfx_module.circ(graphics, memory, screen_x, screen_y, 9, 7)
  end

  do
    if self.layout_row ~= nil then
      local offset = vec.add(shaken_pos, vec.make(fixed.from_int(0), fixed.from_int(7)))
      row_image.row_image_draw_at(graphics, memory, self.layout_row, offset, fixed.LIT(0x00008000, 0.5))
    end
  end

  do
    local targeted_by = entity_id.get(self.targeted_by)

    if targeted_by ~= nil then
      local probe = require("game.probe")
      local dw = fixed.from_int(0)
      local radius_f
      local radius

      if probe.targeting_of(targeted_by) ~= nil then
        dw = probe.targeting_of(targeted_by).archetype.attach_distance
      end
      radius_f = fixed.mul(fixed.sqrt(dw), fixed.from_int(128))
      radius = fixed.flr_int(radius_f)

      -- The cart's fractional .1 pattern is transparent: gfx.fillp_transparent, not gfx.fillp.
      gfx_module.fillp_transparent(graphics, 0xAAAA)
      gfx_module.circfill(graphics, memory, screen_x, screen_y, radius, 2)
      gfx_module.fillp_reset(graphics)
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
  assert(self.class.skip_draw_off_screen and entity_geom.is_on_screen(self),
    "planet_draw_method trusts draw_layers' cull: only a class with skip_draw_off_screen may use it")
  planet_draw_on_screen(dispatch_context.gfx(), dispatch_context.mem(), self, screen_pos)
end

function M.planet_left_press_method(self, mouse)
  return M.planet_left_press(self)
end

function M.planet_left_press(self)
  local out = down_zero()

  if self == nil then
    return out
  end
  if self.state == entity_class.STATE_KNOWN then
    M.planet_projects_panel(self)
    out.stop = true
    return out
  end
  if self.state == entity_class.STATE_SETTLED then
    local slipway = require("game.slipway")
    local keys = slipway.slipway_keys_zero()
    local made

    assert(slipway.slipway_ready(), "slipway_init() must run before planet_left_press builds a slipway")
    keys.start = entity_id.link(self)
    made = slipway.slipway_new(keys)
    out.held = entity_id.link(made)
    out.stop = made ~= nil
  end
  return out
end

function M.planet_apply_project(self, research_project_instance)
  local planet = require("game.planet")
  local result_planet
  local fp = fixed.from_int(2)

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
    self.is_dead = true

    target_kind = require("game.planet_family").planet_kind_named(research_project_instance.class_name)

    keys.map_pos = entity_class.map_pos(self)
    keys.pos = entity_class.pos(self)
    if self.planet_id ~= nil then
      planet.planet_keys_set_id(keys, self.planet_id)
    end

    result_planet = (target_kind ~= nil) and target_kind.new_entity(keys) or nil
  elseif research_project_instance ~= nil and research_project_instance.next_type ~= nil then
    self.planet_type = game_data.planet_types_lookup(research_project_instance.next_type)
  else
    entity.set_state(self, entity_class.STATE_SETTLED)
    planet.planet_install_project(self, research_project_instance)
    self.fold_anchor = true
    fp = tech_gate.tech_gate_if_unlocked("autoassemblers", fixed.from_int(3), fixed.from_int(2))
  end

  require("game.economy").economy_advance_clock(require("game.probe").main_economy_typed(), fp)

  planet.planet_restart_shake(result_planet)
end

return M
