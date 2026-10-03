// Native work ownership from MIT recvx-decomp eneset.c and en01.c.
#include "room.hpp"
#include "scenario.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ashford {
std::optional<size_t> allocate_enemy_work(std::vector<ScenarioEntity> &pool,
                                          const RoomEntity &authored,
                                          const RoomCollision &collision) {
  if (pool.size() != 128)
    throw std::runtime_error("Enemy work requires the original 128-slot pool");
  const auto found =
      std::find_if(pool.begin(), pool.end(),
                   [](const auto &work) { return !(work.flags & 1); });
  if (found == pool.end())
    return std::nullopt;
  if (!std::isfinite(authored.position.x) ||
      !std::isfinite(authored.position.y) ||
      !std::isfinite(authored.position.z))
    throw std::runtime_error("Nonfinite enemy work position");
  for (float ground : collision.ground)
    if (!std::isfinite(ground))
      throw std::runtime_error("Nonfinite enemy work ground");
  ScenarioEntity replacement;
  replacement.flags = authored.flags;
  replacement.id = authored.id;
  replacement.type = authored.type;
  replacement.model_version = authored.model_version;
  replacement.position = authored.position;
  replacement.angles = {0, authored.rotation_y, 0};
  replacement.floor = player_floor_number(collision, authored.position.y);
  replacement.clip_joints = {1, -1, 0, 0, 0, 0, 0, 0};
  if (replacement.id > 40) {
    replacement.model_flags |= 0x20;
    replacement.flags |= 0x8000;
    if (replacement.id >= 41 && replacement.id <= 48)
      replacement.clip_joints = {0, 3, 5, 8, 12, 15, 19, -1};
    else if (replacement.id >= 91 && replacement.id <= 98)
      replacement.clip_joints = {0, 3, 5, 17, 21, 24, 28, -1};
  }
  if (replacement.id > 90)
    replacement.model_flags |= 0x100;
  // bhSetEnemy overwrites the first two clipping entries after selecting
  // the ID-specific list; the remaining entries retain that list.
  replacement.clip_joints[0] = 1;
  replacement.clip_joints[1] = -1;
  const auto slot = size_t(found - pool.begin());
  *found = std::move(replacement);
  return slot;
}
std::optional<size_t> link_zombie_enemy(ScenarioState &state, size_t parent,
                                        int32_t joint, uint16_t id) {
  if (!state.collision || parent >= state.enemies.size() || joint < 0 ||
      joint >= 64)
    throw std::runtime_error("Invalid linked zombie work context");
  const auto &root = state.enemies[parent];
  if (!(root.flags & 1) || !root.model || !root.motions)
    throw std::runtime_error("Linked zombie requires bound active parent work");
  // zob_lkmtab is allocated as ID 1, then its controller ID is replaced.
  // Applying bhSetEnemy's ID-dependent flags to the replacement ID would
  // change the original linked-work behavior.
  RoomEntity authored;
  authored.flags = 0x81;
  authored.id = 1;
  auto slot = allocate_enemy_work(state.enemies, authored, *state.collision);
  if (!slot)
    return std::nullopt;
  auto &linked = state.enemies[*slot];
  linked.id = id;
  linked.model_flags |= 0x21;
  linked.linked_parent = parent;
  linked.pose_owner = parent;
  linked.linked_joint = joint;
  linked.model = root.model;
  linked.model_bank = root.model_bank;
  linked.motions = root.motions;
  return slot;
}
} // namespace ashford
