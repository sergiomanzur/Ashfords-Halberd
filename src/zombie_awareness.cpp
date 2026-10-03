#include "zombie_awareness.hpp"
#include "enemy_targeting.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::bhEne01_EneSearch.
namespace ashford {
detail::ZombieAwarenessUpdate
detail::evaluate_zombie_awareness(const ScenarioState &state, size_t work,
                                  Vec3 player) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie awareness work pool");
  const auto &enemy = state.enemies[work];
  if (!(enemy.flags & 1) || enemy.id != 1 || !enemy.zombie_work)
    throw std::runtime_error("Missing original zombie awareness work");
  const auto owner_index = enemy.pose_owner.value_or(work);
  if (owner_index >= state.enemies.size() ||
      (owner_index != work && state.enemies[owner_index].pose_owner))
    throw std::runtime_error("Invalid zombie awareness model-work owner");
  const auto &owner = state.enemies[owner_index];
  if (!owner.model_work || owner.model_work->joints.size() <= 11)
    throw std::runtime_error("Missing cached zombie awareness neck joint");
  const auto &matrix = owner.model_work->joints[11].matrix;
  const Vec3 probe{float(matrix[3]), 0, float(matrix[11])};
  if (!std::isfinite(probe.x) || !std::isfinite(probe.z))
    throw std::runtime_error("Nonfinite cached zombie awareness probe");
  auto awareness = enemy.zombie_work->awareness;
  auto timeout = enemy.zombie_work->awareness_timeout;
  auto flags = enemy.zombie_flags;
  if ((awareness & 0x1f) < 31) {
    if (owner.joint_pose.size() <= 11 || !owner.joint_pose[11].angles)
      throw std::runtime_error("Missing zombie awareness neck pose");
    const auto yaw = int32_t(uint32_t(enemy.angles[1]) +
                             uint32_t((*owner.joint_pose[11].angles)[1]));
    const auto cone =
        zombie_personality_at(enemy.zombie_work->personality).view_angle;
    if (search_enemy_player(probe, yaw, player, cone) != -1)
      awareness |= 0x20;
    if (awareness & 0x20) {
      awareness |= 0x40;
      awareness &= ~0x20;
    } else if ((awareness & 0x1f) == 30)
      awareness &= ~0x40;
  }
  ++awareness;
  if ((awareness & 0x1f) >= 31)
    awareness &= ~0x1f;
  if (awareness & 0x40) {
    flags |= 0x400;
    timeout = 15;
  } else if (flags & 0x400) {
    // USA ELF17a504..17a514: lbu, addiu-1, sb, andi0xff, bgez.
    // The source --EXP0_UC(0x29)<0 clear is unreachable after unsigned wrap.
    --timeout;
  }
  return {awareness, timeout, flags};
}
void tick_zombie_awareness(ScenarioState &state, size_t work, Vec3 player) {
  const auto update = detail::evaluate_zombie_awareness(state, work, player);
  auto &enemy = state.enemies[work];
  enemy.zombie_work->awareness = update.awareness;
  enemy.zombie_work->awareness_timeout = update.timeout;
  enemy.zombie_flags = update.flags;
}
} // namespace ashford
