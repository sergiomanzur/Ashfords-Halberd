#include "effects.hpp"
#include "room.hpp"
#include "scenario.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace ashford;
void ck(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
template <class F> void bad(F action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Expected invalid room effect rejection");
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void real(Bytes &b, size_t p, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, 4);
  put(b, p, bits);
}
Bytes fixture() {
  Bytes b(1600);
  real(b, 0, 10);
  put(b, 16, 128);
  put(b, 128 + 0x14, 1200);
  put(b, 128 + 0x94, 3);
  for (size_t i = 0; i < 3; ++i) {
    const auto p = 1200 + i * 68;
    put(b, p, i == 0 ? 2 : i == 1 ? 0 : 0x81);
    put(b, p + 4, 123 + uint32_t(i) | (7u << 16));
    put(b, p + 8, 0x0102fffe);
    real(b, p + 12, 3);
    real(b, p + 16, -4);
    real(b, p + 20, 5);
    real(b, p + 24, 6);
    real(b, p + 28, 7);
    real(b, p + 32, 8);
    put(b, p + 36, 0xffff8000);
    real(b, p + 52, 9);
    real(b, p + 56, 10);
    real(b, p + 60, 11);
    put(b, p + 64, 0xfedcba98);
  }
  return b;
}
void records() {
  auto b = fixture();
  const auto decoded = decode_room_effects(b);
  ck(decoded.size() == 3, "Effect table count");
  const auto &e = decoded.at(2);
  ck(e.flags == 0x81 && e.id == 125 && e.type == 7 && e.floor == -2 &&
         e.model_version == 258,
     "EF_WRK identity/signed fields");
  ck(e.position.x == 3 && e.position.y == -4 && e.position.z == 5 &&
         e.scale.x == 6 && e.scale.y == 7 && e.scale.z == 8,
     "EF_WRK position/scale");
  ck(e.angles[0] == -1 && e.angles[1] == -32768 && e.link_type == 0 &&
         e.link_index == 0 && e.linked_joint == 0 && e.offset.x == 9 &&
         e.offset.y == 10 && e.offset.z == 11 && e.parameter == -19088744,
     "EF_WRK rotation/link layout");
  auto invalid = b;
  put(invalid, 128 + 0x14, 0);
  bad([&] { decode_room_effects(invalid); });
  invalid = b;
  put(invalid, 128 + 0x94, 513);
  bad([&] { decode_room_effects(invalid); });
  invalid = b;
  invalid.resize(1403);
  bad([&] { decode_room_effects(invalid); });
  invalid = b;
  real(invalid, 1200 + 24, std::numeric_limits<float>::quiet_NaN());
  bad([&] { decode_room_effects(invalid); });
  invalid = b;
  put(invalid, 1200 + 4, 450);
  bad([&] { decode_room_effects(invalid); });
  put(b, 128 + 0x94, 0);
  put(b, 128 + 0x14, 0);
  ck(decode_room_effects(b).empty(), "Empty room effect table");
}
void binding() {
  auto b = fixture();
  ScenarioState s;
  auto prior = std::make_shared<NativeEffectPool>();
  prior->works[0].flags = 3;
  prior->works[0].id = 998;
  prior->works[1].flags = 1;
  prior->works[1].id = 999;
  prior->scratch.id = 370;
  s.effect_pool = prior;
  bind_room_effects(b, s);
  ck(s.effect_slots == std::vector<size_t>{2, 3, 3},
     "Reserved/free slots and inactive record alias");
  ck(s.effect_pool != prior && prior->works[2].flags == 0 &&
         s.effect_pool->works[0].id == 998 &&
         s.effect_pool->works[1].id == 999 && s.effect_pool->scratch.id == 370,
     "Birth must preserve retained pool and scratch");
  const auto &w = s.effect_pool->works[3];
  ck(w.flags == 1 && w.id == 125 && w.type == 7 && w.floor == -2 &&
         w.model_version == 2 && w.texture_id == -1 && w.scale.x == 6 &&
         w.base_scale.y == 7 && w.angles[0] == -1 && w.angles[1] == -32768 &&
         !w.linked_parent && w.offset.x == 0,
     "Source unlinked effect birth/truncation");
  auto untouched = s;
  run_scenario(Bytes{67, 2, 0, 0, 0}, s);
  ck((s.effect_pool->works[3].state_flags & 0x1000000) &&
         untouched.effect_pool->works[3].state_flags == 0 &&
         s.effect_pool->works[2].state_flags == 0,
     "Opcode67 uses authored map with copy isolation");
  run_scenario(Bytes{67, 1, 2, 0, 0}, s);
  ck(s.effect_pool->works[3].state_flags == 0,
     "Any nonzero visibility must restore mapped effect");
  const auto stable = s.effect_pool;
  const auto map = s.effect_slots;
  bad([&] { run_scenario(Bytes{67, 9, 0, 0, 0}, s); });
  bad([&] { run_scenario(Bytes{67, 0, 0}, s); });
  ck(s.effect_pool == stable && s.effect_slots == map,
     "Bad visibility must preserve pool/map");
  auto invalid = b;
  put(invalid, 1200 + 40, 1);
  bad([&] { bind_room_effects(invalid, s); });
  ck(s.effect_pool == stable && s.effect_slots == map,
     "Unsupported link must preserve pool/map");
  auto full = std::make_shared<NativeEffectPool>();
  for (auto &v : full->works)
    v.flags = 3;
  full->works[511].flags = 0;
  s.effect_pool = full;
  s.effect_slots = {77};
  bad([&] { bind_room_effects(b, s); });
  ck(s.effect_pool == full && full->works[511].flags == 0 &&
         s.effect_slots == std::vector<size_t>{77},
     "Failed partial binding must not consume last free slot");
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Missing mode");
    if (std::string(argv[1]) == "records")
      records();
    else
      binding();
    std::cout << "Native room effect checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
