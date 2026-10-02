#include "actor.hpp"
#include "motion.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool v) {
  if (!v)
    throw std::runtime_error("Motion check failed");
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
void put(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
void flt(Bytes &b, size_t p, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, 4);
  put(b, p, bits);
}
Bytes fixture() {
  Bytes b(80);
  b[0] = 'M';
  b[1] = 'T';
  b[2] = 'N';
  b[3] = 0x80;
  half(b, 4, 16);
  half(b, 6, 1);
  put(b, 8, 0);
  put(b, 12, 60);
  put(b, 16, 12);
  put(b, 20, 2);
  half(b, 24, 3);
  half(b, 26, 2);
  put(b, 28, 20);
  put(b, 32, 44);
  flt(b, 36, 2);
  flt(b, 40, 4);
  flt(b, 44, 6);
  flt(b, 48, 6);
  flt(b, 52, 8);
  flt(b, 56, 10);
  half(b, 60, 65530);
  half(b, 62, 0);
  half(b, 64, 0);
  half(b, 66, 6);
  half(b, 68, 100);
  half(b, 70, 0);
  half(b, 76, 0x1234);
  half(b, 78, 0x5678);
  return b;
}
int main() {
  try {
    auto b = fixture();
    auto m = decode_motion(b);
    ck(m.frames == 2 && m.tracks.size() == 1 && m.flags == 0x80);
    auto first = sample_motion(m, 0);
    ck(first[0].position->x == 2 && (*first[0].angles)[0] == 65530);
    auto middle = sample_motion(m, 32768, true);
    ck(middle[0].position->x == 4 && (*middle[0].angles)[0] == 0 &&
       (*middle[0].angles)[1] == 50);
    auto wrap = sample_motion(m, 98304, true);
    ck(wrap[0].position->x == 4 && (*wrap[0].angles)[0] == 0);
    ck(sample_motion(m, 131072)[0].position->x == 2);
    ck((*sample_motion(m, 8192, true)[0].angles)[0] == 65532);
    bad([&] { decode_motion(b, 3); });
    ck(m.attributes.at(0) == 0x1234 && m.attributes.at(1) == 0x5678);
    bad([&] { sample_motion(m, -1); });
    bad([&] { decode_motion(Bytes(15)); });
    auto corrupt = b;
    put(corrupt, 28, 0xfffffffe);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    put(corrupt, 20, 0);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    put(corrupt, 36, 0x7f800000);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    corrupt.resize(70);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    put(corrupt, 28, 0xffffffff);
    put(corrupt, 32, 0xffffffff);
    auto absent = sample_motion(decode_motion(corrupt), 0);
    ck(!absent[0].position && !absent[0].angles);
    // A package with a skin record, one textureless model and three motion
    // slots.
    Bytes pack(4);
    auto word = [&](uint32_t v) {
      auto p = pack.size();
      pack.resize(p + 4);
      put(pack, p, v);
    };
    auto append = [&](const Bytes &data) {
      pack.insert(pack.end(), data.begin(), data.end());
    };
    word(4);
    append(Bytes{'S', 'K', 'I', 'N'});
    word(16);
    Bytes mdl(16);
    mdl[0] = 'M';
    mdl[1] = 'D';
    mdl[2] = 'L';
    mdl[3] = 128;
    half(mdl, 4, 16);
    put(mdl, 8, 0xffffffff);
    put(mdl, 12, 0xffffffff);
    append(mdl);
    word(0xffffffff);
    put(pack, 0, uint32_t(pack.size() - 4));
    auto motion_size = pack.size();
    word(0);
    auto motion_start = pack.size();
    word(uint32_t(b.size()));
    append(b);
    word(0);
    word(0);
    word(0xffffffff);
    put(pack, motion_size, uint32_t(pack.size() - motion_start));
    word(0);
    auto package = decode_player_package(pack);
    ck(package.models.size() == 1 && package.models[0].skin.size() == 4);
    ck(package.motions.size() == 3 && package.motions[0] &&
       !package.motions[1] && !package.motions[2]);
    ck(package.models[0].textures.empty() && package.auxiliary.empty());
    bad([&] { decode_player_package(pack, 3); });
    auto repeated = pack;
    Bytes repeat_record(84);
    put(repeat_record, 0, 80);
    std::copy(b.begin(), b.end(), repeat_record.begin() + 4);
    repeated.insert(repeated.begin() + 124, repeat_record.begin(),
                    repeat_record.end());
    put(repeated, 36, 180);
    ck(decode_player_package(repeated, 8).motions.size() == 4);
    bad([&] { decode_player_package(repeated, 7); });
    auto broken = pack;
    put(broken, 0, 0xfffffffe);
    bad([&] { decode_player_package(broken); });
    broken = pack;
    put(broken, motion_size - 4, 0);
    bad([&] { decode_player_package(broken); });
    broken = pack;
    broken.pop_back();
    bad([&] { decode_player_package(broken); });
    // Texture payload alignment is relative to the original package start.
    auto aligned = pack;
    put(aligned, 24, 0); // Model header's texture offset.
    auto tex_header = aligned.size();
    aligned.resize(tex_header + 4);
    put(aligned, tex_header, 0x80000004);
    aligned.resize((aligned.size() + 31) & ~size_t(31), 0xcd);
    aligned.insert(aligned.end(), {1, 2, 3, 4});
    ck(decode_player_package(aligned).models[0].textures ==
       Bytes({1, 2, 3, 4}));
    aligned.pop_back();
    bad([&] { decode_player_package(aligned); });
    auto multiple = pack;
    Bytes second(20);
    put(second, 0, 16);
    std::copy(mdl.begin(), mdl.end(), second.begin() + 4);
    multiple.insert(multiple.begin() + 32, second.begin(), second.end());
    put(multiple, 0, 52);
    put(multiple, 24, 0);
    put(multiple, 44, 0);
    auto append_word = [&](uint32_t value) {
      auto p = multiple.size();
      multiple.resize(p + 4);
      put(multiple, p, value);
    };
    append_word(4);
    multiple.insert(multiple.end(), {4, 3, 2, 1});
    append_word(0x80000004);
    multiple.resize((multiple.size() + 31) & ~size_t(31), 0xcd);
    multiple.insert(multiple.end(), {1, 2, 3, 4});
    auto linked = decode_player_package(multiple);
    ck(linked.models.size() == 2 &&
       linked.models[0].textures == Bytes({4, 3, 2, 1}) &&
       linked.models[1].textures == Bytes({1, 2, 3, 4}));
    Bytes skin(40);
    skin[0] = 'S';
    skin[1] = 'K';
    skin[2] = 'I';
    skin[3] = 'N';
    put(skin, 4, 3);
    put(skin, 8, 0);
    put(skin, 12, 3);
    put(skin, 16, 0x01090000);
    put(skin, 20, 0x01040000);
    put(skin, 24, 0x01000000);
    put(skin, 28, 1);
    put(skin, 32, 0);
    put(skin, 36, 0xffffffff);
    auto binding = decode_skin(skin, 2);
    ck(binding.size() == 2 && binding[0].vertices.size() == 3);
    ck(binding[0].vertices[0].joint == 1 &&
       binding[0].vertices[0].rotation_weight == 1);
    ck(binding[0].vertices[1].rotation_weight == 0.5f &&
       binding[0].vertices[2].rotation_weight == 0.1f);
    auto invalid = skin;
    put(invalid, 16, 0x02090000);
    bad([&] { decode_skin(invalid, 2); });
    invalid = skin;
    put(invalid, 16, 0x010a0000);
    bad([&] { decode_skin(invalid, 2); });
    invalid = skin;
    put(invalid, 32, 1);
    bad([&] { decode_skin(invalid, 2); });
    invalid = skin;
    put(invalid, 12, 0xffffffff);
    bad([&] { decode_skin(invalid, 2); });
    // Bounded deterministic mutations must either decode safely or reject.
    uint32_t rng = 0x8172;
    for (int i = 0; i < 600; ++i) {
      auto mutated = b;
      rng = rng * 1664525 + 1013904223;
      mutated[rng % mutated.size()] ^= uint8_t(rng >> 24);
      try {
        auto decoded = decode_motion(mutated);
        sample_motion(decoded, 32768, true);
      } catch (const std::runtime_error &) {
      }
      mutated = pack;
      rng = rng * 1664525 + 1013904223;
      mutated[rng % mutated.size()] ^= uint8_t(rng >> 24);
      try {
        decode_player_package(mutated);
      } catch (const std::runtime_error &) {
      }
    }
    std::cout << "Motion checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
