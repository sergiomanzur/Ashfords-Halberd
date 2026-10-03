#include "room.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Malformed native route did not fail");
}
void put(Bytes &b, size_t p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(value >> (i * 8));
}
void flt(Bytes &b, size_t p, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, 4);
  put(b, p, bits);
}
Bytes fixture() {
  Bytes b(1600);
  flt(b, 0, 10);
  put(b, 16, 128);
  put(b, 128 + 0x28, 1200);
  put(b, 128 + 0x2c, 1500);
  put(b, 128 + 0xa8, 3);
  for (size_t i = 0; i < 3; ++i) {
    size_t p = 1200 + i * 36;
    b[p + 2] = 200;
    b[p + 3] = 255; // Authored ID/floor do not select routes.
    put(b, p + 4, 123);
    flt(b, p + 8, float(i * 20));
    flt(b, p + 20, 10);
    flt(b, p + 24, 10);
    flt(b, p + 28, 10);
    b[p + 32] = 222;
  }
  const Bytes matrix{255, 0, 0, 1, 255, 1, 1, 2, 255};
  std::copy(matrix.begin(), matrix.end(), b.begin() + 1500);
  return b;
}
int main() {
  try {
    auto data = fixture();
    auto routes = decode_room_routes(data);
    ck(routes.regions.size() == 3 && routes.regions[0].id == 200 &&
           routes.regions[0].floor == -1 &&
           routes.regions[0].attribute == 123 &&
           routes.regions[0].parameters[0] == 222 &&
           routes.next_hops.size() == 9,
       "Route decoder did not preserve bounded ATR metadata/matrix");
    ck(room_route_id(routes, {0, 0, 0}) == 0 &&
           room_route_id(routes, {10, 10, 10}) == 0,
       "Route membership filtered inactive flags or excluded an endpoint");
    ck(room_route_id(routes, {-0.001f, 0, 0}) == 255 &&
           room_route_id(routes, {10.001f, 0, 0}) == 255 &&
           room_route_id(routes, {1, -.001f, 1}) == 255 &&
           room_route_id(routes, {1, 10.001f, 1}) == 255 &&
           room_route_id(routes, {1, 1, -.001f}) == 255 &&
           room_route_id(routes, {1, 1, 10.001f}) == 255,
       "Route membership lost its inclusive three-dimensional bounds");
    Vec3 output{99, 98, 97};
    ck(room_route(routes, {1, 1, 1}, {41, 2, 3}, output) == 1 &&
           output.x == 25 && output.y == 5 && output.z == 5,
       "Route matrix orientation or next-region center changed");
    ck(room_route(routes, {41, 1, 1}, {1, 2, 3}, output) == 0 && output.x == 5,
       "Route destination-row/source-column order was transposed");
    ck(room_route(routes, {1, 1, 1}, {9, 8, 7}, output) == 255 &&
           output.x == 9 && output.y == 8 && output.z == 7,
       "Same-region source behavior changed");
    auto preserved = [&](auto f) {
      output = {99, 98, 97};
      f();
      ck(output.x == 99 && output.y == 98 && output.z == 97,
         "Missing/invalid route partially changed output");
    };
    preserved([&] {
      ck(room_route(routes, {15, 1, 1}, {41, 1, 1}, output) == 255,
         "Missing source accepted");
    });
    preserved([&] {
      ck(room_route(routes, {1, 1, 1}, {15, 1, 1}, output) == 255,
         "Missing destination accepted");
    });
    routes.next_hops[6] = 255;
    preserved([&] {
      bad([&] { room_route(routes, {1, 1, 1}, {41, 1, 1}, output); });
    });
    routes.next_hops.resize(1);
    preserved([&] {
      bad([&] { room_route(routes, {1, 1, 1}, {41, 1, 1}, output); });
    });
    // Unused matrix data must not reject same-region/missing-region queries.
    ck(room_route(routes, {1, 1, 1}, {2, 3, 4}, output) == 255 && output.y == 3,
       "Same-region query consumed its unused matrix");
    routes.regions.push_back(routes.regions[0]);
    ck(room_route_id(routes, {1, 1, 1}) == 0,
       "Overlapping routes did not keep first index");
    routes.regions[0].size.x = -1;
    ck(room_route_id(routes, {1, 1, 1}) == 3,
       "Negative source region extent matched");
    auto malformed = data;
    put(malformed, 128 + 0xa8, 256);
    bad([&] { decode_room_routes(malformed); });
    malformed = data;
    put(malformed, 128 + 0x28, 0);
    bad([&] { decode_room_routes(malformed); });
    malformed = data;
    put(malformed, 128 + 0x28, 1580);
    bad([&] { decode_room_routes(malformed); });
    malformed = data;
    put(malformed, 128 + 0x2c, 1598);
    bad([&] { decode_room_routes(malformed); });
    malformed = data;
    flt(malformed, 1208, INFINITY);
    bad([&] { decode_room_routes(malformed); });
    malformed = data;
    put(malformed, 128 + 0xa8, 0);
    put(malformed, 128 + 0x28, 0xffffffff);
    put(malformed, 128 + 0x2c, 0xffffffff);
    auto empty = decode_room_routes(malformed);
    ck(empty.regions.empty() && empty.next_hops.empty() &&
           room_route_id(empty, {}) == 255,
       "Zero-route room dereferenced unused pointers");
    bad([&] { room_route_id(routes, {INFINITY, 0, 0}); });
    routes.regions[0].size.x = INFINITY;
    bad([&] { room_route_id(routes, {}); });
    std::cout << "Native room routes passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
