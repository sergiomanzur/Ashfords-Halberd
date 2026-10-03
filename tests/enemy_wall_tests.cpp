#include "enemy_wall.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void bad(F f) {
  bool hit = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    hit = true;
  }
  ck(hit, "Invalid wall data not rejected");
}
CollisionRecord wall(uint8_t type = 0) {
  CollisionRecord w;
  w.flags = 1;
  w.type = type;
  w.position = {0, 0, 0};
  w.size = {10, 10, 10};
  return w;
}
int main() {
  try {
    RoomCollision r;
    r.height = 30;
    r.walls = {wall()};
    std::vector<CollisionRecord> dyn{wall()};
    CollisionBody b;
    b.radius = 1;
    b.height = 2;
    b.position = {-1, -2, -1};
    b.floor = 99;
    b.flags = 0x2000;
    r.walls[0].attribute = 0x10040;
    r.walls[0].floor = -1;
    ck(enemy_wall_type(r, dyn, b) == &r.walls[0],
       "Static first, inclusive lower/body-Y or ignored attrs/floor failed");
    b.position.x = 11;
    dyn.clear();
    ck(!enemy_wall_type(r, dyn, b), "Rectangle upper X must be strict");
    b.position = {0, 10, 11};
    ck(!enemy_wall_type(r, dyn, b), "Rectangle upper Z must be strict");
    b.position.z = 0;
    ck(enemy_wall_type(r, dyn, b), "Rectangle top Y must be inclusive");
    b.position.y = 10.01f;
    ck(!enemy_wall_type(r, dyn, b), "Vertical gap should miss");
    r.walls[0].size.y = 0;
    b.position.y = 30;
    ck(enemy_wall_type(r, dyn, b), "Zero height must use room height");
    r.walls[0].type = 1;
    b.flags = 0x400;
    dyn = {wall(0)};
    b.position.y = 0;
    ck(enemy_wall_type(r, dyn, b) == &dyn[0],
       "Odd400 gate/dynamic order failed");
    b.flags = 0;
    r.walls[0].flags = 0;
    ck(enemy_wall_type(r, dyn, b) == &dyn[0], "Inactive wall should skip");
    r.walls[0] = wall(2);
    r.walls[0].size.x = 2;
    dyn.clear();
    b.position = {3, 0, 0};
    ck(!enemy_wall_type(r, dyn, b),
       "Cylinder radial upper bound must be strict");
    b.position.x = 2.99f;
    ck(enemy_wall_type(r, dyn, b), "Cylinder expanded radius failed");
    r.walls[0].type = 3;
    b.flags = 0x400;
    ck(!enemy_wall_type(r, dyn, b), "Odd cylinder gate failed");
    r.walls[0] = wall(6);
    b.position = {-.5f, 0, 5};
    ck(!enemy_wall_type(r, dyn, b), "Slope query must ignore radius");
    b.position.x = 0;
    ck(enemy_wall_type(r, dyn, b), "Slope lower boundary failed");
    b.radius = std::numeric_limits<float>::quiet_NaN();
    ck(enemy_wall_type(r, dyn, b), "Type6 must bypass unused radius");
    b.radius = 1;
    b.position.x = 10;
    ck(!enemy_wall_type(r, dyn, b), "Slope upper boundary failed");
    r.walls[0] = wall(7);
    b.position = {5, 9, 5};
    b.flags = 0x100;
    ck(enemy_wall_type(r, dyn, b), "Flag100 ledge crossing failed");
    b.position.y = 8;
    ck(!enemy_wall_type(r, dyn, b),
       "Flag100 ledge upper body bound must be strict");
    b.position.y = 10;
    ck(!enemy_wall_type(r, dyn, b),
       "Flag100 ledge lower body bound must be strict");
    b.flags = 0;
    b.position.y = -1;
    b.height = 11;
    ck(enemy_wall_type(r, dyn, b),
       "Normal ledge upper body bound must be inclusive");
    b.position.y = 0;
    ck(!enemy_wall_type(r, dyn, b), "Normal ledge must start below base");
    r.walls[0].size.y = 0;
    b.height = 1;
    b.position.y = -1;
    ck(enemy_wall_type(r, dyn, b),
       "Type7 must retain raw zero height, not room height");
    r.walls[0] = wall(4);
    b.flags = 0;
    b.height = 2;
    b.position = {1, 0, 1};
    ck(enemy_wall_type(r, dyn, b), "Triangle query failed");
    r.height = std::numeric_limits<float>::quiet_NaN();
    ck(enemy_wall_type(r, dyn, b),
       "Nonzero triangle height must bypass unused room height");
    r.height = 30;
    r.walls[0].type = 5;
    b.flags = 0x400;
    ck(!enemy_wall_type(r, dyn, b), "Odd triangle gate failed");
    r.walls[0] = wall(8);
    ck(!enemy_wall_type(r, dyn, b), "Unknown type must skip");
    r.walls.clear();
    b.flags = 1;
    b.radius = 0;
    b.position = {0, 0, 0};
    auto front = wall();
    front.position = {-.5f, 0, -7};
    front.size = {1, 10, 2};
    r.walls = {front};
    ck(enemy_direction_wall(r, dyn, b, 0, 0, 6) == &r.walls[0],
       "Forward Ninja query failed");
    ck(!enemy_direction_wall(r, dyn, b, 16384, 0, 6), "Yaw rotation failed");
    auto left = wall();
    left.position = {5, 0, -1};
    left.size = {2, 10, 2};
    r.walls = {left};
    ck(enemy_side_wall(r, dyn, b, 0, 6, 7) == 1, "Left side sign failed");
    auto right = left;
    right.position.x = -7;
    r.walls = {right};
    ck(enemy_side_wall(r, dyn, b, 0, 6, 7) == -1, "Right side sign failed");
    r.walls.push_back(left);
    ck(enemy_side_wall(r, dyn, b, 0, 6, 7) == 7, "Both policy failed");
    ck(enemy_side_wall(r, dyn, b, 0, 6, 0) == 0, "Both0 policy failed");
    b.position.x = 20;
    ck(enemy_side_wall(r, dyn, b, 0, 6, 7) == 0, "Explicit origin failed");
    b.position.x = std::numeric_limits<float>::infinity();
    bad([&] { enemy_wall_type(r, dyn, b); });
    b.position.x = 0;
    bad([&] {
      enemy_direction_wall(r, dyn, b, 0, 0,
                           std::numeric_limits<float>::quiet_NaN());
    });
    std::cout << "Source wall types, boundaries, dynamic ordering and "
                 "directional probes passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
