#include "player.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool b) {
  if (!b)
    throw std::runtime_error("Player check failed");
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
void put(Bytes &b, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.push_back(uint8_t(v >> (i * 8)));
}
void flt(Bytes &b, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, v);
}
int main() {
  try {
    Bytes b;
    put(b, 12);
    flt(b, 0);
    flt(b, .25f);
    flt(b, .5f);
    put(b, 0xffffffff);
    auto tables = decode_player_translation_tables(b);
    ck(tables.size() == 1 && tables[0].size() == 3);
    ck(player_translation_speed(tables, 0, 0) == .25f);
    ck(player_translation_speed(tables, 0, 1) == .5f);
    bad([&] { player_translation_speed(tables, 0, 2); });
    bad([&] { player_translation_speed(tables, 1, 0); });
    Bytes truncated = b;
    truncated.pop_back();
    bad([&] { decode_player_translation_tables(truncated); });
    Bytes invalid;
    put(invalid, 3);
    put(invalid, 0xffffffff);
    bad([&] { decode_player_translation_tables(invalid); });
    Bytes nonfinite;
    put(nonfinite, 4);
    put(nonfinite, 0x7fc00000);
    put(nonfinite, 0xffffffff);
    bad([&] { decode_player_translation_tables(nonfinite); });
    b.resize(b.size() + 20);
    ck(decode_player_translation_tables(b).size() == 1);
    b.resize(b.size() + 40);
    ck(decode_player_translation_tables(b).size() == 1);
    b.back() = 1;
    bad([&] { decode_player_translation_tables(b); });
    b.back() = 0;
    b.resize(b.size() + 4);
    bad([&] { decode_player_translation_tables(b); });
    PlayerTranslationTables action(12, std::vector<float>{0, 1});
    bad([&] { player_translation_speed(action, 11, 0); });
    Vec3 p{1, 2, 3};
    auto q = player_add_speed(p, 0, 1);
    ck(q.x == 1 && q.y == 2 && std::fabs(q.z - 2) < .00001f);
    q = player_add_speed(p, 16384, 1);
    ck(std::fabs(q.x) < .00001f && q.z == 3);
    q = player_add_speed(p, 32768, 1);
    ck(q.x == 1 && std::fabs(q.z - 4) < .00001f);
    q = player_add_speed(p, 49152, 1);
    ck(std::fabs(q.x - 2) < .00001f && q.z == 3);
    q = player_add_speed(p, 0, 1, 32768);
    ck(std::fabs(q.z - 4) < .00001f);
    bad([&] { player_add_speed(p, 0, INFINITY); });
    std::cout << "Player translation and movement checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
