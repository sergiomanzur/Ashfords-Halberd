#include "face_parameters.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
namespace {
void ck(bool b) {
  if (!b)
    throw std::runtime_error("Face parameter check failed");
}
void near(float a, float b) { ck(std::abs(a - b) < 0.00001f); }
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected);
}
Bytes fixture() {
  // HEAD, one list with two vertex indices, one connection, jaw, tongue,
  // then two 164-byte normal shape keys. Every preceding table affects offset.
  Bytes b(64 + 4 + 8 + 20 + 8 + 8 + 328);
  b[0] = 'M';
  b[1] = 'A';
  b[2] = 'S';
  b[3] = 'K';
  put(b, 0x10, 1);
  put(b, 0x14, 1);
  put(b, 0x18, 1);
  put(b, 0x1c, 1);
  put(b, 0x20, 2);
  b[66] = 1;
  b[67] = 2;
  put(b, 68, 0);
  put(b, 72, 1);
  size_t p = 112;
  for (size_t i = 0; i < 40; ++i) {
    flt(b, p + i * 4, float(i));
    flt(b, p + 164 + i * 4, float(i + 8));
  }
  half(b, p + 160, 10);
  half(b, p + 162, 0);
  half(b, p + 164 + 160, 20);
  half(b, p + 164 + 162, 4);
  return b;
}
} // namespace
int main() {
  try {
    auto bytes = fixture();
    auto keys = decode_face_track(bytes);
    ck(keys.size() == 2 && keys[0].frame == 10 && keys[1].frame == 20);
    near(keys[0].parameters.muscle[3], 3);
    near(keys[1].parameters.jaw_angle, 40);
    near(keys[0].parameters.eye.x, 34);
    near(keys[1].parameters.tongue.z, 47);
    auto before = sample_face_track(keys, 0);
    near(before.muscle[3], 3);
    auto after = sample_face_track(keys, 21);
    near(after.muscle[3], 11);
    auto middle = sample_face_track(keys, 12.5f);
    near(middle.muscle[3], 5);
    near(middle.eye.x, 36);
    FaceParameters previous;
    previous.muscle.fill(42);
    auto scaled =
        interpolate_face_parameters(previous, keys[0], keys[1], 0.25f, 0.5f, 8);
    near(scaled.muscle[3], 2.5f);
    near(scaled.muscle[4], 42);
    near(scaled.jaw_angle, 17);
    near(scaled.jaw_translation, 17.5f);
    near(scaled.eye.x, 36);
    near(scaled.tongue.x, 19.5f);
    keys[0].interpolation = 1;
    near(sample_face_track(keys, 12.5f).muscle[3], 3.5f);
    keys[0].interpolation = 2;
    near(sample_face_track(keys, 12.5f).muscle[3], 7);
    keys[0].interpolation = 3;
    // At angle 16383, njCos reflects to quarter-table entry zero exactly.
    auto reflected =
        interpolate_face_parameters({}, keys[0], keys[1], 16383.0f / 32768.0f);
    near(reflected.muscle[3], 7);
    near(sample_face_track(keys, 12.5f).muscle[3], 4.1718440f);
    keys[0].interpolation = 4;
    near(sample_face_track(keys, 12.5f).muscle[3], 3);
    keys[0].interpolation = 77;
    near(sample_face_track(keys, 12.5f).muscle[3], 5);
    ck(bytes == fixture());
    bad([&] {
      auto b = bytes;
      b.resize(b.size() - 1);
      decode_face_track(b);
    });
    bad([&] {
      auto b = bytes;
      b[0] = 'X';
      decode_face_track(b);
    });
    bad([&] {
      auto b = bytes;
      put(b, 0x10, 0xffffffff);
      decode_face_track(b);
    });
    bad([&] {
      auto b = bytes;
      half(b, 112 + 164 + 160, 10);
      decode_face_track(b);
    });
    bad([&] {
      auto b = bytes;
      put(b, 112, 0x7fc00000);
      decode_face_track(b);
    });
    bad([&] { sample_face_track({}, 0); });
    bad([&] { sample_face_track(keys, -1); });
    bad([&] { sample_face_track(keys, INFINITY); });
    std::cout << "Facial keyframe decoding and interpolation passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
