#pragma once
#include "ambient.hpp"
#include "vector.hpp"
#include <optional>
namespace ashford {
struct Camera;
struct CameraDefinition;
struct ScenarioState;
struct ScenarioEntity;
// Explicit native LGT_WORK fields. Disk pointers never become host pointers.
struct LightWork {
  uint32_t flags = 0;
  int32_t type = 0, angular_speed = 0;
  uint32_t link = 0, link_index = 0, link_joint = 0, source = 0;
  Vec3 position{}, local{}, vector{};
  std::array<float, 3> accumulators{}, color{};
  float near_range = 0, far_range = 0;
  std::array<int32_t, 5> angles{};
  int32_t mode = 0;
  std::array<int32_t, 4> counters{};
  Vec3 work_position{}, work_vector{};
  std::array<float, 3> work_accumulators{}, work_color{};
  float work_near = 0, work_far = 0;
  std::array<int32_t, 5> work_angles{};
};
struct RoomLights {
  std::vector<LightWork> normal, events;
};
struct NativeDirectionalLight {
  Vec3 vector{};
  std::array<float, 3> color{};
};
struct NativePointLight {
  Vec3 position{};
  std::array<float, 3> color{};
  float near_range = 0, far_range = 0;
};
struct NativeLighting {
  bool has_direction = false;
  NativeDirectionalLight direction;
  std::array<NativePointLight, 3> points{};
  size_t point_count = 0;
  Vec3 single_vector{}; // Already negated, camera space, as Ninja stores it.
  std::array<float, 3> single_color{};
  float single_intensity = 0, single_ambient = 0;
};
struct LightMaterial {
  std::array<uint8_t, 4> diffuse{255, 255, 255, 255};
  std::array<uint8_t, 3> ambient{255, 255, 255}, specular{255, 255, 255};
  uint8_t exponent = 17;
  uint64_t gs_alpha = 0x44;
};
struct LightMaterialUpdate {
  std::optional<std::array<uint8_t, 4>> diffuse;
  std::optional<std::array<uint8_t, 3>> ambient, specular;
  std::optional<uint8_t> exponent;
  std::optional<uint64_t> gs_alpha;
};
RoomLights decode_room_lights(const Bytes &);
void initialize_room_lights(RoomLights &);
void select_light_cut(RoomLights &, const CameraDefinition &);
NativeLighting control_lights(RoomLights &, ScenarioState &,
                              const ScenarioEntity *, const Camera &,
                              const CameraDefinition &, uint32_t camera_flags);
NativeLighting drawing_lights(RoomLights &, const NativeLighting &, bool easy,
                              bool half);
Vec3 light_direction(std::array<int32_t, 3> angles);
void apply_light_material(LightMaterial &, const LightMaterialUpdate &);
uint64_t light_blend_register(uint8_t material_flags);
std::array<uint8_t, 4> light_vertex_color(const NativeLighting &,
                                          const Camera &, Vec3 position,
                                          Vec3 normal, const LightMaterial &,
                                          const std::array<float, 3> &ambient,
                                          uint8_t draw_mode,
                                          uint8_t strip_flags, bool textured);
std::array<float, 4> light_vertex_rgba(const NativeLighting &, const Camera &,
                                       Vec3 position, Vec3 normal,
                                       const LightMaterial &,
                                       const std::array<float, 3> &ambient,
                                       uint8_t draw_mode, uint8_t strip_flags,
                                       bool textured);
} // namespace ashford
