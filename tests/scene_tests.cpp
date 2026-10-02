#include "scene.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool ok) {
  if (!ok)
    throw std::runtime_error("Scene check failed");
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed);
}
void p32(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void p16(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
void pf(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  p32(b, p, v);
}
Bytes model() {
  Bytes b(512);
  b[0] = 'M';
  b[1] = 'D';
  b[2] = 'L';
  b[3] = 128;
  p16(b, 4, 16);
  p16(b, 6, 1);
  p32(b, 12, 0);
  p32(b, 16, 7);
  p32(b, 20, 64);
  p32(b, 60, 0xffffffff);
  p32(b, 64, 0xffffffff);
  p32(b, 80, 96);
  p32(b, 84, 272);
  b[112] = 51;
  p16(b, 114, 39);
  p16(b, 116, 0);
  p16(b, 118, 3);
  pf(b, 176, 0);
  pf(b, 180, 0);
  pf(b, 184, 0);
  pf(b, 208, 1);
  pf(b, 212, 0);
  pf(b, 216, 0);
  pf(b, 240, 0);
  pf(b, 244, 1);
  pf(b, 248, 0);
  b[272] = 255;
  b[288] = 66;
  p16(b, 290, 11);
  p16(b, 292, 1);
  p16(b, 294, 3);
  for (int i = 0; i < 3; ++i) {
    p16(b, 296 + i * 6, uint16_t(i));
    p16(b, 298 + i * 6, uint16_t(i == 1 ? 1023 : 0));
    p16(b, 300 + i * 6, uint16_t(i == 2 ? 1023 : 0));
  }
  b[314] = 255;
  return b;
}
int main() {
  try {
    auto b = model();
    p32(b, 16, 15);
    ck(decode_model(b, 0).empty());
    ck(decode_model(b, 0, true)[0].initially_hidden);
    b = model();
    auto meshes = decode_model(b, 0);
    ck(meshes.size() == 1 && meshes[0].triangles.size() == 1);
    ck(meshes[0].triangles[0].corners[1].position.x == 1 &&
       meshes[0].triangles[0].corners[1].u == 1);
    b[288] = 65;
    p16(b, 304, 255);
    p16(b, 312, 255);
    ck(decode_model(b, 0)[0].triangles[0].corners[1].u == 1);
    b = model();
    p16(b, 290, 12);
    b[314] = 0;
    b[316] = 255;
    ck(decode_model(b, 0)[0].triangles.size() == 1);
    b[314] = 1;
    bad([&] { decode_model(b, 0); });
    b = model();
    b[288] = 64;
    p16(b, 290, 5);
    p16(b, 296, 0);
    p16(b, 298, 1);
    p16(b, 300, 2);
    b[302] = 255;
    ck(decode_model(b, 0)[0].triangles[0].texture == 0xffff);
    b = model();
    p16(b, 296, 3);
    bad([&] { decode_model(b, 0); });
    b = model();
    p32(b, 60, 0);
    bad([&] { decode_model(b, 0); });
    b = model();
    p32(b, 80, 0xfffffff0);
    bad([&] { decode_model(b, 0); });
    b = model();
    p32(b, 16, 6);
    pf(b, 24, 2);
    ck(decode_model(b, 0)[0].triangles[0].corners[1].position.x == 3);
    b = model();
    p32(b, 16, 3);
    pf(b, 48, 2);
    pf(b, 52, 3);
    pf(b, 56, 4);
    auto scaled = decode_model(b, 0);
    ck(scaled[0].triangles[0].corners[1].position.x == 2 &&
       scaled[0].triangles[0].corners[2].position.y == 3);
    b = model();
    p32(b, 16, 5);
    p32(b, 44, 16384);
    ck(std::abs(decode_model(b, 0)[0].triangles[0].corners[1].position.y - 1) <
       .00001);
    b = model();
    auto original = b;
    for (size_t i = 0; i < 28; ++i)
      b[296 + i] = original[288 + i];
    b[288] = 17;
    p16(b, 290, 2);
    b[292] = 0;
    b[293] = 0;
    b[294] = 255;
    b[295] = 255;
    auto material = decode_model(b, 0)[0].triangles[0].color;
    ck(material[0] == 255 && material[2] == 0);
    Bytes t(16 + 48 + 2 + 1024);
    t[0] = 'T';
    t[1] = 'I';
    t[2] = 'M';
    t[3] = '2';
    t[4] = 4;
    p16(t, 6, 1);
    p32(t, 16, 48 + 2 + 1024);
    p32(t, 20, 1024);
    p32(t, 24, 2);
    p16(t, 28, 48);
    p16(t, 30, 256);
    t[34] = 3;
    t[35] = 5;
    p16(t, 36, 2);
    p16(t, 38, 1);
    t[64] = 8;
    t[65] = 16;
    t[66 + 16 * 4] = 11;
    t[66 + 16 * 4 + 3] = 128;
    t[66 + 8 * 4] = 22;
    t[66 + 8 * 4 + 3] = 64;
    auto tex = decode_tim2(t, 0, t.size());
    ck(tex.width == 2 && tex.rgba[0] == 11 && tex.rgba[3] == 255 &&
       tex.rgba[4] == 22 && tex.rgba[7] == 128);
    t[32] = 16;
    ck(decode_tim2(t, 0, t.size()).rgba == tex.rgba);
    t[32] = 1;
    ck(decode_tim2(t, 0, t.size()).rgba == tex.rgba);
    t[32] = 5;
    bad([&] { decode_tim2(t, 0, t.size()); });
    t[32] = 0;
    t.pop_back();
    bad([&] { decode_tim2(t, 0, t.size()); });
    Bytes four(16 + 48 + 1 + 128);
    four[0] = 'T';
    four[1] = 'I';
    four[2] = 'M';
    four[3] = '2';
    four[4] = 4;
    p16(four, 6, 1);
    p32(four, 16, 48 + 1 + 128);
    p32(four, 20, 128);
    p32(four, 24, 1);
    p16(four, 28, 48);
    p16(four, 30, 32);
    four[34] = 3;
    four[35] = 4;
    p16(four, 36, 2);
    p16(four, 38, 1);
    four[64] = 0x88;
    four[65 + 8 * 4] = 22;
    four[65 + 16 * 4] = 11;
    ck(decode_tim2(four, 0, four.size()).rgba[0] == 11);
    four[34] = 0x83;
    ck(decode_tim2(four, 0, four.size()).rgba[0] == 22);
    Bytes pack(32);p32(pack,0,0x324d4954);p32(pack,4,uint32_t(four.size()));
    pack.insert(pack.end(),four.begin(),four.end());
    auto terminal=pack.size();pack.resize(terminal+4);p32(pack,terminal,0xffffffff);
    ck(decode_texture_pack(pack,0).at(0).rgba==decode_tim2(four,0,four.size()).rgba);
    // Reproducible mutation smoke: malformed binary data must terminate safely.
    uint32_t random = 0x12345678;
    for (int trial = 0; trial < 1500; ++trial) {
      auto mutated = model();
      for (int n = 0; n < 3; ++n) {
        random = random * 1664525u + 1013904223u;
        auto at = random % mutated.size();
        random = random * 1664525u + 1013904223u;
        mutated[at] = uint8_t(random >> 24);
      }
      try {
        decode_model(mutated, 0);
      } catch (const std::runtime_error &) {
      }
    }
    std::cout << "Scene boundaries passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
