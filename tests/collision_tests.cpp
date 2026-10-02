#include "collision.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool b) {
  if (!b)
    throw std::runtime_error("Collision check failed");
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
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
int main() {
  try {
    Bytes b(1400);
    flt(b, 0, 10);
    put(b, 16, 128);
    put(b, 128 + 0x18, 1200);
    put(b, 128 + 0x98, 1);
    flt(b, 128 + 0x118, 30);
    flt(b, 128 + 0x120 + 8, 0);
    flt(b, 128 + 0x120 + 12, 20);
    b[1200] = 1;
    b[1203] = 255;
    put(b, 1204, 1);
    flt(b, 1220, 10);
    flt(b, 1224, 10);
    flt(b, 1228, 10);
    auto room = decode_room_collision(b);
    ck(room.walls.size() == 1 && room.walls[0].floor == -1 &&
       room.height == 30);
    ck(player_floor_number(room, 0) == 0 && player_floor_number(room, 20) == 1);
    CollisionBody body;
    body.position = {-.5f, 0, 5};
    body.radius = 1;
    body.height = 2;
    body.floor = -1;
    auto result = collide_player_walls(room, body);
    ck(result.hits == 1 && result.position.x == -1 && result.position.z == 5);
    body.position = {10.5f, 0, 5};
    result = collide_player_walls(room, body);
    ck(result.position.x == 11);
    body.position = {5, 0, -.5f};
    result = collide_player_walls(room, body);
    ck(result.position.z == -1);
    body.position = {5, 0, 10.5f};
    result = collide_player_walls(room, body);
    ck(result.position.z == 11);
    body.position = {-.5f, 0, -.5f};
    result = collide_player_walls(room, body);
    ck(std::fabs(result.position.x + .7071f) < .001f &&
       std::fabs(result.position.z + .7071f) < .001f);
    body.position = {-.9f, 0, -.9f};
    result = collide_player_walls(room, body);
    ck(result.position.x == -.9f && result.hits == 1);
    body.floor = 0;
    ck(collide_player_walls(room, body).hits == 0);
    body.floor = -1;
    body.position = {-.5f, 11, 5};
    ck(collide_player_walls(room, body).hits == 0);
    room.walls[0].size.y = 0;
    ck(collide_player_walls(room, body).hits == 1);
    body.position = {1, 0, 0};
    room.walls[0].type = 2;
    room.walls[0].size.x = 2;
    result = collide_player_walls(room, body);
    ck(std::fabs(result.position.x - 3) < .001f && result.hits == 1);
    room.walls[0].type = 1;
    body.flags = 0x400;
    ck(collide_player_walls(room, body).hits == 0);
    body.flags = 0x100;
    room.walls[0].type = 7;
    room.walls[0].size = {10, 10, 10};
    body.position = {5, 9, 5};
    result = collide_player_walls(room, body);
    ck(result.hits == 1 && result.position.x == 11);
    body.position.y = 10;
    ck(collide_player_walls(room, body).hits == 0);
    room.walls[0].type = 6;
    room.walls[0].id = 0;
    room.walls[0].floor = 0;
    body.floor = 0;
    body.position = {5, 6, 5};
    result = collide_player_walls(room, body);
    ck(result.hits == 1 && std::fabs(result.position.y - 5.5f) < .001f);
    body.flags = 0;
    ck(collide_player_walls(room, body).hits == 0);
    body.floor = -1;
    room.walls[0].floor = -1;
    room.walls[0].type = 4;
    room.walls[0].id = 0;
    body.position = {2, 0, 2};
    result = collide_player_walls(room, body);
    ck(result.hits == 1 && std::fabs(result.position.x - 6) < .001f &&
       std::fabs(result.position.z - 6) < .001f);
    body.position = {9, 0, 9};
    ck(collide_player_walls(room, body).hits == 0);
    for (uint8_t id = 1; id < 4; ++id) {
      auto &wall = room.walls[0];
      wall.id = id;
      float sx = id >= 2 ? -1.f : 1.f, sz = (id == 1 || id == 3) ? -1.f : 1.f;
      wall.size = {10 * sx, 10, 10 * sz};
      body.position = {2 * sx, 0, 2 * sz};
      result = collide_player_walls(room, body);
      ck(result.hits == 1 && std::fabs(result.position.x - 6 * sx) < .001f &&
         std::fabs(result.position.z - 6 * sz) < .001f);
    }
    for (uint8_t id = 1; id < 4; ++id) {
      auto &wall = room.walls[0];
      wall.type = 6;
      wall.id = id;
      wall.floor = 0;
      wall.size = {10, 10, 10};
      body.floor = 0;
      body.flags = 0x100;
      body.position = {5, 6, 5};
      result = collide_player_walls(room, body);
      ck(result.hits == 1 && std::fabs(result.position.y - 5.5f) < .001f);
    }
    room.walls[0].type = 7;
    body.flags = 0;
    body.height = 12;
    body.position = {5, -1, 5};
    ck(collide_player_walls(room, body).hits == 1);
    body.position.y = 0;
    ck(collide_player_walls(room, body).hits == 0);
    room.walls[0].size.y = -5;
    body.flags = 0x100;
    body.height = 2;
    body.position = {5, -6, 5};
    ck(collide_player_walls(room, body).hits == 1);
    room.walls[0].type = 8;
    bad([&] { collide_player_walls(room, body); });
    auto broken = b;
    put(broken, 128 + 0x18, 1390);
    bad([&] { decode_room_collision(broken); });
    broken = b;
    flt(broken, 1220, INFINITY);
    bad([&] { decode_room_collision(broken); });
    body.position.x = NAN;
    bad([&] { collide_player_walls(room, body); });
    RoomCollision empty;
    empty.ground[2] = 7;
    body.position = {1, 20, 2};
    body.flags = 0x100;
    body.floor = 0;
    result = collide_player_walls(empty, body);
    ck(result.position.y == 7 && result.hits == 0);
    empty.walls.push_back(room.walls[0]);
    empty.walls[0].flags = 0;
    ck(collide_player_walls(empty, body).position.y == 7);
    body.floor = -3;
    bad([&] { collide_player_walls(empty, body); });
    RoomCollision ordered;
    ordered.height = 30;
    CollisionRecord a;
    a.flags = 1;
    a.size = {10, 10, 10};
    auto second = a;
    second.position.x = 10;
    ordered.walls = {a, second};
    body.position = {10.5f, 0, 5};
    body.floor = 0;
    body.flags = 0;
    body.height = 2;
    ck(collide_player_walls(ordered, body).position.x == 9);
    ordered.walls = {second, a};
    ck(collide_player_walls(ordered, body).position.x == 11);
    ordered.walls = {a};
    body.position = {6, 0, 4};
    ck(collide_player_walls(ordered, body).position.x == 11);
    ck(collide_player_walls_ex(ordered, body, {-.5f, 0, 4}).position.x == -1);
    ordered.walls[0].type = 6;
    body.flags = 0x100;
    body.position = {5, 6, 5};
    ck(collide_player_walls_ex(ordered, body, {5, 6, 5}).hits == 0);
    uint32_t seed = 0x6a31d;
    for (size_t i = 0; i < 600; ++i) {
      auto mutation = b;
      seed = seed * 1664525u + 1013904223u;
      size_t offset = seed % mutation.size();
      seed = seed * 1664525u + 1013904223u;
      mutation[offset] ^= uint8_t(1u << (seed % 8));
      try {
        auto decoded = decode_room_collision(mutation);
        CollisionBody probe;
        probe.position = {1, 0, 1};
        collide_player_walls(decoded, probe);
        collide_player_walls_ex(decoded, probe, {1, 0, 1});
      } catch (const std::runtime_error &) {
      }
    }
    std::cout << "Collision checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
