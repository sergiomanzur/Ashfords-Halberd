#pragma once
#include "face_deformation.hpp"
#include "face_lipsync.hpp"
#include <memory>
namespace ashford {
struct FaceShapeTrack {
  uint8_t actor_id = 0;
  std::vector<FaceKeyframe> keys;
};
struct FaceController {
  uint32_t flags = 0;
  int32_t phase = 1, idle_count = 0, frame = 0, last_frame = 0, start_frame = 0;
  size_t mask_number = 0, lip_number = 0, lip_group = 0;
  std::shared_ptr<const std::vector<FaceKeyframe>> original;
  std::shared_ptr<const FaceShapeTrack> selected, pending;
  std::shared_ptr<const std::vector<LipShape>> lip_shapes;
  std::shared_ptr<const std::vector<Bytes>> clips;
  std::optional<size_t> selected_clip;
  LipPlayback lip;
  FaceParameters parameters;
};
struct FaceRuntime {
  PreparedFaceModel model;
  FaceDeformationData deformation;
  FaceController controller;
};
FaceRuntime
initialize_face_runtime(const ActorModel &actor, const Bytes &mask,
                        std::shared_ptr<const std::vector<Bytes>> clips = {});
bool select_face_mask(FaceController &controller,
                      const std::vector<Bytes> &masks, size_t index,
                      int32_t start_frame = 0);
bool select_face_lip(FaceController &controller, size_t index);
bool tick_face_runtime(FaceRuntime &face, std::vector<JointPose> &pose,
                       uint16_t actor_id, uint32_t &state_flags,
                       uint32_t game_flags, int loop_now = 0,
                       int loop_count = 1);
} // namespace ashford
