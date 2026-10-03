#include "effects.hpp"
#include "stateful_wall.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Malformed stateful dependency accepted");
}
CollisionRecord box() {
  CollisionRecord r;
  r.flags = 1;
  r.size = {10, 10, 10};
  return r;
}
ScenarioState fixture() {
  ScenarioState s;
  s.collision.emplace();
  s.collision->height = 10;
  s.flags[6][0] = 8;
  return s;
}
ScenarioEntity player() {
  ScenarioEntity p;
  p.flags = 1;
  p.geometry.radius = 3;
  p.geometry.height = 10;
  return p;
}
void rectangle() {
  auto s = fixture();
  s.collision->walls = {box()};
  auto p = player();
  auto r = collide_stateful_walls_ex(s, p, p, {5, 0, 5}, {-5, 0, 5}, 1, 10);
  ck(r.hits == 1 && r.position.x == -1 && (p.state_flags & 1),
     "Source rectangle previous face/state1");
  for (size_t facing = 0; facing < 4; ++facing) {
    s = fixture();
    auto w = box();
    w.attribute = 0x10000;
    s.collision->walls = {w};
    p = player();
    p.angles[1] = int32_t(facing * 16384);
    p.push_count = 7;
    p.state_flags = 0x80;
    collide_stateful_walls_ex(s, p, p, {5, 0, 5}, {-5, 0, 5}, 1, 10);
    ck(p.push_count == 8 && p.push_index == 0 &&
           s.collision->walls[0].attribute == 0x30000,
       "Source push probe or stamp/index");
    const uint32_t masks[] = {0x40000, 0x200000, 0x100000, 0x80000};
    s = fixture();
    w.attribute = 0x10000 | masks[facing];
    s.collision->walls = {w};
    p.push_count = 7;
    p.state_flags = 0;
    collide_stateful_walls_ex(s, p, p, {5, 0, 5}, {-5, 0, 5}, 1, 10);
    ck(p.push_count == 0, "Forbidden push direction not reset");
  }
  s = fixture();
  s.collision->walls = {box()};
  p = player();
  p.push_count = 7;
  collide_stateful_walls_ex(s, p, p, {-.8f, 0, -.8f}, {-2, 0, -2}, 1, 10);
  ck(p.push_count == 0, "Rounded branch must reset even outside corner radius");
  s = fixture();
  s.dynamic_walls = {box()};
  p = player();
  p.state_flags = 0x80;
  p.flags = 0x101;
  r = check_enemy_wall2(s, p, p, {5, 0, 5}, {-5, 0, 5}, 1, 10);
  ck(r.wall == 0 && p.push_index == 0 && p.flags == 0x101 &&
         r.displacement.x == -6,
     "Dynamic merged query/original proposal/flag restoration");
  s = fixture();
  s.collision->ground[2] = 8;
  p = player();
  p.flags |= 0x100;
  r = collide_stateful_walls_ex(s, p, p, {5, 2, 5}, {5, 2, 5}, 1, 10);
  ck(r.position.y == 8, "Ex no-hit ground snap");
  r = check_enemy_wall2(s, p, p, {5, 2, 5}, {5, 2, 5}, 1, 10);
  ck(r.position.y == 2 && p.flags == 0x101,
     "Wall2 must suppress snap then restore flags");
  s = fixture();
  auto ignored = box();
  ignored.type = 6;
  ignored.position.x = std::numeric_limits<float>::quiet_NaN();
  s.collision->walls = {ignored};
  ignored.type = 255;
  s.dynamic_walls = {ignored};
  ck(!collide_stateful_walls_ex(s, p, p, {5, 0, 5}, {5, 0, 5}, 1, 10).hits,
     "Source skipped shape consumed");
}
void circle() {
  for (bool positive : {false, true}) {
    auto s = fixture();
    auto w = box();
    w.type = 2;
    w.size.x = 2;
    w.attribute = 0x40;
    s.collision->walls = {w};
    auto p = player();
    p.state_flags = 0x40000000;
    p.position = {7, 8, 9};
    p.angles[0] = 123;
    auto pool = std::make_shared<NativeEffectPool>();
    pool->scratch.id = 370;
    pool->works[0].flags = 2;
    pool->works[1].flags = 1;
    pool->works[1].id = 999;
    s.effect_pool = pool;
    auto r = collide_stateful_walls_ex(s, p, p, {0, 0, positive ? 1.f : -1.f},
                                       {0, 0, 0}, 1, 10);
    ck(r.hits == 1 &&
           p.modes ==
               std::array<uint8_t, 4>{2, uint8_t(positive ? 0 : 1), 0, 0} &&
           p.flags == 0x10005 && s.flags[5][0] == 4,
       "Circle packed damage modes/flags");
    ck(s.collision->walls[0].attribute == 0 && s.effect_pool != pool &&
           s.effect_pool->scratch.id == 370 &&
           s.effect_pool->works[1].id == 999,
       "Circle effect allocation/stamp/scratch");
    const auto &e = s.effect_pool->works[2];
    uint32_t offset;
    std::memcpy(&offset, &e.offset.x, 4);
    ck(e.flags == 1 && e.id == 29 && e.linked_wall == 0 && !e.linked_parent &&
           e.texture_id == -1 && e.type == 0 && e.floor == 0 &&
           e.model_version == 0 && e.scale.x == 1 && e.scale.y == 1 &&
           e.scale.z == 1 && e.base_scale.x == 0 && e.control0 == 0 &&
           offset == 123 && e.position.x == 7 && e.position.y == 8 &&
           e.position.z == 9,
       "Complete source wall-timer birth");
    for (int i = 0; i < 31; ++i)
      tick_wall_damage_timers(s);
    ck(s.effect_pool->works[2].control0 == 31 &&
           s.effect_pool->works[2].flags == 1 &&
           s.collision->walls[0].attribute == 0,
       "Timer restored early");
    s.collision->walls[0].flags = 0;
    tick_wall_damage_timers(s);
    ck(s.effect_pool->works[2].flags == 0 &&
           s.collision->walls[0].attribute == 0x40,
       "Timer expiry/inactive wall restore");
  }
  auto s = fixture();
  auto w = box();
  w.type = 2;
  w.size.x = 2;
  w.attribute = 0x40;
  s.dynamic_walls = {w};
  auto p = player();
  p.state_flags = 0x40000000;
  auto pool = std::make_shared<NativeEffectPool>();
  for (auto &e : pool->works)
    e.flags = 1;
  s.effect_pool = pool;
  check_enemy_wall2(s, p, p, {0, 0, 1}, {0, 0, 0}, 1, 10);
  ck(s.dynamic_walls[0].attribute == 0x40 && p.flags == 1 && p.modes[0] == 2 &&
         s.effect_pool == pool,
     "Full pool or same-work flag restoration");
  s = fixture();
  s.dynamic_walls = {w};
  p = player();
  p.flags = 4;
  p.state_flags = 0x40000000;
  collide_stateful_walls_ex(s, p, p, {0, 0, 1}, {0, 0, 0}, 1, 10);
  ck(!s.effect_pool && p.modes[0] == 0,
     "Global playerflags6 did not suppress damage");
  s = fixture();
  s.dynamic_walls = {w};
  auto actor = player();
  actor.state_flags = 0x40000000;
  p = player();
  check_enemy_wall2(s, actor, p, {0, 0, -1}, {0, 0, 0}, 1, 10);
  ck(actor.flags == 1 && p.flags == 0x10005 &&
         s.effect_pool->works[0].linked_wall == 0,
     "Separate player/work damage or dynamic linkage");
}
void steps_and_rollback() {
  auto s = fixture();
  auto a = box();
  a.type = 2;
  a.parameters[0] = 1;
  a.size = {4, 1, 4};
  s.collision->miscellaneous = {a};
  auto p = player();
  p.position = {2, 0, 2};
  p.state_flags = 0x80;
  p.flags = 0x101;
  const auto r = check_enemy_wall2(s, p, p, {5, 0, 2}, {2, 0, 2}, 1, 10);
  ck(r.hits == 1 && r.position.x == 4.5f && !r.wall && p.step_area == 0 &&
         p.push_index == 1 && p.state_flags == 0x20081 &&
         s.dynamic_walls.empty(),
     "Source actual-position/radius step limits or cleanup");
  s = fixture();
  auto malformed = box();
  malformed.attribute = 1;
  malformed.floor = 9;
  malformed.position.x = std::numeric_limits<float>::quiet_NaN();
  s.collision->walls = {malformed, box()};
  p = player();
  p.flags = 0x101;
  p.state_flags = 0x80;
  p.push_count = 17;
  bad([&] { check_enemy_wall2(s, p, p, {5, 0, 5}, {-5, 0, 5}, 1, 10); });
  ck(p.state_flags == 0x80 && p.flags == 0x101 && p.push_count == 17 &&
         !p.push_index && s.collision->walls[1].attribute == 0 &&
         !s.effect_pool,
     "Late type query failed to roll back state/attrs");
}
void timer_gates() {
  auto s = fixture();
  s.dynamic_walls = {box()};
  auto pool = std::make_shared<NativeEffectPool>();
  auto &e = pool->works[0];
  e.flags = 1;
  e.id = 29;
  e.control0 = 31;
  e.linked_wall = 0;
  s.effect_pool = pool;
  s.flags[6][0] = 0;
  tick_wall_damage_timers(s);
  ck(s.effect_pool == pool, "Disabled effect gate consumed timer");
  s.effect_flags = 1;
  e.state_flags = 0x1000000;
  tick_wall_damage_timers(s);
  ck(s.effect_pool == pool, "Frozen timer advanced");
  e.state_flags = 0;
  e.flags = 2;
  tick_wall_damage_timers(s);
  ck(s.effect_pool == pool, "Reserved/inactive timer advanced");
  e.flags = 1;
  pool->works[1] = e;
  pool->works[1].linked_wall = 99;
  bad([&] { tick_wall_damage_timers(s); });
  ck(s.effect_pool == pool && s.dynamic_walls[0].attribute == 0,
     "Late expiry failure not atomic");
  pool->works[1].flags = 0;
  tick_wall_damage_timers(s);
  ck(s.dynamic_walls[0].attribute == 0x40 && s.effect_pool->works[0].flags == 0,
     "Effectflag gate/dynamic timer expiry");
}
void dynamic_shapes() {
  // Literal original shape fixtures exercise the new merged response loop,
  // including odd-type flags, triangle IDs and grounded/airborne ledges.
  for (uint8_t type : {0, 1, 2, 3, 4, 5, 7}) {
    for (uint8_t id = 0; id < (type == 4 || type == 5 ? 4 : 1); ++id) {
      auto s = fixture();
      auto w = box();
      w.type = type;
      w.id = id;
      auto p = player();
      Vec3 point{-0.5f, 0, 5}, prior = point, expected{-1, 0, 5};
      if (type == 2 || type == 3) {
        w.size.x = 2;
        point = {0, 0, 1};
        prior = point;
        expected = {0, 0, 3};
      }
      if (type == 4 || type == 5) {
        const float sx = id >= 2 ? -1.f : 1.f,
                    sz = id == 1 || id == 3 ? -1.f : 1.f;
        w.size.x = 10 * sx;
        w.size.z = 10 * sz;
        point = {2 * sx, 0, 2 * sz};
        prior = point;
        expected = {6 * sx, 0, 6 * sz};
      }
      if (type == 7) {
        p.flags |= 0x100;
        point = {5, 9, 5};
        prior = point;
        expected = {11, 9, 5};
      }
      s.dynamic_walls = {w};
      const auto r = collide_stateful_walls_ex(s, p, p, point, prior, 1, 2);
      ck(r.hits == 1 && std::abs(r.position.x - expected.x) < .0001f &&
             r.position.y == expected.y &&
             std::abs(r.position.z - expected.z) < .0001f &&
             (p.state_flags & 1),
         "Literal dynamic wall shape response");
      if (type == 1 || type == 3 || type == 5) {
        p.flags = 0x400;
        p.state_flags = 0;
        ck(!collide_stateful_walls_ex(s, p, p, point, prior, 1, 2).hits,
           "Odd dynamic shape actorflag400 filter");
      }
    }
  }
}
int main() {
  try {
    rectangle();
    circle();
    steps_and_rollback();
    timer_gates();
    dynamic_shapes();
    std::cout << "Stateful source WallEx/Wall2, step cleanup and Effect29 "
                 "timer pass\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
