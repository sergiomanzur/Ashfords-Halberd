#pragma once
#include "face_model.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace face_test {
using namespace ashford;

inline void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
inline void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
inline uint32_t word(const Bytes &b, size_t p) {
  uint32_t v = 0;
  for (size_t i = 0; i < 4; ++i)
    v |= uint32_t(b.at(p + i)) << (8 * i);
  return v;
}
inline void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
inline void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
inline float value(const Bytes &b, size_t p) {
  auto v = word(b, p);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}
inline void near(float a, float b) {
  ck(std::abs(a - b) < 0.001f, "Facial geometry position mismatch");
}
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Malformed facial data accepted");
}
inline ActorModel fixture() {
  ActorModel a;
  auto &b = a.model;
  b.resize(16 + 12 * 52);
  b[0] = 'M';
  b[1] = 'D';
  b[2] = 'L';
  b[3] = 128;
  half(b, 4, 16);
  half(b, 6, 12);
  put(b, 8, 0xffffffff);
  for (size_t i = 0; i < 12; ++i) {
    auto p = 16 + i * 52;
    put(b, p, 6);
    put(b, p + 4, 0xffffffff);
    for (size_t axis = 0; axis < 3; ++axis)
      flt(b, p + 32 + axis * 4, 1);
    put(b, p + 44, i < 6 ? uint32_t((i + 1) * 52) : 0xffffffff);
    put(b, p + 48, 0xffffffff);
    if (i >= 1 && i <= 5)
      flt(b, p + 12, 1);
  }
  flt(b, 24, 10);
  put(b, 16 + 6 * 52 + 44, 7 * 52);
  put(b, 16 + 6 * 52 + 48, 11 * 52);
  for (size_t i = 7; i < 10; ++i)
    put(b, 16 + i * 52 + 48, uint32_t((i + 1) * 52));
  flt(b, 16 + 11 * 52 + 8, 2);
  for (size_t i :
       {size_t(0), size_t(6), size_t(7), size_t(8), size_t(9), size_t(10)}) {
    size_t d = b.size(), v = d + 24, p = v + 176;
    b.resize(p + 28);
    put(b, 16 + i * 52 + 4, uint32_t(d - 16));
    put(b, d, uint32_t(v - 16));
    put(b, d + 4, uint32_t(p - 16));
    b[v] = 51;
    half(b, v + 2, 39);
    half(b, v + 6, 3);
    // Independent meshes; face jaw anchors (0,0,0) and (1,0,0).
    flt(b, v + 64, i == 6 ? 0 : 1);
    flt(b, v + 96, 1);
    flt(b, v + 128 + 4, 1);
    b[v + 160] = 255;
    b[p] = 66;
    half(b, p + 2, 11);
    half(b, p + 4, 1);
    half(b, p + 6, 3);
    for (size_t n = 0; n < 3; ++n)
      half(b, p + 8 + n * 6, uint16_t(n));
    b[p + 26] = 255;
  }
  auto &s = a.skin;
  s = Bytes{'S', 'K', 'I', 'N'};
  auto append = [&](uint32_t v) {
    auto p = s.size();
    s.resize(p + 4);
    put(s, p, v);
  };
  append(3);
  append(0);
  append(3);
  for (int i = 0; i < 3; ++i)
    append(0x0b091234);
  for (int i = 1; i < 11; ++i)
    append(0);
  append(1);
  append(0);
  append(0xffffffff);
  return a;
}
inline Bytes mask_fixture() {
  Bytes m(64);
  m[0] = 'M';
  m[1] = 'A';
  m[2] = 'S';
  m[3] = 'K';
  half(m, 4, 1);
  half(m, 10, 1);
  m[0x34] = 1;
  m[0x35] = 3;
  m[0x36] = 2;
  for (size_t i = 0; i < 9; ++i)
    m[0x37 + i] = 255;
  m[0x37] = 4;
  m[0x38] = 5;
  return m;
}
} // namespace face_test
