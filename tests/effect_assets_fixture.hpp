#pragma once
#include "effect_assets.hpp"
#include <algorithm>
#include <cstring>
#include <vector>
namespace effect_assets_fixture {
using ashford::Bytes;
inline void word(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
inline void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
inline void real(Bytes &b, size_t p, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, 4);
  word(b, p, bits);
}
inline void record(Bytes &b, uint32_t magic, const Bytes &data) {
  const auto p = b.size();
  b.resize(p + 32);
  word(b, p, magic);
  word(b, p + 4, uint32_t(data.size()));
  b.insert(b.end(), data.begin(), data.end());
}
inline void terminal(Bytes &b) {
  const auto p = b.size();
  b.resize(p + 4);
  word(b, p, 0xffffffff);
}
inline Bytes model() {
  Bytes b(68);
  word(b, 0, 0x804c444d);
  half(b, 4, 16);
  half(b, 6, 1);
  word(b, 8, 0xffffffff);
  word(b, 16, 6);
  word(b, 20, 0xffffffff);
  for (size_t p : {size_t(48), size_t(52), size_t(56)})
    real(b, p, 1);
  word(b, 60, 0xffffffff);
  word(b, 64, 0xffffffff);
  return b;
}
inline Bytes image(bool shared, uint32_t index, uint8_t red) {
  const size_t prefix = shared ? 128 : 16, header = shared ? 128 : 48;
  const size_t colors = shared ? 128 : 64;
  Bytes b(prefix + header + 1 + colors);
  word(b, 0, 0x324d4954);
  b[4] = 4;
  b[5] = shared ? 1 : 0;
  half(b, 6, 1);
  word(b, 8, index);
  b[12] = 6;
  word(b, prefix, uint32_t(b.size() - prefix));
  word(b, prefix + 4, uint32_t(colors));
  word(b, prefix + 8, 1);
  half(b, prefix + 12, uint16_t(header));
  half(b, prefix + 14, 16);
  b[prefix + 16] = shared ? 5 : 0;
  b[prefix + 18] = shared ? 3 : 0x83;
  b[prefix + 19] = 4;
  half(b, prefix + 20, 1);
  half(b, prefix + 22, 1);
  for (size_t i = 0; i < colors / 4; ++i) {
    const auto p = prefix + header + 1 + i * 4;
    b[p] = uint8_t(red + i);
    b[p + 1] = 40;
    b[p + 2] = 60;
    b[p + 3] = shared ? 128 : 64;
  }
  return b;
}
inline Bytes room_pack(uint32_t index = 3000, uint8_t red = 160,
                       uint32_t descriptor = 0xff00) {
  Bytes b, metadata(12);
  word(metadata, 0, 1);
  word(metadata, 8, descriptor);
  record(b, 0x00494c50, metadata);
  record(b, 0x324d4954, image(true, index, red));
  terminal(b);
  return b;
}
inline Bytes global(size_t textures = 40) {
  Bytes b;
  for (size_t i = 0; i < 4; ++i) {
    const auto p = b.size();
    const auto mdl = model();
    b.resize(p + 4);
    word(b, p, uint32_t(mdl.size()));
    b.insert(b.end(), mdl.begin(), mdl.end());
  }
  terminal(b); // Original cursor reads this unused next size, then aligns.
  b.resize((b.size() + 31) & ~size_t(31));
  for (size_t i = 0; i < textures; ++i)
    record(b, 0x324d4954, image(false, uint32_t(1000 + i), uint8_t(10 + i)));
  terminal(b);
  return b;
}
inline Bytes room(const std::vector<uint16_t> &ids) {
  Bytes b(2400);
  real(b, 0, 10);
  word(b, 16, 128);
  word(b, 128 + 0x14, 1280);
  word(b, 128 + 0x94, uint32_t(ids.size()));
  std::vector<uint16_t> families;
  for (size_t i = 0; i < ids.size(); ++i) {
    const auto p = 1280 + i * 68;
    word(b, p, i % 2); // Inactive records still load their family.
    half(b, p + 4, ids[i]);
    if (((ids[i] >= 30 && ids[i] < 100) || ids[i] >= 400) &&
        std::find(families.begin(), families.end(), ids[i]) == families.end())
      families.push_back(ids[i]);
  }
  word(b, 32, 1000);
  word(b, 1000, uint32_t(1 + families.size()));
  word(b, 1004, 1900);
  word(b, 1900, 0xffffffff);
  for (size_t i = 0; i < families.size(); ++i) {
    word(b, 1008 + i * 4, uint32_t(b.size()));
    const auto texture = room_pack(uint32_t(3000 + i), uint8_t(100 + i * 10));
    b.insert(b.end(), texture.begin(), texture.end());
  }
  return b;
}
} // namespace effect_assets_fixture
