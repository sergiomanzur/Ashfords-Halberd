#pragma once
#include "face_parameters.hpp"
namespace ashford {
struct LipShape {
  std::array<float, 32> muscle{};
  int32_t jaw_angle = 0, tongue_x = 0, tongue_y = 0;
  float jaw_translation = 0, tongue_z = 0;
  int16_t interpolation = 0;
};
struct LipSegment {
  uint8_t shape = 0, duration = 0;
};
struct LipPlayback {
  std::vector<LipSegment> segments;
  size_t cursor = 0;
  int16_t count = 0, time = 0;
  uint8_t current = 0, next = 0;
  bool active = false;
};
// Contiguous 164-byte PARAM_WORK records; angle fields contain signed words.
std::vector<LipShape> decode_lip_shapes(const Bytes &bytes);
// bhSetRoom/bhSetLip: indexed resident records, including empty entries.
std::vector<Bytes> decode_room_lip_clips(const Bytes &room);
// Own a bounded stream of shape/duration pairs terminated by 0xff.
LipPlayback start_lip_playback(const Bytes &clip, size_t shape_count);
// face_bh.c fmSetLipSyncParam; leaves both arguments intact on failure.
void tick_lip_playback(FaceParameters &parameters,
                       const std::vector<LipShape> &shapes,
                       LipPlayback &playback,
                       uint32_t muscle_mask = 0x03fff000);
} // namespace ashford
