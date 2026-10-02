#pragma once
#include "scene.hpp"
namespace ashford {
struct Camera {
  Vec3 position;
  std::array<uint32_t, 3> angles{};
  float distance = 0;
  uint32_t perspective = 8192;
  std::array<uint32_t, 16> hidden{};
};
struct CameraDefinition {
  Camera base;
  uint16_t flags = 0;
  Vec3 tracking;
  std::array<float, 4> heights{};
  std::array<int16_t, 4> rolls{};
};
struct RoomCut {
  Vec3 origin, size;
  std::array<CameraDefinition, 2> variants;
};
std::vector<RoomCut> decode_room_cuts(const Bytes &data);
// A static authored-camera snapshot at the cut midpoint. Player tracking,
// cut attributes and scripted transitions require the gameplay state.
Camera resolve_camera(const RoomCut &cut, size_t variant);
Vec3 camera_point(const Camera &camera, Vec3 point);
} // namespace ashford
