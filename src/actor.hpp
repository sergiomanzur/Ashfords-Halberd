#pragma once
#include "motion.hpp"
namespace ashford {
enum class SkinLayout { Standard, FaceModel };
struct SkinWeight {
  uint16_t joint = 0;
  float rotation_weight = 1;
  uint16_t auxiliary = 0;
};
struct SkinNode {
  uint32_t flags = 0, group = 0;
  std::vector<SkinWeight> vertices;
};
std::vector<SkinNode> decode_skin(const Bytes &data, size_t object_count,
                                 SkinLayout layout = SkinLayout::Standard);
struct ActorModel {
  Bytes model;
  Bytes skin;
  Bytes textures;
  // FM layout belongs to the expanded model after ordinary bind conversion.
  SkinLayout skin_layout = SkinLayout::Standard;
  bool skin_preconverted = false;
};
struct ActorModelBank {
  std::vector<ActorModel> models;
  Bytes mask;
};
struct ActorMorphModels {
  ActorModel source, target;
};
struct ActorJointWork {
  uint32_t flags = 0;
  ModelMatrix matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
struct ActorModelWork {
  ModelMatrix base_matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<ActorJointWork> joints;
  size_t first_joint = 0;
  std::optional<size_t> joint_count;
};
struct ActorPlacement {
  uint32_t flags =
      0; // BH_PWORK flags: linked 0x80, retained/reset base 0x1000.
  Vec3 position{}, linked_offset{};
  std::array<int32_t, 3> angles{};
  std::optional<ModelMatrix> parent_joint;
};
// njplus.c ordinary translation-only bind conversion, before pose/morphing.
ActorModel prepare_actor_skin(const ActorModel &actor);
// npCalcMorphing/npTransform on a private model, preserving retained assets.
// Skinned inputs must already use source bind-converted coordinates.
ActorModel morph_actor_model(const ActorModel &source, const ActorModel &target,
                            const std::vector<JointPose> &pose, float amount,
                            size_t first_joint = 0,
                            std::optional<size_t> joint_count = {});
// MdlPut.c bhCalcModel/bhCalcTree. Owns cached matrices in full model order;
// updates only the selected linear range and restores O_WORK flag-1 matrices.
ActorModelWork calculate_actor_model_work(const ActorModel &actor,
                                          const std::vector<JointPose> &pose,
                                          ActorModelWork work = {},
                                          const ActorPlacement &placement = {});
// Motion.c::bhCalcFixOffset. Signed path ends with-1; trailing padding is
// ignored. Uses complete current object poses and the prior terminal cache.
// Unlike bhCalcModel this operation ignores placement flag1000.
Vec3 calculate_actor_fix_offset(const std::vector<JointPose> &pose,
                                const ActorModelWork &cached,
                                const ActorPlacement &placement,
                                const std::vector<int8_t> &path,
                                Vec3 offset = {});
// Draw selected objects with their cached matrices and original full-model
// skin bindings. Controllers which rebase skin lists require their own path.
Scene pose_actor_model_work(const ActorModel &actor,
                            const std::vector<JointPose> &pose,
                            const ActorModelWork &work);
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
// Same validated skeleton traversal as rendering, in actor joint order.
std::vector<ModelMatrix>
actor_joint_matrices(const ActorModel &actor,
                     const std::vector<JointPose> &pose, Vec3 position = {},
                     std::array<int32_t, 3> angles = {});
// bhCalcTree baseline linked placement: parent joint, local offset, XYZ angles.
// Poses the supplied whole model; use ActorModelWork for selected ranges,
// saved matrices and the optional 0x1000 placement flags.
Scene pose_actor_linked(const ActorModel &actor,
                        const std::vector<JointPose> &pose,
                        const ModelMatrix &parent_joint, Vec3 offset = {},
                        std::array<int32_t, 3> angles = {});
struct PlayerPackage {
  std::vector<ActorModel> models;
  std::vector<std::optional<Motion>> motions;
  Bytes auxiliary;
};
PlayerPackage decode_player_package(const Bytes &data,
                                    size_t key_limit = 2000000);
} // namespace ashford
