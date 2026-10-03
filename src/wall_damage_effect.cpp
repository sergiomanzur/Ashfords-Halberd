#include "effects.hpp"
#include "scenario.hpp"
#include <stdexcept>
namespace ashford {
void tick_wall_damage_timers(ScenarioState &s) {
  if (!(s.flags[6][0] & 8) && !(s.effect_flags & 1))
    return;
  if (!s.effect_pool)
    return;
  bool active = false;
  for (const auto &e : s.effect_pool->works)
    if ((e.flags & 1) && !(e.state_flags & 0x1000000) && e.id == 29)
      active = true;
  if (!active)
    return;
  auto pool = std::make_shared<NativeEffectPool>(*s.effect_pool);
  // Validate all expiring links before committing counters or any wall attrs.
  std::vector<size_t> restore;
  for (auto &e : pool->works) {
    if (!(e.flags & 1) || (e.state_flags & 0x1000000) || e.id != 29)
      continue;
    e.control0 = int32_t((uint32_t(e.control0) + 1u) & 31u);
    if (e.control0)
      continue;
    if (!s.collision || !e.linked_wall ||
        *e.linked_wall >= s.collision->walls.size() + s.dynamic_walls.size())
      throw std::runtime_error("Invalid expiring wall-timer link");
    restore.push_back(*e.linked_wall);
    e.flags = 0;
  }
  for (size_t index : restore) {
    if (index < s.collision->walls.size())
      s.collision->walls[index].attribute |= 0x40;
    else
      s.dynamic_walls[index - s.collision->walls.size()].attribute |= 0x40;
  }
  s.effect_pool = std::move(pool);
}
} // namespace ashford
