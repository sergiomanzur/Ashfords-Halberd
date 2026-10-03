#include "zombie.hpp"
#include <stdexcept>
namespace ashford {
void tick_zombie_controller(ScenarioState &state, size_t work,
                            ScenarioEntity &player) {
  if (state.enemies.size() != 128 || work >= state.enemies.size() ||
      !(state.enemies[work].flags & 1) || state.enemies[work].id != 1)
    throw std::runtime_error("Invalid native zombie runtime work");
  for (const auto &e : state.enemies)
    if (&e == &player)
      throw std::runtime_error("Zombie runtime player aliases enemy pool");
  const auto &incoming = state.enemies[work];
  // DmgCheck's original early gate. Its consumed hit path must not be skipped
  // while native weapon damage integration remains unfinished.
  if (!(incoming.flags & 0x80) && (incoming.flags & 4) &&
      !(incoming.flags & 2) && incoming.type != 10)
    throw std::runtime_error("Unimplemented zombie runtime damage check");
  if (incoming.modes[0] > 0 && !(incoming.flags & 0x80) &&
      (incoming.type == 1 || incoming.type == 3 || incoming.type == 7) &&
      !(incoming.zombie_flags & 0x10000))
    throw std::runtime_error("Unimplemented zombie runtime linked fire birth");
  switch (incoming.modes[0]) {
  case 0:
    initialize_zombie_work(state, work);
    player.flags &= ~4u;
    break;
  case 6:
    // Source MainLoop bypasses Mode0 dispatch, retaining the rest of the frame.
    break;
  default:
    throw std::runtime_error("Unimplemented zombie runtime mode " +
                             std::to_string(incoming.modes[0]));
  }
  // MainLoop runs these even on the Init frame, after Init's own pose sample.
  tick_zombie_player_grab(state, work, player);
  tick_zombie_motion(state, work, native_zombie_motion_effects());
  const ActorModelWork absent;
  tick_zombie_neck(state, work,
                   player.model_work ? *player.model_work : absent);
  auto &enemy = state.enemies[work];
  enemy.flags &= ~4u;
  auto &extended = *enemy.zombie_work;
  const uint8_t delay = uint8_t(extended.attack_delay);
  if (delay) {
    const auto next = uint8_t(delay - 1);
    extended.attack_delay = next < 128 ? int8_t(next) : int8_t(int(next) - 256);
  }
  if (extended.frame_timer_30 > 0)
    --extended.frame_timer_30;
  if (extended.frame_timer_34 > 0)
    --extended.frame_timer_34;
  tick_zombie_collision(state, work, player);
  if (state.enemies[work].zombie_flags & 0x80000)
    link_zombie_player(state, work, player);
  calculate_zombie_model(state, work);
}
} // namespace ashford
