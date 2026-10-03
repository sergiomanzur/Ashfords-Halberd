#pragma once
#include "ninja_math.hpp"
#include "scene.hpp"
#include <limits>
#include <stdexcept>
// Native float adaptation of MIT recvx-decomp ps2_NaMatrix.c. Row-major native
// storage represents the source columns; affine W components remain0/0/0/1.
namespace ashford::detail {
using NinjaMatrix = std::array<float, 16>;
inline float ninja_finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite Ninja matrix value");
  return value;
}
inline NinjaMatrix ninja_identity() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
inline NinjaMatrix ninja_load(const ModelMatrix &source) {
  NinjaMatrix result{};
  for (size_t i = 0; i < 16; ++i) {
    if (!std::isfinite(source[i]) ||
        source[i] > std::numeric_limits<float>::max() ||
        source[i] < -std::numeric_limits<float>::max())
      throw std::runtime_error("Cached Ninja matrix outside float range");
    result[i] = float(source[i]);
  }
  if (result[12] != 0 || result[13] != 0 || result[14] != 0 || result[15] != 1)
    throw std::runtime_error("Cached Ninja matrix is not affine");
  return result;
}
inline ModelMatrix ninja_store(const NinjaMatrix &matrix) {
  ModelMatrix result{};
  for (size_t i = 0; i < 16; ++i)
    result[i] = ninja_finite(matrix[i]);
  return result;
}
inline Vec3 ninja_point(const NinjaMatrix &m, Vec3 p) {
  ninja_finite(p.x);
  ninja_finite(p.y);
  ninja_finite(p.z);
  return {ninja_finite(((m[0] * p.x + m[1] * p.y) + m[2] * p.z) + m[3]),
          ninja_finite(((m[4] * p.x + m[5] * p.y) + m[6] * p.z) + m[7]),
          ninja_finite(((m[8] * p.x + m[9] * p.y) + m[10] * p.z) + m[11])};
}
inline void ninja_translate(NinjaMatrix &m, Vec3 p) {
  const auto translated = ninja_point(m, p);
  m[3] = translated.x;
  m[7] = translated.y;
  m[11] = translated.z;
}
inline void ninja_rotate(NinjaMatrix &m, std::array<int32_t, 3> angles) {
  auto rotate = [&](size_t a, size_t b, int32_t angle, bool reverse) {
    const auto sc = ninja_sin_cos(uint16_t(angle));
    const float sine = reverse ? -sc[0] : sc[0], cosine = sc[1];
    for (size_t row = 0; row < 3; ++row) {
      const float ca = m[row * 4 + a], cb = m[row * 4 + b];
      m[row * 4 + a] = ninja_finite(ca * cosine + cb * sine);
      m[row * 4 + b] = ninja_finite(ca * (-sine) + cb * cosine);
    }
  };
  rotate(0, 1, angles[2], false);
  rotate(0, 2, angles[1], true);
  rotate(1, 2, angles[0], false);
}
} // namespace ashford::detail
