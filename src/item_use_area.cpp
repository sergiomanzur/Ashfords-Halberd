#include "item_use_area.hpp"
#include "runtime.hpp"
#include "player.hpp"
#include <stdexcept>
#include <cmath>
namespace ashford {
const CollisionRecord *selected_inventory_use_area(const ScenarioState &s) {
  if (!(s.flags[10][0] & 0x200) || !s.collision) return nullptr;
  const size_t index = s.floor_index, count = s.collision->floors.size();
  const CollisionRecord *area = index < count ? &s.collision->floors[index] :
      index-count < s.dynamic_floors.size() ? &s.dynamic_floors[index-count] : nullptr;
  return area && (area->flags & 1) && area->type == 0 ? area : nullptr;
}
void update_inventory_use_area(RoomRuntime &r) {
  auto &s = r.scenario;
  s.flags[10][0] &= ~0x200u;
  if (!s.collision || (r.player_work.flags & 6)) return;
  const auto probe = player_add_speed(r.world.front_anchor, r.world.player.yaw,
                                     3.f + (3.f-r.world.front_offset));
  const unsigned angle = (r.world.player.yaw + 8192u) & 0xc000u;
  auto inspect = [&](const CollisionRecord &a, size_t index) {
    if (!(a.flags & 1) || a.type != 0) return;
    if (!std::isfinite(a.position.x) || !std::isfinite(a.position.y) ||
        !std::isfinite(a.position.z) || !std::isfinite(a.size.x) ||
        !std::isfinite(a.size.y) || !std::isfinite(a.size.z))
      throw std::runtime_error("Nonfinite inventory use area");
    const unsigned forbidden = angle == 32768 ? 0x400 : angle == 16384 ? 0x800 :
                               angle == 0 ? 0x1000 : 0x2000;
    if (a.attribute & forbidden) return;
    const bool front = (a.attribute & 1) != 0;
    const auto p = front ? probe : r.world.player.position;
    if (front && a.floor != r.world.floor) return;
    if (p.x < a.position.x || p.x > a.position.x+a.size.x ||
        p.z < a.position.z || p.z > a.position.z+a.size.z) return;
    if (!front && (p.y < a.position.y ||
                   p.y > a.position.y+a.size.y)) return;
    if (index > 255) throw std::runtime_error("Inventory floor index exceeds source range");
    s.floor_index = static_cast<uint8_t>(index);
    s.flags[10][0] |= 0x200;
  };
  size_t index = 0;
  for (const auto &a : s.collision->floors) inspect(a, index++);
  for (const auto &a : s.dynamic_floors) inspect(a, index++);
}
}
