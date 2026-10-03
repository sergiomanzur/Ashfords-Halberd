#pragma once
#include "actor.hpp"
namespace ashford {
struct FaceParameters {
  std::array<float, 32> muscle{};
  float jaw_angle = 0, jaw_translation = 0;
  Vec3 eye{}, tongue{};
};
struct FaceKeyframe {
  FaceParameters parameters;
  int16_t frame = 0, interpolation = 0;
};
// Normal MASK PARAM_WORK encoding. Lip tracks use a separate packed encoding.
std::vector<FaceKeyframe> decode_face_track(const Bytes &mask);
// Owned raw PARAM_WORK region and ordinary keys, also used by resident tracks.
Bytes face_track_records(const Bytes &mask);
std::vector<FaceKeyframe> decode_face_parameter_records(const Bytes &records);
float face_interpolation_rate(int16_t mode, float t);
FaceParameters interpolate_face_parameters(FaceParameters previous,
                                           const FaceKeyframe &a,
                                           const FaceKeyframe &b, float t,
                                           float level = 1,
                                           uint32_t muscle_mask = 0xffffffff);
FaceParameters sample_face_track(const std::vector<FaceKeyframe> &track,
                                 float frame);
} // namespace ashford
