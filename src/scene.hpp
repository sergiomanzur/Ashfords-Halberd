#pragma once
#include "assets.hpp"
#include <array>
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
};
struct Mesh {
  uint32_t object = 0;
  std::vector<Triangle> triangles;
  bool initially_hidden = false;
};
struct Texture {
  uint32_t width = 0, height = 0;
  Bytes rgba;
};
using ModelMatrix = std::array<double,16>;
// Optional native matrices override disk transforms for posed actors.
std::vector<Mesh> decode_model(const Bytes &data, size_t offset,
                               bool include_hidden = false,
                               const std::vector<ModelMatrix>* matrices = nullptr);
Texture decode_tim2(const Bytes &data, size_t offset, size_t size);
std::vector<Texture> decode_texture_pack(const Bytes &data, size_t offset);
struct Scene {
  std::vector<Mesh> meshes;
  std::vector<Texture> textures;
};
Scene decode_room_scene(const Bytes &data);
} // namespace ashford
