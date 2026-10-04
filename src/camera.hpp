#pragma once
#include "scene.hpp"
#include <optional>
namespace ashford {
struct Camera {
  Vec3 position;
  std::array<uint32_t, 3> angles{};
  float distance = 0;
  uint32_t perspective = 8192;
  std::array<uint32_t, 16> hidden{};
  std::optional<FogParameters> fog;
};
struct CameraDefinition {
  Camera base;
  uint16_t flags = 0;
  Vec3 tracking;
  std::array<float, 4> heights{};
  std::array<int16_t, 4> rolls{};
  FogParameters fog;
  uint8_t light_clip = 0;
  std::array<uint32_t, 8> light_hidden{};
};
struct CutArea {
  uint32_t attribute = 0;
  int8_t floor = 0;
  uint8_t type = 0;
  float height = 0, min_x = 0, min_z = 0, max_x = 0, max_z = 0;
};
struct RoomCut {
  Vec3 origin, size;
  std::array<CameraDefinition, 2> variants;
  uint8_t flags = 0;
  std::vector<CutArea> areas;
};
std::vector<RoomCut> decode_room_cuts(const Bytes &data);
// A static authored-camera snapshot at the cut midpoint. Player tracking,
// cut attributes and scripted transitions require the gameplay state.
Camera resolve_camera(const RoomCut &cut, size_t variant);
Vec3 camera_point(const Camera &camera, Vec3 point);
std::optional<size_t> find_camera_cut(const std::vector<RoomCut> &cuts, float x,
                                      float z, int8_t floor);
std::optional<size_t> select_camera_cut(const std::vector<RoomCut> &cuts,
                                        std::optional<size_t> current, float x,
                                        float z, int8_t floor,
                                        bool force = false);
} // namespace ashford
