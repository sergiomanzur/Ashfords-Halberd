#pragma once
#include "motion.hpp"
namespace ashford {
struct SkinWeight {
  uint8_t joint = 0;
  float rotation_weight = 1;
  uint16_t auxiliary = 0;
};
struct SkinNode {
  uint32_t flags = 0, group = 0;
  std::vector<SkinWeight> vertices;
};
std::vector<SkinNode> decode_skin(const Bytes &data, size_t object_count);
struct ActorModel {
  Bytes model;
  Bytes skin;
  Bytes textures;
};
struct ActorModelBank {
  std::vector<ActorModel> models;
  Bytes mask;
};
struct RoomActorModels {
  std::vector<ActorModelBank> enemies;
  // Object slots 0-3 belong to player attachments, not room asset pointers.
  std::vector<ActorModel> objects, items;
};
RoomActorModels decode_room_actor_models(const Bytes &data);
ActorModelBank decode_actor_model_bank(const Bytes &data, size_t begin,
                                       size_t end);
// Authored enemy order, including empty banks for inactive enemies.
std::vector<ActorModelBank> decode_room_enemy_models(const Bytes &data);
std::vector<JointPose> actor_bind_pose(const ActorModel &actor);
Scene pose_actor(const ActorModel &actor, const std::vector<JointPose> &pose,
                 Vec3 position = {}, uint16_t yaw = 0);
Scene pose_actor_rotated(const ActorModel &actor,
                         const std::vector<JointPose> &pose, Vec3 position,
                         std::array<int32_t, 3> angles);
struct PlayerPackage {
  std::vector<ActorModel> models;
  std::vector<std::optional<Motion>> motions;
  Bytes auxiliary;
};
PlayerPackage decode_player_package(const Bytes &data,
                                    size_t key_limit = 2000000);
} // namespace ashford
