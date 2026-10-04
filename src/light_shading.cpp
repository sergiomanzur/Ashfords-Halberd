#include "camera.hpp"
#include "light.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ashford {
namespace {
float finite(float f) {
  if (!std::isfinite(f))
    throw std::runtime_error("Nonfinite native light contribution");
  return f;
}
Vec3 view_vector(const Camera &camera, Vec3 v) {
  Camera rotation = camera;
  rotation.position = {};
  rotation.distance = 0;
  auto p = camera_point(rotation, v);
  p.z = -p.z;
  return p; // Original cam.mtx, before screen depth flip.
}
Vec3 view_point(const Camera &camera, Vec3 v) {
  auto p = camera_point(camera, v);
  p.z = -p.z;
  return p;
}
float dot(Vec3 a, Vec3 b) {
  return finite((a.x * b.x + a.y * b.y) + a.z * b.z);
}
float clamp(float v) { return std::clamp(v, 0.f, 1.f); }
float power17(float value, float result = 1) {
  for (int i = 0; i < 17; ++i)
    result *= value;
  // VU saturates overflow. Clamping the final contribution gives that result.
  if (std::isnan(result))
    throw std::runtime_error("Invalid native specular contribution");
  return clamp(result);
}
} // namespace
uint64_t light_blend_register(uint8_t flags) {
  // MIT ps2_NinjaCnk.c mode tables and ps2_NaSystem.c alpha_tbl.
  constexpr int src[8] = {11, 10, 3, 5, 8, 6, 2, 4},
                dst[8] = {11, 10, 9, 7, 8, 6, 2, 4};
  const int a = src[(flags >> 3) & 7], b = dst[flags & 7];
  if (a == 2 && b == 7)
    return 0x8000000068ull;
  if (a == 5 && b == 11)
    return 0x1000000026ull;
  if ((a == 8 && (b == 3 || b == 6)) || (a == 9 && b == 6))
    return 0x44;
  if ((a == 8 || a == 9) && b == 10)
    return 0x48;
  if (a == 11 && (b == 3 || b == 6 || b == 10))
    return 0x46;
  if (a == 11 && b == 11)
    return 0;
  throw std::runtime_error("Unsupported source Ninja alpha mode combination");
}
void apply_light_material(LightMaterial &m, const LightMaterialUpdate &u) {
  if (u.diffuse)
    m.diffuse = *u.diffuse;
  if (u.ambient)
    m.ambient = *u.ambient;
  if (u.specular)
    m.specular = *u.specular;
  if (u.exponent)
    m.exponent = *u.exponent;
  if (u.gs_alpha)
    m.gs_alpha = *u.gs_alpha;
}
std::array<float, 4>
light_vertex_rgba(const NativeLighting &lights, const Camera &camera,
                  Vec3 position, Vec3 normal, const LightMaterial &material,
                  const std::array<float, 3> &ambient, uint8_t mode,
                  uint8_t flags, bool textured) {
  if (mode > 3 || lights.point_count > 3)
    throw std::runtime_error("Invalid native Ninja lighting mode");
  const bool multi = mode == 1 || mode == 3;
  const auto n = view_vector(camera, normal);
  std::array<float, 3> intensity{};
  auto accumulate = [&](float scale, const std::array<float, 3> &rgb) {
    for (size_t c = 0; c < 3; ++c)
      intensity[c] = finite(intensity[c] + finite(scale * finite(rgb[c])));
  };
  if (multi) {
    if (lights.has_direction) {
      auto v = view_vector(camera, lights.direction.vector);
      v = {-v.x, -v.y, -v.z};
      accumulate(std::max(0.f, dot(n, v)), lights.direction.color);
    }
    const auto p = view_point(camera, position);
    for (size_t i = 0; i < lights.point_count; ++i) {
      const auto &l = lights.points[i];
      const auto q = view_point(camera, l.position);
      const Vec3 delta{q.x - p.x, q.y - p.y, q.z - p.z};
      const float d2 = dot(delta, delta),
                  near = finite(l.near_range * l.near_range),
                  far = finite(l.far_range * l.far_range);
      const float diffuse = dot(delta, n);
      // VU0_CALCPOINT sign flags: equal far included;0328 is0320's delay slot.
      if (diffuse <= 0 || d2 > far)
        continue;
      float contribution = finite(diffuse / std::sqrt(d2));
      if (d2 > near)
        contribution = finite(contribution * (near / d2));
      accumulate(contribution, l.color);
    }
  } else
    accumulate(std::max(0.f, dot(n, lights.single_vector)) *
                   finite(lights.single_intensity),
               lights.single_color);
  constexpr uint8_t table[4][8] = {{9, 2, 4, 2, 6, 2, 3, 2},
                                   {9, 2, 4, 2, 6, 2, 3, 2},
                                   {10, 2, 4, 2, 7, 2, 3, 2},
                                   {8, 2, 4, 2, 5, 2, 3, 2}};
  const uint8_t function =
      textured ? table[mode][flags & 7] : ((flags & 1) ? 2 : 4);
  std::array<float, 4> result{};
  for (size_t c = 0; c < 3; ++c) {
    const float i = intensity[c];
    const float a = multi ? finite(ambient[c]) * (material.ambient[c] / 255.f)
                          : finite(lights.single_ambient);
    const float diffuse =
        128.f * ((!textured || mode >= 2) ? material.diffuse[c] / 255.f : 1.f);
    const float specular = 31.f * (multi ? 1.f : material.specular[c] / 255.f);
    float color = 0;
    switch (function) {
    case 2:
      color = 128;
      break;
    case 3:
      color = clamp(i) * diffuse;
      break;
    case 4:
      color = clamp(i + a) * diffuse;
      break;
    case 5:
      // Standard opaque VU1 and deferred VU0 both use material constants.
      color = clamp(i) * diffuse + clamp(i - 1.f) * specular;
      break;
    case 6:
      color = clamp(i) * diffuse + clamp(i + a - 1.f) * specular;
      break;
    case 7:
      color = clamp(i) * diffuse + power17(i) * specular;
      break;
    case 8:
      color = clamp(i + a) * diffuse + clamp(i - 1.f) * specular;
      break;
    case 9:
      color = clamp(i + a) * diffuse + clamp(i + a - 1.f) * specular;
      break;
    case 10:
      color = clamp(i + a) * diffuse + power17(i + a, i + a + 1.f) * specular;
      break;
    }
    result[c] = std::clamp(finite(color), 0.f, 255.f);
  }
  result[3] = 128.f * (material.diffuse[3] / 255.f);
  return result;
}
std::array<uint8_t, 4>
light_vertex_color(const NativeLighting &l, const Camera &c, Vec3 p, Vec3 n,
                   const LightMaterial &m, const std::array<float, 3> &a,
                   uint8_t mode, uint8_t flags, bool textured) {
  const auto color = light_vertex_rgba(l, c, p, n, m, a, mode, flags, textured);
  std::array<uint8_t, 4> result{};
  for (size_t i = 0; i < 4; ++i)
    result[i] = uint8_t(color[i]);
  return result;
}
} // namespace ashford
