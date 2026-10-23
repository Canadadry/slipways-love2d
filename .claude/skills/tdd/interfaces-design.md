# Interface Design for Testability

Good interfaces make testing natural. Functions marked real are in `game/`; the others are illustrative.

1. **Accept dependencies, don't create them**

   ```lua
   -- Testable (real signature): the caller hands the graphics state and the vram
   function M.hud_draw_bar(gfx, mem, y1, y2, col)

   -- Hard to test: draws into a global screen
   function M.hud_draw_bar(y1, y2, col)
   ```

2. **Return results, don't produce side effects**

   ```lua
   -- Testable (real signature): returns the cost
   function M.economy_formulas_route_base_cost(from_pos, to_pos)

   -- Hard to test: adds into the global credits
   function M.economy_formulas_pay_route(from_pos, to_pos)
   ```

3. **Small surface area**
   - Fewer functions in the module's returned table = fewer tests needed
   - Fewer parameters = simpler test setup

4. **Make illegal states unrepresentable**

   A field meaningful only in some cases says the data is wrong. Here, presence is a field that is `nil` when absent, never a bit or a `has_` flag, and a link to another entity is its id, never its table (see `warn-review`'s reference rules):

   ```lua
   -- Allows an impossible state: the end's table outlives the end's death
   self.end_ = end_

   -- A checked id (real code, slipway_connect): entity_get returns nil once end_ is dead
   self.end_ = entity_id.entity_id_link(end_)
   ```

5. **Keep functions pure; push state to the top**

   Core logic is pure: same input, same output, no I/O, no global. A thin top layer holds the state and calls into it.

   ```lua
   -- Pure (real code): trivial to test
   function M.box_contains(box, p)

   -- Stateful: reads the entity and the mouse, stays at the top
   function M.entity_geom_contains_cursor_inline(e, mouse)
     -- ...
     return geom.box_contains(e.hit_box, point)
   end
   ```

6. **Assert preconditions at the boundary**

   Every public function asserts what it requires, so invalid input fails at the call site, not three frames later.

   ```lua
   function M.slipway_connect(self, end_)
     assert(self ~= nil and end_ ~= nil, "slipway_connect: a resumed slipway joins two saved nodes")
     -- ...
   end
   ```

   Assertions are not input validation for player data — they are contracts between modules. Key invariants inside a module ("no dead id in a live list") belong as assertions too, not just as tests.
