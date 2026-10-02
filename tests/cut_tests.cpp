#include "camera.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool b) {
  if (!b)
    throw std::runtime_error("Cut selection check failed");
}
template <class F> void bad(F f) {
  bool fail = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    fail = true;
  }
  ck(fail);
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
Bytes fixture() {
  Bytes b(3000);
  put(b, 0, 0x41200000);
  put(b, 16, 128);
  put(b, 128, 1132);
  put(b, 256, 2);
  for (size_t i = 0; i < 2; ++i) {
    size_t c = 1132 + i * 680;
    b[c] = 1;
    b[c + 3] = 1;
    put(b, c + 4, uint32_t(2600 + i * 32));
    size_t a = 2600 + i * 32;
    put(b, a, uint32_t(100 + i));
    b[a + 4] = 1;
    flt(b, a + 8, 2);
    flt(b, a + 16, float(i) * 4);
    flt(b, a + 20, 0);
    flt(b, a + 24, float(i + 1) * 4);
    flt(b, a + 28, 4);
  }
  return b;
}
int main() {
  try {
    auto bytes = fixture();
    auto cuts = decode_room_cuts(bytes);
    ck(cuts[0].areas.size() == 1 && cuts[0].areas[0].attribute == 100 &&
       cuts[0].areas[0].height == 2);
    ck(find_camera_cut(cuts, 0, 0, 1) == 0 &&
       find_camera_cut(cuts, 4, 1, 1) == 1);
    ck(!find_camera_cut(cuts, 1, 1, 2) && !find_camera_cut(cuts, 1, 4, 1));
    ck(select_camera_cut(cuts, 0, 4.5f, 2, 1) ==
       0); // Previous cut still within the one-unit probes.
    ck(select_camera_cut(cuts, 0, 5, 2, 1) ==
       1); // Lower-inclusive/upper-exclusive reference bounds.
    ck(select_camera_cut(cuts, 1, 20, 2, 1) ==
       1); // No candidate keeps the current cut.
    ck(select_camera_cut(cuts, 0, 4.5f, 2, 1, true) == 1);
    ck(select_camera_cut(cuts, std::nullopt, 1, 1, 1) == 0);
    cuts[0].flags = 0;
    ck(!find_camera_cut(cuts, 1, 1, 1));
    cuts = decode_room_cuts(bytes);
    for (uint8_t type = 1; type <= 4; ++type) {
      auto &a = cuts[0].areas[0];
      a.type = type;
      bool upper = type == 1 || type == 2;
      ck(bool(find_camera_cut(cuts, 2, 3, 1)) == upper);
      ck(bool(find_camera_cut(cuts, 2, 1, 1)) != upper);
      ck(!find_camera_cut(cuts, 2, 2,
                          1)); // Diagonal belongs to neither triangular half.
      // Off-centre probes distinguish the two orientations in each half.
      ck(bool(find_camera_cut(cuts, 1, 2, 1)) == (type == 2 || type == 3));
      ck(bool(find_camera_cut(cuts, 3, 2, 1)) == (type == 1 || type == 4));
    }
    cuts = decode_room_cuts(bytes);
    cuts[1].areas[0] = cuts[0].areas[0];
    ck(find_camera_cut(cuts, 1, 1, 1) == 0); // First matching cut wins overlap.
    // Degenerate rectangles are empty; degenerate triangles reject on decode.
    auto broken = bytes;
    broken[2600 + 7] = 5;
    bad([&] { decode_room_cuts(broken); });
    broken = bytes;
    put(broken, 1136, 2990);
    bad([&] { decode_room_cuts(broken); });
    broken = bytes;
    put(broken, 2616, 0x7f800000);
    bad([&] { decode_room_cuts(broken); });
    bad([&] { find_camera_cut(cuts, NAN, 0, 1); });
    bad([&] { select_camera_cut(cuts, 99, 0, 0, 1); });
    std::cout << "Cut selection checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
