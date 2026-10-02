#pragma once
#include "scene.hpp"
#include <optional>
namespace ashford {
struct MotionTrack {
  std::vector<Vec3> positions;
  std::vector<std::array<uint16_t, 3>> angles;
};
struct Motion {
  uint8_t flags = 0;
  uint32_t frames = 0;
  std::vector<MotionTrack> tracks;
  std::vector<uint16_t> attributes;
};
struct JointPose {
  std::optional<Vec3> position;
  std::optional<std::array<uint16_t, 3>> angles;
};
Motion decode_motion(const Bytes &data, size_t key_limit = 2000000);
// A bounded length-prefixed motion stream, terminated by 0xffffffff.
// Empty records preserve authored motion indices. Padding after the sentinel
// belongs to the containing file and is not interpreted as another record.
std::vector<std::optional<Motion>>
decode_motion_bank(const Bytes &data, size_t begin, size_t end,
                   size_t key_limit = 2000000);
// The room bank follows the per-enemy pointers in RDX header section 24.
// Runtime storage has 512 slots, including null slots for absent records.
std::vector<std::optional<Motion>>
decode_room_motion_bank(const Bytes &data, size_t key_limit = 2000000);
// Original 16.16 frame position; optional SetMtnSlow interpolation.
std::vector<JointPose> sample_motion(const Motion &motion, int64_t frame_fixed,
                                     bool interpolate = false);
} // namespace ashford
