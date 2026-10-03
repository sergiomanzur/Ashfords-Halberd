#include "zombie.hpp"
#include <stdexcept>
namespace ashford {
Scene draw_zombie_model(const ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie drawing work pool");
  const auto &enemy = state.enemies[work];
  const auto index = enemy.pose_owner.value_or(work);
  if (!(enemy.flags & 1) || enemy.id != 1 || !enemy.zombie_work ||
      index >= state.enemies.size())
    throw std::runtime_error("Missing zombie drawing work");
  const auto &owner = state.enemies[index];
  if (!(owner.flags & 1) || (index != work && owner.pose_owner) ||
      !owner.model || !owner.model_work)
    throw std::runtime_error("Missing zombie drawing owner/model cache");
  if (enemy.model_flags & 4)
    throw std::runtime_error(
        "Zombie draw-time cache recalculation requires its scheduler");
  if ((enemy.model_flags & 2) && !(state.flags[5][0] & 0x20)) {
    if (!enemy.morph_models)
      throw std::runtime_error("Missing prepared zombie morph models");
    const auto &pair = *enemy.morph_models;
    const auto morphed = morph_actor_model(
        pair.source, pair.target, owner.joint_pose, enemy.shape_count,
        owner.model_work->first_joint, owner.model_work->joint_count);
    return pose_actor_model_work(morphed, owner.joint_pose, *owner.model_work);
  }
  return pose_actor_model_work(*owner.model, owner.joint_pose,
                               *owner.model_work);
}
} // namespace ashford
