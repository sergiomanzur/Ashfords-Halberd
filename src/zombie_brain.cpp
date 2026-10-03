#include "enemy_navigation_internal.hpp"
#include "room.hpp"
#include "zombie.hpp"
#include "zombie_awareness.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::Brain00/KaidanCheck.
namespace ashford {
namespace {
void finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite zombie pursuit value");
}
std::optional<size_t> stairs(const ScenarioState &state, size_t work,
                             const ScenarioEntity &player) {
  if ((player.state_flags & 0x80000000) || (player.flags & 4))
    return std::nullopt;
  for (size_t i = 0; i < state.enemies.size(); ++i) {
    const auto &other = state.enemies[i];
    if (i != work && other.id == 1 && (other.flags & 1) && !(other.flags & 2) &&
        !(other.flags & 0x80) && other.zombie_work &&
        (other.zombie_flags & 0x400000))
      return std::nullopt;
  }
  if (!state.collision)
    throw std::runtime_error("Missing zombie stair collision context");
  const auto &enemy = state.enemies[work];
  const auto index =
      enemy_stair_trigger(*state.collision, state.dynamic_miscellaneous,
                          enemy.position, enemy.angles[1], enemy.floor, 1);
  if (!index)
    return std::nullopt;
  const auto &room = state.collision->miscellaneous;
  const auto &record = *index < room.size()
                           ? room[*index]
                           : state.dynamic_miscellaneous[*index - room.size()];
  if (record.parameters[2] < 4)
    return std::nullopt;
  if ((record.parameters[0] == 0 && player.floor > enemy.floor) ||
      (record.parameters[0] != 0 && player.floor < enemy.floor))
    return index;
  return std::nullopt;
}
} // namespace
void tick_zombie_brain(ScenarioState &state, size_t work,
                       const ScenarioEntity &player) {
  const auto sensed =
      detail::evaluate_zombie_awareness(state, work, player.position);
  auto &enemy = state.enemies[work];
  auto &extended = *enemy.zombie_work;
  auto flags = sensed.flags;
  auto route_id = extended.route_id, route_timer = extended.route_timer;
  auto target_x = extended.target_x, target_z = extended.target_z;
  auto modes = enemy.modes;
  auto require_routes = [&]() -> const RoomRoutes & {
    if (!state.routes)
      throw std::runtime_error("Missing current-room zombie route data");
    return *state.routes;
  };
  auto route = [&] {
    const auto &routes = require_routes();
    // Source leaves its local pos undefined when either region is missing.
    // Keep shared-region255 output, but reject that undefined path explicitly.
    if (room_route_id(routes, enemy.position) == 255 ||
        room_route_id(routes, player.position) == 255)
      throw std::runtime_error(
          "Zombie pursuit outside defined room route regions");
    Vec3 target;
    route_id = room_route(routes, enemy.position, player.position, target);
    finite(target.x);
    finite(target.z);
    target_x = target.x;
    target_z = target.z;
    route_timer = 0;
  };
  if (enemy.type == 5)
    route();
  else if (flags & 0x400) {
    finite(extended.player_distance);
    if (extended.player_distance < 50 && !extended.path_blocked &&
        enemy.floor == player.floor) {
      finite(player.position.x);
      finite(player.position.z);
      target_x = player.position.x;
      target_z = player.position.z;
      route_id = room_route_id(require_routes(), enemy.position);
      route_timer = 0;
    } else {
      const auto current_id = room_route_id(require_routes(), enemy.position);
      ++route_timer;
      if (route_id == current_id || route_timer >= 255)
        route();
    }
  }
  const auto stair_index = stairs(state, work, player);
  std::optional<detail::StairPair> pair;
  if (stair_index) {
    pair = detail::stair_pair(state, *stair_index);
    if (!(enemy.flags & 0x200000)) {
      modes[1] = 0;
      modes[2] = 8;
      modes[3] = 0;
      flags |= 0x400000;
    } else
      flags &= ~0x400;
  }
  // Only validated scalar work and the selected stair pair change.
  extended.awareness = sensed.awareness;
  extended.awareness_timeout = sensed.timeout;
  extended.route_id = route_id;
  extended.route_timer = route_timer;
  extended.target_x = target_x;
  extended.target_z = target_z;
  enemy.zombie_flags = flags;
  enemy.modes = modes;
  if (pair) {
    extended.stair_target = stair_index;
    enemy.stair_index = stair_index;
    if (enemy.flags & 0x200000) {
      pair->first->attribute &= ~0x400000;
      pair->second->attribute &= ~0x400000;
    } else {
      pair->first->attribute |= 0x400000;
      pair->second->attribute |= 0x400000;
    }
  }
}
} // namespace ashford
