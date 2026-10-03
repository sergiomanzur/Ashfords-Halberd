#include "collision.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
namespace {
void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
bool near(float a, float b) { return std::fabs(a - b) < .00002f; }
CollisionRecord box(uint8_t type, Vec3 pos = {0, 0, 0},
                    Vec3 size = {10, 10, 10}) {
  CollisionRecord r;
  r.flags = 1;
  r.type = type;
  r.position = pos;
  r.size = size;
  return r;
}
} // namespace
int main() {
  try {
    RoomCollision room;
    room.height = 15;
    room.walls = {box(0)};
    for (uint8_t type : {0, 1}) {
      room.walls[0].type = type;
      auto r = query_collision_line(room, {}, {-5, 5, 5}, {15, 5, 5});
      ck(r.wall == 0 && near(r.position.x, 0) && r.normal.x == -1,
         "PositiveX box face failed");
      r = query_collision_line(room, {}, {15, 5, 5}, {-5, 5, 5});
      ck(r.wall == 0 && near(r.position.x, 10) && r.normal.x == 1,
         "NegativeX box face failed");
      r = query_collision_line(room, {}, {5, 5, -5}, {5, 5, 15});
      ck(r.wall == 0 && near(r.position.z, 0) && r.normal.z == -1,
         "PositiveZ box face failed");
      r = query_collision_line(room, {}, {5, 5, 15}, {5, 5, -5});
      ck(r.wall == 0 && near(r.position.z, 10) && r.normal.z == 1,
         "NegativeZ box face failed");
      r = query_collision_line(room, {}, {5, 20, 5}, {5, -5, 5});
      ck(r.wall == 0 && near(r.position.y, 10) && r.normal.y == 1,
         "Box top failed");
      r = query_collision_line(room, {}, {5, -5, 5}, {5, 20, 5});
      ck(r.wall == 0 && near(r.position.y, 0) && r.normal.y == -1,
         "Box bottom failed");
    }
    room.walls[0] = box(0, {5, 0, 0}, {1, 10, 10});
    ck(!query_collision_line(room, {}, {0, 5, 5}, {5, 5, 5}).wall,
       "Box endpoint must be strict");
    ck(query_collision_line(room, {}, {5, 5, 5}, {10, 5, 5}).wall == 0,
       "Box start on face must hit");
    room.walls.push_back(box(0, {7, 0, 0}, {1, 10, 10}));
    auto r = query_collision_line(room, {box(0, {3, 0, 0}, {1, 10, 10})},
                                  {0, 5, 5}, {10, 5, 5});
    ck(r.wall == 2 && near(r.position.x, 3),
       "Static/dynamic endpoint clipping order failed");
    room.walls[0].flags = 0;
    room.walls[1].attribute = 0xffff;
    room.walls[1].floor = 9;
    r = query_collision_line(room, {}, {0, 5, 5}, {10, 5, 5});
    ck(r.wall == 1 && near(r.position.x, 7),
       "Default line query must ignore attribute/floor filters");
    room.walls = {box(0, {0, 0, 0}, {10, 0, 10})};
    r = query_collision_line(room, {}, {5, 20, 5}, {5, 0, 5});
    ck(r.wall == 0 && near(r.position.y, 15),
       "Box zero height must use room fallback");
    for (uint8_t type : {2, 3}) {
      room.walls = {box(type, {0, 0, 0}, {5, 10, 99})};
      r = query_collision_line(room, {}, {10, 5, 0}, {0, 5, 0});
      ck(r.wall == 0 && near(r.position.x, 5) && r.normal.x > 0,
         "Octagonal column side failed");
      r = query_collision_line(room, {}, {0, 20, 0}, {0, 0, 0});
      ck(r.wall == 0 && near(r.position.y, 10) && r.normal.y > 0,
         "Completed octagonal top failed");
      ck(!query_collision_line(room, {}, {0, -5, 0}, {0, 5, 0}).wall,
         "Column must not invent bottom face");
    }
    for (uint8_t type : {4, 5}) {
      room.walls = {box(type, {0, 0, 0}, {10, 5, 10})};
      r = query_collision_line(room, {}, {3, 10, 3}, {3, 0, 3});
      ck(r.wall == 0 && near(r.position.y, 5) && r.normal.y == 100,
         "Triangle top/normal failed");
      ck(!query_collision_line(room, {}, {8, 10, 8}, {8, 0, 8}).wall,
         "Triangle top contains excluded half");
      ck(!query_collision_line(room, {}, {3, 0, 3}, {3, 10, 3}).wall,
         "Triangle top must be one-sided");
      r = query_collision_line(room, {}, {8, 3, 8}, {3, 3, 3});
      ck(r.wall == 0 && near(r.position.x, 5) && near(r.position.z, 5),
         "Triangle diagonal side failed");
    }
    room.walls = {box(6, {0, 0, 0}, {10, 5, 10})};
    r = query_collision_line(room, {}, {5, 10, 5}, {5, -5, 5});
    ck(r.wall == 0 && near(r.position.y, 2.5f) && r.normal.x == -50 &&
           r.normal.y == 100,
       "Slope plane failed");
    ck(!query_collision_line(room, {}, {5, -5, 5}, {5, 10, 5}).wall,
       "Slope must be one-sided");
    room.walls = {box(7, {0, 10, 0}, {10, -5, 10})};
    r = query_collision_line(room, {}, {5, 0, 5}, {5, 20, 5});
    ck(r.wall == 0 && near(r.position.y, 5) && r.normal.y == -1,
       "Raw type7 ascending face failed");
    r = query_collision_line(room, {}, {5, 20, 5}, {5, 0, 5});
    ck(r.wall == 0 && near(r.position.y, 10) && r.normal.y == 1,
       "Raw type7 descending face failed");
    ck(!query_collision_line(room, {}, {20, 7, 5}, {0, 7, 5}).wall,
       "Preserve source type7 negativeX conditional");
    r = query_collision_line(room, {}, {-5, 7, 5}, {15, 7, 5});
    ck(r.wall == 0 && near(r.position.x, 0), "Raw type7 positiveX side failed");
    room.walls = {box(9, {NAN, NAN, NAN}, {NAN, NAN, NAN})};
    ck(!query_collision_line(room, {}, {0, 0, 0}, {1, 0, 0}).wall,
       "Unknown shape consumed unused geometry");
    std::cout << "Native line collision checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
