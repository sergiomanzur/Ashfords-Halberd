#include "runtime.hpp"
#include <cmath>
#include <stdexcept>
namespace ashford {
bool examine_room_interaction(RoomRuntime &runtime) {
  const auto &world = runtime.world;
  const auto &player = world.player;
  auto &state = runtime.scenario;
  std::array<Vec3, 5> probes{
      player_add_speed(world.front_anchor, player.yaw,
                       1.5f + (3.f - world.front_offset)),
      player_add_speed(player.position, player.yaw, .75f),
      player_add_speed(player.position, player.yaw, 4.5f),
      player_add_speed(world.front_anchor, player.yaw,
                       3.f + (3.f - world.front_offset)),
      player_add_speed(player.position, player.yaw, 2.f)};
  state.flags[10][0] &= ~0x100u;
  constexpr uint32_t directions[4] = {0x1000, 0x800, 0x400, 0x2000};
  for (size_t index = 0; index < state.interactions.size(); ++index) {
    const auto &trigger = state.interactions[index];
    if (!(trigger.flags & 1))
      continue;
    if (trigger.type >= probes.size())
      throw std::runtime_error("Interaction probe type outside original table");
    float right = trigger.position.x + trigger.size.x,
          back = trigger.position.z + trigger.size.z;
    if (!std::isfinite(trigger.position.x) ||
        !std::isfinite(trigger.position.z) || !std::isfinite(right) ||
        !std::isfinite(back))
      throw std::runtime_error("Nonfinite interaction extent");
    const auto &probe = probes[trigger.type];
    if (world.floor != trigger.floor || probe.x < trigger.position.x ||
        probe.x > right || probe.z < trigger.position.z || probe.z > back)
      continue;
    if (index > 255)
      throw std::runtime_error(
          "Interaction index outside script-addressable range");
    state.flags[10][0] |= 0x100;
    state.interaction_index = uint8_t(index);
    if (trigger.type == 0 || trigger.type == 3 || trigger.type == 4) {
      uint16_t angle = uint16_t(player.yaw + 8192);
      if (trigger.attribute & directions[angle >> 14]) {
        state.flags[10][0] &= ~0x100u;
        continue;
      }
    }
    switch (trigger.type) {
    case 0:
      if (!(state.flags[11][0] & 9)) {
        state.player_state_flags |= 0x80000000;
        state.flags[10][0] |= 1;
        state.flags[5][0] |= 4;
        state.door = ScenarioDoor{uint16_t(trigger.attribute),
                                  trigger.parameters[0], trigger.parameters[1],
                                  trigger.parameters[2], trigger.parameters[3]};
      }
      return true;
    case 3:
      if (!(state.flags[5][0] & 0x80) && (trigger.attribute & 0x8009))
        throw std::runtime_error(
            "Examination camera/message interface is pending");
      return true;
    case 4: {
      if (trigger.attribute & 3)
        throw std::runtime_error(
            "Kneeling pickup/item-box controller is pending");
      if (trigger.parameters[0] >= state.items.size())
        throw std::runtime_error("Pickup binding outside room items");
      const auto &item = state.items[trigger.parameters[0]];
      if (!(item.flags & 1))
        throw std::runtime_error(
            "Inactive pickup fallback requires item-box controller");
      if (trigger.attribute & 0x10)
        state.flags[10][0] |= 0x20000;
      state.selected_item_id = item.id;
      state.flags[5][0] &= ~4u;
      state.flags[10][0] |= 0x10;
      return true;
    }
    default:
      throw std::runtime_error("Stair/ledge interaction controller is pending");
    }
  }
  return false;
}
} // namespace ashford
