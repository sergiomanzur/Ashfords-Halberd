#include "effects.hpp"
#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
template <class F> void bad(F fn) {
  bool rejected = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Invalid source effect input accepted");
}
CollisionRecord wall(uint8_t type, float y = 0, float height = 10) {
  CollisionRecord r;
  r.flags = 1;
  r.type = type;
  r.position = {0, y, 0};
  r.size = {10, height, 10};
  return r;
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  auto &e = s.enemies[0];
  e.flags = 1;
  e.id = 1;
  e.zombie_work.emplace();
  e.motion_number = 8;
  e.angles[1] = 16384;
  e.damage_joint = 5;
  e.damage_velocity = {7, 8, 9};
  e.position = {0, 0, 0};
  e.model_work.emplace();
  e.model_work->joints.resize(18);
  for (size_t i = 0; i < 18; ++i) {
    auto &m = e.model_work->joints[i].matrix;
    m[3] = 10 + i;
    m[7] = 20 + i;
    m[11] = 30 + i;
  }
  auto floor = wall(3, 0, 5);
  floor.position.x = floor.position.z = -100;
  floor.size.x = floor.size.z = 200;
  floor.parameters[0] = 1;
  s.collision->floors.push_back(floor);
  auto ground = wall(7, 0);
  ground.position.x = ground.position.z = -100;
  ground.size.x = ground.size.z = 200;
  s.collision->walls.push_back(ground);
  return s;
}
size_t active(const NativeEffectPool &p) {
  size_t n = 0;
  for (const auto &e : p.works)
    n += bool(e.flags & 1);
  return n;
}
int main(int argc, char **) {
  try {
    if (argc == 1) {
      RoomCollision room;
      room.height = 4;
      ck(query_ground_position(room, {}, {}).height == -1000,
         "Ground sentinel changed");
      room.walls = {wall(0)};
      auto q = query_ground_position(room, {}, {5, 15, 5});
      ck(q.height == 10 && q.wall == 0, "Rectangle source ground failed");
      ck(!query_ground_position(room, {}, {10, 15, 5}).wall,
         "Rectangle upper boundary must be excluded");
      room.walls[0].size.y = 0;
      ck(query_ground_position(room, {}, {5, 15, 5}).height == 4,
         "Ground fallback height failed");
      room.walls = {wall(0, 0, 10), wall(0, 0, 10)};
      ck(query_ground_position(room, {wall(0, 0, 10)}, {5, 15, 5}).wall == 0,
         "Ground ties must retain earliest source wall");
      ck(query_ground_position(room, {wall(0, 0, 12)}, {5, 15, 5}).wall == 2,
         "Ground dynamic wall order/nearest failed");
      ck(!query_ground_position(room, {}, {5, 10010, 5}).wall,
         "Ground10000 sentinel must be strict");
      room.walls = {wall(2)};
      ck(query_ground_position(room, {}, {9, 15, 0}).height == 10,
         "Circle ground failed");
      ck(!query_ground_position(room, {}, {10, 15, 0}).wall,
         "Circle boundary must be strict");
      room.walls = {wall(4)};
      ck(query_ground_position(room, {}, {0, 15, 5}).height == 10,
         "Triangle source origin division boundary failed");
      ck(!query_ground_position(room, {}, {5, 15, 5}).wall,
         "Triangle diagonal must be excluded");
      room.walls[0].size.x = -10;
      ck(query_ground_position(room, {}, {-2, 15, 5}).height == 10,
         "Signed triangle width failed");
      room.walls = {wall(4, 1000, -1e-6f)};
      ck(query_ground_position(room, {}, {0, 1000, 5}).height == 1000,
         "Triangle3 must compare rounded source py+h against py");
      room.walls = {wall(6, 0, 10)};
      for (uint8_t id = 0; id < 4; ++id) {
        room.walls[0].id = id;
        ck(query_ground_position(room, {}, {2, 3, 4}).height == (id == 0   ? 2
                                                                 : id == 1 ? 6
                                                                 : id == 2 ? 4
                                                                           : 8),
           "Source raw slope height or negative nearest distance changed");
      }
      room.walls = {wall(7, 0)};
      ck(query_ground_position(room, {}, {5, -.0001f, 5}).height == 0,
         "Source type7 epsilon failed");
      ck(!query_ground_position(room, {}, {5, -.0002f, 5}).wall,
         "Source type7 lower epsilon exceeded");
      auto water = wall(3);
      water.parameters[0] = 1;
      room.floors = {water};
      ck(query_water(room, {}, {10, 10, 10}) == 0,
         "Water source inclusive boundary failed");
      room.floors[0].parameters[0] = 0;
      ck(query_water(room, {water}, {10, 10, 10}) == 1,
         "Water dynamic first match failed");
      room.floors[0].parameters[0] = 1;
      ck(query_water(room, {water}, {0, 0, 0}) == 0,
         "Water static priority failed");
      ck(!query_water(room, {}, {10, 10.01f, 10}), "Water Y boundary exceeded");
      std::cout << "Source ground and water queries passed\n";
    } else {
      auto s = fixture();
      tick_zombie_kami_effect(s, 0, 28);
      ck(s.effect_pool && active(*s.effect_pool) == 3,
         "Kami first event must allocate blood plus both particle sets");
      const auto &p = *s.effect_pool;
      ck(p.works[0].id == 5 && p.works[0].type == 1 &&
             p.works[0].flags == 0x81 && p.works[0].linked_parent == 0 &&
             p.works[0].linked_joint == 11 && p.works[0].offset.z == -1.8f &&
             p.works[0].scale.x == 1.5f,
         "Source blood1 linked work failed");
      ck(p.works[1].id == 302 && p.works[1].type == 1 &&
             p.works[1].particle_storage == std::array<size_t, 2>{2, 3} &&
             p.works[2].flags == 2 && p.works[3].flags == 2 &&
             p.works[4].particle_storage == std::array<size_t, 2>{5, 6},
         "Particle three-slot reservations/order failed");
      ck(p.works[1].motion_color == 0xff5f0000 &&
             p.works[4].motion_color == 0xff530000 &&
             p.works[4].texture_slot == 4 && p.works[4].particle_sprite,
         "Particle source color or logical texture slot failed");
      ck(s.enemies[0].damage_joint == 11 &&
             s.enemies[0].damage_velocity.x == -1 && s.random_seed == 1,
         "Blood source joint/direction or creation RNG changed");
      ck(std::abs(p.works[1].direction.x * p.works[1].direction.x +
                  p.works[1].direction.y * p.works[1].direction.y +
                  p.works[1].direction.z * p.works[1].direction.z - 1) <
             .00001f,
         "Source particle direction normalization failed");
      tick_zombie_kami_effect(s, 0, 38);
      ck(active(*s.effect_pool) == 4, "Later Kami event must omit particles");
      const auto retained = s.effect_pool;
      s.enemies[0].motion_number = 0;
      s.enemies[0].model_work.reset();
      tick_zombie_kami_effect(s, 0, 28);
      tick_zombie_water_effect(s, 0, 8);
      ck(s.effect_pool == retained,
         "Inactive effects must ignore unused caches/water context");
      s = fixture();
      s.enemies[0].motion_number = 111;
      tick_zombie_kami_effect(s, 0, -1);
      ck(active(*s.effect_pool) == 3,
         "Kami frame minus1 must remain a literal match");
      s = fixture();
      s.enemies[0].motion_number = 0;
      s.enemies[0].state_flags = 0x100000;
      tick_zombie_water_effect(s, 0, 8);
      ck(s.effect_pool && active(*s.effect_pool) == 4 &&
             s.effect_pool->works[0].id == 108 &&
             s.effect_pool->works[0].linked_joint == 10 &&
             s.effect_pool->works[0].position.x == 10,
         "Source water ring-before-splash order failed");
      for (size_t i = 1; i < 4; ++i)
        ck(s.effect_pool->works[i].id == 11 &&
               s.effect_pool->works[i].type == 1 &&
               s.effect_pool->works[i].linked_joint == int(i - 1) &&
               s.effect_pool->works[i].position.y == 5 &&
               s.effect_pool->works[i].scale.x == .8f,
           "Source water splash tuple failed");
      ck(s.effect_pool->works[1].angles[1] == -32768 &&
             s.effect_pool->works[2].angles[1] == 0 &&
             s.effect_pool->works[3].angles[1] == -16384 &&
             s.effect_pool->ground_wall == 0,
         "Source splash yaw/ground scratch failed");
      s = fixture();
      s.enemies[0].motion_number = 33;
      s.enemies[0].state_flags = 0x100000;
      tick_zombie_water_effect(s, 0, 12);
      ck(active(*s.effect_pool) == 8 && s.effect_pool->works[4].id == 108,
         "Duplicate water events must both emit");
      ck(s.effect_pool->works[1].scale.x == 2 &&
             s.effect_pool->works[1].position.z == 40 &&
             s.effect_pool->works[5].position.z == 38,
         "Source yaw offset/scaling failed");
      auto prior = s.effect_pool;
      tick_zombie_water_effect(s, 0, -1);
      ck(s.effect_pool == prior,
         "Water frame minus1 must terminate, not match");
      s = fixture();
      auto full = std::make_shared<NativeEffectPool>();
      for (auto &e : full->works)
        e.flags = 1;
      s.effect_pool = full;
      tick_zombie_kami_effect(s, 0, 28);
      ck(active(*s.effect_pool) == 512 && s.enemies[0].damage_joint == 11 &&
             s.enemies[0].damage_velocity.x == -1,
         "Pool full must retain source scalar side effects");
      s = fixture();
      full = std::make_shared<NativeEffectPool>();
      for (auto &e : full->works)
        e.flags = 1;
      for (size_t i : {0u, 1u, 2u, 3u, 4u, 5u})
        full->works[i].flags = 0;
      s.effect_pool = full;
      tick_zombie_kami_effect(s, 0, 28);
      ck(s.effect_pool->works[0].id == 5 && s.effect_pool->works[1].id == 302 &&
             s.effect_pool->works[2].flags == 2 &&
             s.effect_pool->works[3].flags == 2 &&
             s.effect_pool->works[4].flags == 0 &&
             s.effect_pool->works[5].flags == 0,
         "Partial particle failure must free only unfinished group");
      s = fixture();
      s.enemies[0].motion_number = 33;
      s.enemies[0].state_flags = 0x100000;
      s.enemies[0].model_work->joints[9].matrix[3] =
          std::numeric_limits<double>::quiet_NaN();
      bad([&] { tick_zombie_water_effect(s, 0, 12); });
      ck(!s.effect_pool,
         "Invalid Water event partially committed effect allocations");
      s = fixture();
      auto stale = std::make_shared<NativeEffectPool>();
      stale->scratch.angles = {123, 456};
      stale->scratch.floor = -7;
      s.effect_pool = stale;
      tick_zombie_kami_effect(s, 0, 38);
      ck(s.effect_pool->works[0].angles == std::array<int32_t, 3>{123, 0, 0} &&
             s.effect_pool->works[0].floor == -7 &&
             s.effect_pool->scratch.angles[1] == 0 &&
             stale->works[0].flags == 0,
         "Blood must retain EF scratch angleX/floor and immutable prior pool");
      s.enemies[0].motion_number = 0;
      s.enemies[0].state_flags = 0x100000;
      tick_zombie_water_effect(s, 0, 8);
      ck(s.effect_pool->works[2].angles[0] == 123 &&
             s.effect_pool->works[2].floor == 0 &&
             s.effect_pool->scratch.angles[1] == -16384,
         "Water must retain scratch angleX but overwrite floor/yaw");
      s = fixture();
      s.enemies[1] = s.enemies[0];
      s.enemies[1].pose_owner = 0;
      s.enemies[1].model_work.reset();
      s.enemies[1].motion_number = 0;
      s.enemies[1].state_flags = 0x100000;
      tick_zombie_water_effect(s, 1, 8);
      ck(s.effect_pool->works[0].position.x == 10,
         "Linked work must use shared owner cached root translation");
      s.enemies[1].motion_number = 8;
      tick_zombie_kami_effect(s, 1, 28);
      ck(s.effect_pool->works[4].linked_parent == 1 &&
             s.effect_pool->works[5].linked_parent == 1 &&
             s.enemies[1].damage_joint == 11 && s.enemies[0].damage_joint == 5,
         "Blood linkage and damage fields must belong to current work");
      s = fixture();
      full = std::make_shared<NativeEffectPool>();
      for (auto &effect : full->works)
        effect.flags = 1;
      s.effect_pool = full;
      s.enemies[0].motion_number = 0;
      s.enemies[0].state_flags = 0x100000;
      s.enemies[0].model_work->joints[0].matrix[3] =
          std::numeric_limits<double>::quiet_NaN();
      tick_zombie_water_effect(s, 0, 8);
      ck(active(*s.effect_pool) == 512 && s.effect_pool->scratch.id == 11,
         "Full pool ring must ignore unused root coordinates, retain splash "
         "scratch");
      s = fixture();
      s.enemies[0].motion_number = 0;
      s.enemies[0].state_flags = 0x100000;
      s.collision->floors.clear();
      tick_zombie_water_effect(s, 0, 8);
      ck(active(*s.effect_pool) == 1 && s.effect_pool->works[0].id == 108 &&
             s.effect_pool->ground_wall == 0 && s.effect_pool->scratch.id == 0,
         "Dry splash location must retain prior ring and ground query only");
      std::cout << "Native source Kami/Water effect creation passed\n";
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
