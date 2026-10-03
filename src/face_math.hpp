#pragma once
#include "ninja_math.hpp"
#include "scene.hpp"
#include <cmath>
#include <stdexcept>
namespace ashford::face_math {
inline ModelMatrix identity() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
inline ModelMatrix multiply(const ModelMatrix &a, const ModelMatrix &b) {
  ModelMatrix result{};
  for (size_t i = 0; i < 4; ++i)
    for (size_t j = 0; j < 4; ++j) {
      float sum = 0;
      for (size_t k = 0; k < 4; ++k)
        sum += float(a[i * 4 + k]) * float(b[k * 4 + j]);
      result[i * 4 + j] = sum;
    }
  return result;
}
inline ModelMatrix translation(Vec3 p) {
  auto m = identity();
  m[3] = p.x;
  m[7] = p.y;
  m[11] = p.z;
  return m;
}
inline ModelMatrix rotation(int axis, uint16_t angle) {
  auto m = identity();
  if (!angle)
    return m;
  auto sc = ninja_sin_cos(angle);
  int i = (axis + 1) % 3, j = (axis + 2) % 3;
  m[i * 4 + i] = sc[1];
  m[j * 4 + j] = sc[1];
  m[i * 4 + j] = -sc[0];
  m[j * 4 + i] = sc[0];
  return m;
}
inline ModelMatrix scaling(Vec3 p) {
  auto m = identity();
  m[0] = p.x;
  m[5] = p.y;
  m[10] = p.z;
  return m;
}
inline Vec3 point(const ModelMatrix &m, Vec3 p) {
  Vec3 r{
      float(m[0]) * p.x + float(m[1]) * p.y + float(m[2]) * p.z + float(m[3]),
      float(m[4]) * p.x + float(m[5]) * p.y + float(m[6]) * p.z + float(m[7]),
      float(m[8]) * p.x + float(m[9]) * p.y + float(m[10]) * p.z +
          float(m[11])};
  if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.z))
    throw std::runtime_error("Facial matrix transform overflow");
  return r;
}
} // namespace ashford::face_math
