#include "enemy_targeting.hpp"
#include "ninja_math.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp pwksub.c, en01b.c and zonzon.c.
namespace ashford {
namespace {
void finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite enemy targeting value");
}
void finite(Vec3 value) {
  finite(value.x);
  finite(value.y);
  finite(value.z);
}
} // namespace
int32_t search_enemy_player(Vec3 probe, int32_t yaw, Vec3 player,
                            int32_t cone) {
  finite(probe);
  finite(player);
  if (!cone)
    cone = 32768;
  const auto a = ninja_sin_cos(uint16_t(yaw));
  const auto r = ninja_sin_cos(uint16_t(cone));
  // First rotation produces vc0 from (0,0,-1). The second rotates the
  // existing matrix, retaining quarter-table rounding and multiplication.
  const float x0 = -a[0], z0 = -a[1];
  const float x2 = -(a[1] * r[0] + a[0] * r[1]);
  const float z2 = -(-a[0] * r[0] + a[1] * r[1]);
  const float threshold = -(x0 * x2 + z0 * z2);
  const Vec3 delta{probe.x - player.x, probe.y - player.y, probe.z - player.z};
  finite(delta);
  const float squared =
      (delta.x * delta.x + delta.y * delta.y) + delta.z * delta.z;
  finite(squared);
  // VU reciprocal square root of zero saturates; zero vector stays zero.
  const float inverse = squared ? 1.f / std::sqrt(squared) : 0;
  const float product = (x0 * (delta.x * inverse) + 0.f * (delta.y * inverse)) +
                        z0 * (delta.z * inverse);
  finite(product);
  if (!(product < threshold))
    return -1;
  // This subtraction and negation order preserves signed zero from source.
  const float dx = player.x - probe.x, dz = player.z - probe.z;
  const auto angle = int32_t(10430.381f * std::atan2(-dx, -dz));
  return angle == -1 ? -2 : angle;
}
int32_t enemy_direction(Vec3 origin, Vec3 target) {
  // NitenDir_ck consumes only XZ; unused Y does not affect this query.
  const float x = origin.x - target.x, z = origin.z - target.z;
  finite(x);
  finite(z);
  return int32_t(std::atan2(x, z) * 10430.381f);
}
int32_t turn_enemy_neck(Vec3 origin, Vec3 target, int32_t neck_yaw,
                        int32_t limit, int32_t actor_yaw) {
  if (limit < 0 || limit >= 32768)
    throw std::runtime_error("Invalid enemy neck limit");
  const auto rotation =
      uint16_t(uint32_t(enemy_direction(origin, target)) - uint32_t(actor_yaw));
  // The source keeps neck_yaw as int while each comparison/step explicitly
  // casts to unsigned short. Use word arithmetic to avoid signed overflow.
  if (uint16_t(uint32_t(rotation) - uint32_t(neck_yaw) + 910) < 1820) {
    if (uint16_t(rotation + limit) <= limit + limit)
      return rotation;
    return neck_yaw;
  }
  if (!rotation)
    return uint16_t(uint32_t(neck_yaw) + (neck_yaw > 32768 ? 910u : -910u));
  if (uint16_t(uint32_t(rotation) - uint32_t(neck_yaw)) < 32768) {
    int32_t result = uint16_t(uint32_t(neck_yaw) + 910);
    if (uint16_t(result - limit) < 65536 - (limit + limit))
      result = limit;
    return result;
  }
  int32_t result = uint16_t(uint32_t(neck_yaw) - 910);
  if (uint16_t(result + limit) > limit + limit)
    result = 65536 - limit;
  return result;
}
const CollisionRecord *
enemy_floor_trigger(const RoomCollision &room,
                    const std::vector<CollisionRecord> &dynamic, Vec3 position,
                    int32_t floor, uint8_t id, uint8_t action) {
  finite(position.x);
  finite(position.z);
  auto find = [&](const std::vector<CollisionRecord> &records)
      -> const CollisionRecord * {
    for (const auto &record : records) {
      if (!(record.flags & 1) || record.type != 2)
        continue;
      finite(record.position.x);
      finite(record.position.z);
      finite(record.size.x);
      finite(record.size.z);
      const float xmax = record.position.x + record.size.x;
      const float zmax = record.position.z + record.size.z;
      finite(xmax);
      finite(zmax);
      if (record.position.x <= position.x && !(xmax < position.x) &&
          record.position.z <= position.z && !(zmax < position.z) &&
          record.floor == floor && record.parameters[0] == id &&
          record.parameters[1] == action)
        return &record;
    }
    return nullptr;
  };
  if (const auto *record = find(room.floors))
    return record;
  return find(dynamic);
}
void rotate_zombie_neck(ScenarioState &state, size_t enemy, size_t neck,
                        const ActorModelWork &player, size_t player_neck) {
  if (state.enemies.size() != 128 || enemy >= state.enemies.size() ||
      !(state.enemies[enemy].flags & 1))
    throw std::runtime_error("Invalid zombie neck work slot");
  const auto &work = state.enemies[enemy];
  int32_t yaw = work.angles[1];
  if (work.flags & 0x80) {
    if (!work.linked_parent || *work.linked_parent >= state.enemies.size())
      throw std::runtime_error("Missing zombie neck parent");
    yaw = state.enemies[*work.linked_parent].angles[1];
  }
  const auto owner_slot = work.pose_owner.value_or(enemy);
  if (owner_slot >= state.enemies.size() ||
      (owner_slot != enemy && state.enemies[owner_slot].pose_owner))
    throw std::runtime_error("Invalid zombie neck model-work owner");
  auto &owner = state.enemies[owner_slot];
  if (!owner.model_work || neck >= owner.model_work->joints.size() ||
      neck >= owner.joint_pose.size() || !owner.joint_pose[neck].angles ||
      player_neck >= player.joints.size())
    throw std::runtime_error("Missing cached zombie/player neck joint");
  auto origin = [](const ActorJointWork &joint) {
    return Vec3{float(joint.matrix[3]), float(joint.matrix[7]),
                float(joint.matrix[11])};
  };
  auto &angles = *owner.joint_pose[neck].angles;
  const auto result =
      turn_enemy_neck(origin(owner.model_work->joints[neck]),
                      origin(player.joints[player_neck]), angles[1], 9102, yaw);
  angles[1] = uint16_t(result);
}
} // namespace ashford
