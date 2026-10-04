#pragma once
#include "ambient.hpp"
#include "assets.hpp"
#include "fog.hpp"
#include <array>
#include <optional>
namespace ashford {
struct Vec3 {
  float x = 0, y = 0, z = 0;
};
struct Corner {
  Vec3 position;
  float u = 0, v = 0;
};
struct Triangle {
  std::array<Corner, 3> corners;
  uint16_t texture = 0;
  std::array<uint8_t, 4> color{255, 255, 255, 255};
  std::optional<AmbientMaterial> ambient_material;
};
struct ModelBounds {
  Vec3 center;
  float radius = 0;
};
struct Mesh {
  uint32_t object = 0;
  std::vector<Triangle> triangles;
  bool initially_hidden = false;
  AmbientTarget ambient_target = AmbientTarget::Room;
  std::optional<AmbientMaterial> ending_ambient_material;
  std::optional<ModelBounds> bounds;
  bool model_clipping = false;
  bool fog_enabled = true;
  std::optional<bool> fog_state;
};
struct Texture {
  uint32_t width = 0, height = 0;
  Bytes rgba;
};
using ModelMatrix = std::array<double, 16>;
// Optional native matrices override disk transforms for posed actors.
std::vector<Mesh>
decode_model(const Bytes &data, size_t offset, bool include_hidden = false,
             const std::vector<ModelMatrix> *matrices = nullptr);
// MdlPut drawing walks a contiguous object range, independently of hierarchy.
std::vector<Mesh> decode_model_range(const Bytes &data, size_t offset,
                                     const std::vector<ModelMatrix> &matrices,
                                     size_t first_object, size_t object_count);
Texture decode_tim2(const Bytes &data, size_t offset, size_t size);
std::vector<Texture> decode_texture_pack(const Bytes &data, size_t offset);
struct Scene {
  std::vector<Mesh> meshes;
  std::vector<Texture> textures;
  std::optional<RoomAmbient> ambient;
  std::optional<NativeFog> fog;
  std::optional<bool> ending_fog_state;
};
Scene decode_room_scene(const Bytes &data);
} // namespace ashford
