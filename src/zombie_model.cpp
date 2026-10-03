#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::bhEne01_CalcEnemy.
namespace ashford {
namespace {
size_t owner(const ScenarioState &state, size_t work) {
  const auto index = state.enemies[work].pose_owner.value_or(work);
  if (index >= state.enemies.size() || !(state.enemies[index].flags & 1) ||
      (index != work && state.enemies[index].pose_owner) ||
      !state.enemies[index].model_work)
    throw std::runtime_error("Missing CalcEnemy model-work owner");
  return index;
}
size_t parent(const ScenarioState &state, size_t work) {
  const auto index = state.enemies[work].linked_parent;
  if (!index || *index >= state.enemies.size() || *index == work ||
      !(state.enemies[*index].flags & 1))
    throw std::runtime_error("Missing CalcEnemy parent work");
  return *index;
}
Vec3 origin(const ModelMatrix &matrix) {
  return {detail::ninja_finite(float(matrix[3])),
          detail::ninja_finite(float(matrix[7])),
          detail::ninja_finite(float(matrix[11]))};
}
struct CacheUpdate {
  size_t owner;
  ActorModelWork work;
};
struct BaseUpdate {
  size_t actor;
  ModelMatrix matrix;
};
} // namespace
void calculate_zombie_model(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid CalcEnemy work pool");
  auto &enemy = state.enemies[work];
  if (!(enemy.flags & 1) || enemy.id != 1 || !enemy.zombie_work)
    throw std::runtime_error("Missing CalcEnemy zombie extended work");
  std::vector<CacheUpdate> caches;
  std::vector<BaseUpdate> bases;
  caches.reserve(2);
  bases.reserve(2);
  auto cache = [&](size_t actor) -> const ActorModelWork & {
    const auto index = owner(state, actor);
    for (const auto &update : caches)
      if (update.owner == index)
        return update.work;
    return *state.enemies[index].model_work;
  };
  auto joint = [&](size_t actor, size_t index) -> const ModelMatrix & {
    const auto &selected = cache(actor);
    if (index >= selected.joints.size())
      throw std::runtime_error("Missing CalcEnemy cached joint");
    return selected.joints[index].matrix;
  };
  auto calculate = [&](size_t actor_index, bool suffix) {
    const auto &actor = state.enemies[actor_index];
    const auto index = owner(state, actor_index);
    const auto &posed = state.enemies[index];
    auto calculated = cache(actor_index);
    if (!posed.model || posed.joint_pose.size() != calculated.joints.size() ||
        posed.joint_pose.size() < 18 || calculated.first_joint != 0 ||
        (calculated.joint_count &&
         *calculated.joint_count != calculated.joints.size()))
      throw std::runtime_error(
          "CalcEnemy requires full initialized model work");
    const auto old_base = calculated.base_matrix;
    const auto old_first = calculated.first_joint;
    const auto old_count = calculated.joint_count;
    ActorPlacement placement;
    placement.flags = actor.flags;
    placement.position = actor.position;
    placement.angles = actor.angles;
    if (actor.flags & 0x80) {
      if (actor.linked_joint < 0)
        throw std::runtime_error("Invalid CalcEnemy linked joint");
      placement.parent_joint =
          joint(parent(state, actor_index), size_t(actor.linked_joint));
      placement.linked_offset = actor.linked_offset;
    }
    if (actor.model_matrix)
      calculated.base_matrix = *actor.model_matrix;
    else if (index != actor_index) {
      if ((actor.flags & 0x1000) && !(actor.flags & 0x80))
        throw std::runtime_error("Missing retained CalcEnemy actor matrix");
      calculated.base_matrix = detail::ninja_store(detail::ninja_identity());
    }
    if (suffix) {
      calculated.first_joint = 8;
      calculated.joint_count = calculated.joints.size() - 8;
    }
    calculated = calculate_actor_model_work(*posed.model, posed.joint_pose,
                                            std::move(calculated), placement);
    bases.push_back({actor_index, calculated.base_matrix});
    calculated.first_joint = old_first;
    calculated.joint_count = old_count;
    // Shared ML_WORK keeps the owner's base metadata; BH_PWORK bases remain
    // independent even after the body overwrites the shared upper cache.
    if (index != actor_index)
      calculated.base_matrix = old_base;
    for (auto &update : caches)
      if (update.owner == index) {
        update.work = std::move(calculated);
        return;
      }
    caches.push_back({index, std::move(calculated)});
  };
  auto geometry = enemy.geometry;
  std::optional<size_t> capsule_parent;
  EnemyGeometry parent_geometry;
  if (enemy.flags & 0x80) {
    capsule_parent = parent(state, work);
    const auto &root = state.enemies[*capsule_parent];
    if (!root.zombie_work)
      throw std::runtime_error("Missing CalcEnemy parent limb work");
    calculate(*capsule_parent, false);
    parent_geometry = root.geometry;
    if (root.zombie_work->damage_88 == -1 ||
        root.zombie_work->damage_8c == -1) {
      const auto matrix = detail::ninja_load(joint(*capsule_parent, 9));
      parent_geometry.capsule_start = detail::ninja_point(matrix, {0, 3.5f, 0});
      parent_geometry.capsule_end = detail::ninja_point(matrix, {0, -6, 0});
    } else {
      parent_geometry.capsule_start = origin(joint(*capsule_parent, 10));
      const auto a = origin(joint(*capsule_parent, 4)),
                 b = origin(joint(*capsule_parent, 7));
      parent_geometry.capsule_end = {detail::ninja_finite((a.x + b.x) / 2),
                                     detail::ninja_finite((a.y + b.y) / 2),
                                     detail::ninja_finite((a.z + b.z) / 2)};
    }
    parent_geometry.capsule_radius = 2;
  } else if (enemy.zombie_flags & 0x80000000u) {
    const auto root = parent(state, work);
    calculate(root, false);
    calculate(work, true);
    geometry.capsule_start =
        detail::ninja_point(detail::ninja_load(joint(root, 10)), {0, 3, 0});
    geometry.capsule_end = origin(joint(root, 9));
    geometry.capsule_radius = 2.5f;
  }
  auto secondary = enemy.zombie_secondary_flags;
  if (secondary & 0x200) {
    geometry.collision_radius =
        detail::ninja_finite(geometry.collision_radius + .2f);
    if (geometry.collision_radius >= 2.5f) {
      geometry.collision_radius = 2.5f;
      secondary &= ~0x200u;
    }
  }
  geometry.height = geometry.collision_height =
      detail::ninja_finite(detail::ninja_finite(float(joint(work, 10)[7])) -
                           detail::ninja_finite(enemy.position.y));
  for (auto &update : caches)
    *state.enemies[update.owner].model_work = std::move(update.work);
  for (const auto &update : bases)
    state.enemies[update.actor].model_matrix = update.matrix;
  if (capsule_parent)
    state.enemies[*capsule_parent].geometry = parent_geometry;
  enemy.geometry = geometry;
  enemy.zombie_secondary_flags = secondary;
}
} // namespace ashford
