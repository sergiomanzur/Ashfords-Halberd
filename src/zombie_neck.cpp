#include "enemy_targeting.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::bhEne01_SearchNeck.
namespace ashford {
namespace {
int32_t word(uint32_t bits) {
  return bits < 0x80000000u ? int32_t(bits)
                            : int32_t(int64_t(bits) - 0x100000000ll);
}
int32_t decay(int32_t neck) {
  if (!neck)
    return 0;
  if (neck < 32768) {
    neck = word(uint32_t(neck) - 910u);
    return neck < 0 ? 0 : neck;
  }
  neck = word(uint32_t(neck) + 910u);
  return neck >= 65536 ? 0 : neck;
}
} // namespace
void tick_zombie_neck(ScenarioState &state, size_t work,
                      const ActorModelWork &player) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie search-neck pool");
  auto &enemy = state.enemies[work];
  if (!(enemy.flags & 1) || enemy.id != 1)
    throw std::runtime_error("Invalid zombie search-neck work");
  if (enemy.modes[0] >= 5)
    return;
  if (!enemy.zombie_work)
    throw std::runtime_error("Missing zombie search-neck extended work");
  if (!(enemy.zombie_flags & 0x80000000))
    return;
  if (!enemy.linked_parent || *enemy.linked_parent >= state.enemies.size() ||
      *enemy.linked_parent == work)
    throw std::runtime_error("Missing linked zombie search-neck parent");
  const auto &parent = state.enemies[*enemy.linked_parent];
  if (!parent.zombie_work)
    throw std::runtime_error("Missing zombie search-neck parent work");
  const auto owner_index = enemy.pose_owner.value_or(work);
  if (owner_index >= state.enemies.size() ||
      (owner_index != work && state.enemies[owner_index].pose_owner))
    throw std::runtime_error("Invalid zombie search-neck pose owner");
  auto &owner = state.enemies[owner_index];
  if (owner.joint_pose.size() <= 11 || !owner.joint_pose[11].angles)
    throw std::runtime_error("Missing zombie search-neck pose");
  auto neck = enemy.zombie_work->neck_yaw;
  const auto flags = parent.zombie_flags | enemy.zombie_flags;
  const bool override = flags & 0x1000;
  if (override) {
    if (owner.joint_pose.size() <= 8 || !owner.joint_pose[8].angles)
      throw std::runtime_error("Missing zombie search-neck torso pose");
  } else if ((flags & 0x800) &&
             ((parent.zombie_work->awareness | enemy.zombie_work->awareness) &
              0x40)) {
    const auto distance = enemy.zombie_work->player_distance;
    if (!std::isfinite(distance))
      throw std::runtime_error("Nonfinite zombie search-neck distance");
    if (distance < 35) {
      if (!owner.model_work || owner.model_work->joints.size() <= 11 ||
          player.joints.size() <= 5)
        throw std::runtime_error(
            "Missing cached zombie search-neck/player joint");
      auto origin = [](const ActorJointWork &j) {
        return Vec3{float(j.matrix[3]), float(j.matrix[7]),
                    float(j.matrix[11])};
      };
      // RotNeck uses parent yaw when primary80 is set; retain raw seeded EXP50.
      const auto yaw =
          (enemy.flags & 0x80) ? parent.angles[1] : enemy.angles[1];
      neck = turn_enemy_neck(origin(owner.model_work->joints[11]),
                             origin(player.joints[5]), neck, 9102, yaw);
    } else
      neck = decay(neck);
  } else
    neck = decay(neck);
  if (override)
    (*owner.joint_pose[8].angles)[1] = enemy.torso_yaw;
  (*owner.joint_pose[11].angles)[1] = neck;
  enemy.zombie_work->neck_yaw = neck;
}
} // namespace ashford
