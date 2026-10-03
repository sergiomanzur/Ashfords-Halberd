#include "room.hpp"
#include "zombie.hpp"
#include <cmath>
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
  ck(failed, "Malformed pursuit context did not reject");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  auto &e = s.enemies[0];
  e.flags = 1;
  e.id = 1;
  e.zombie_work.emplace();
  e.position = {5, 5, 5};
  e.floor = 2;
  e.modes = {1, 1, 2, 3};
  e.model_work.emplace();
  e.model_work->joints.resize(12);
  e.joint_pose.resize(12);
  e.joint_pose[11].angles = std::array<int32_t, 3>{0, 0, 0};
  // Low31 bypasses the first visibility query, then increments to pending20.
  e.zombie_work->awareness = 31;
  e.zombie_flags = 0x400;
  e.zombie_work->target_x = 99;
  e.zombie_work->target_z = 98;
  auto routes = std::make_shared<RoomRoutes>();
  for (float x : {0.f, 20.f, 40.f}) {
    CollisionRecord r;
    r.position = {x, 0, 0};
    r.size = {10, 10, 10};
    routes->regions.push_back(r);
  }
  routes->next_hops = {255, 0, 0, 1, 255, 1, 1, 2, 255};
  s.routes = routes;
  return s;
}
ScenarioEntity player() {
  ScenarioEntity p;
  p.position = {45, 5, 5};
  p.floor = 2;
  return p;
}
CollisionRecord stair() {
  CollisionRecord r;
  r.flags = 1;
  r.type = 1;
  r.floor = 2;
  r.position = {0, 0, 0};
  r.size = {10, 10, 10};
  r.parameters = {0, 0, 4, 255};
  return r;
}
int main() {
  try {
    auto s = fixture();
    auto p = player();
    tick_zombie_brain(s, 0, p);
    auto &e = s.enemies[0];
    auto &w = *e.zombie_work;
    ck(w.target_x == 45 && w.target_z == 5 && w.route_id == 0 &&
           w.route_timer == 0 && w.awareness == 0x20 &&
           w.awareness_timeout == 255 && e.zombie_flags == 0x400,
       "Direct pursuit/awareness byte order failed");
    ck(e.position.x == 5 && e.frame == 0 &&
           e.modes == std::array<uint8_t, 4>{1, 1, 2, 3} && s.random_seed == 1,
       "Pursuit changed unrelated actor/RNG work");
    for (int reason = 0; reason < 3; ++reason) {
      s = fixture();
      auto &z = *s.enemies[0].zombie_work;
      if (reason == 0)
        z.player_distance = 50;
      if (reason == 1)
        z.path_blocked = true;
      if (reason == 2)
        p.floor = 3;
      else
        p.floor = 2;
      tick_zombie_brain(s, 0, p);
      ck(z.target_x == 25 && z.target_z == 5 && z.route_id == 1 &&
             z.route_timer == 0,
         "Exact50/obstruction/floor route fallback failed");
    }
    p = player();
    s = fixture();
    auto &held = *s.enemies[0].zombie_work;
    held.player_distance = 50;
    held.route_id = 1;
    held.route_timer = 253;
    tick_zombie_brain(s, 0, p);
    ck(held.target_x == 99 && held.target_z == 98 && held.route_timer == 254,
       "Route cache advanced before region/timer gate");
    tick_zombie_brain(s, 0, p);
    ck(held.target_x == 25 && held.route_id == 1 && held.route_timer == 0,
       "Timer254->255 must refresh route");
    held.route_timer = 255;
    held.target_x = 77;
    tick_zombie_brain(s, 0, p);
    ck(held.route_timer == 0 && held.target_x == 77,
       "Timer255 must wrap before threshold comparison");
    s = fixture();
    s.enemies[0].zombie_work->player_distance = 50;
    p.position = {7, 5, 8};
    tick_zombie_brain(s, 0, p);
    ck(s.enemies[0].zombie_work->route_id == 255 &&
           s.enemies[0].zombie_work->target_x == 7 &&
           s.enemies[0].zombie_work->target_z == 8,
       "Shared-region255 must preserve destination output");
    p = player();
    s = fixture();
    s.enemies[0].type = 5;
    s.enemies[0].zombie_flags = 0;
    tick_zombie_brain(s, 0, p);
    ck(s.enemies[0].zombie_work->target_x == 25 &&
           s.enemies[0].zombie_flags == 0,
       "Type5 must route without notice or close pursuit shortcut");
    s = fixture();
    s.enemies[0].zombie_flags = 0;
    s.routes.reset();
    tick_zombie_brain(s, 0, p);
    ck(s.enemies[0].zombie_work->target_x == 99 &&
           s.enemies[0].zombie_work->route_timer == 0,
       "Unnoticed root consumed route data or changed target");
    s = fixture();
    s.enemies[0].zombie_flags = 0;
    s.enemies[0].zombie_work->awareness = 0;
    p.position = {0, 0, -10};
    tick_zombie_brain(s, 0, p);
    ck(s.enemies[0].zombie_flags == 0x400 &&
           s.enemies[0].zombie_work->awareness == 0x41 &&
           s.enemies[0].zombie_work->target_z == -10,
       "New awareness notice must feed same-frame pursuit");
    p = player();
    for (int malformed = 0; malformed < 3; ++malformed) {
      s = fixture();
      auto &z = *s.enemies[0].zombie_work;
      z.player_distance = 50;
      if (malformed == 0)
        s.routes.reset();
      if (malformed == 1)
        p.position = {45, 50, 5};
      else
        p = player();
      if (malformed == 2) {
        auto routes = std::make_shared<RoomRoutes>(*s.routes);
        routes->next_hops[6] = 255;
        s.routes = routes;
      }
      bad([&] { tick_zombie_brain(s, 0, p); });
      ck(z.awareness == 31 && z.awareness_timeout == 0 && z.target_x == 99 &&
             z.route_timer == 0 && s.enemies[0].zombie_flags == 0x400,
         "Malformed route partially committed awareness or pursuit");
    }
    p = player();
    for (int direction = 0; direction < 2; ++direction) {
      s = fixture();
      auto r = stair();
      r.parameters[0] = uint8_t(direction);
      r.parameters[3] = 1;
      s.collision->miscellaneous = {r, stair()};
      p.floor = direction ? 1 : 3;
      tick_zombie_brain(s, 0, p);
      ck(s.enemies[0].modes == std::array<uint8_t, 4>{1, 0, 8, 0} &&
             s.enemies[0].zombie_flags == 0x400400 &&
             s.enemies[0].stair_index == 0 &&
             s.enemies[0].zombie_work->stair_target == 0 &&
             (s.collision->miscellaneous[0].attribute & 0x400000) &&
             (s.collision->miscellaneous[1].attribute & 0x400000),
         "Source uphill/downhill stair mode and paired reservation failed");
    }
    for (int gate = 0; gate < 7; ++gate) {
      s = fixture();
      s.collision->miscellaneous = {stair(), stair()};
      p = player();
      p.floor = 3;
      if (gate == 0)
        p.flags = 4;
      if (gate == 1)
        p.state_flags = 0x80000000;
      if (gate == 2) {
        auto &other = s.enemies[1];
        other.flags = 1;
        other.id = 1;
        other.zombie_work.emplace();
        other.zombie_flags = 0x400000;
      }
      if (gate == 3)
        s.collision->miscellaneous[0].parameters[2] = 3;
      if (gate == 4)
        p.floor = 2;
      if (gate == 5)
        p.floor = 1;
      if (gate == 6)
        for (auto &r : s.collision->miscellaneous)
          r.attribute = 0x400000;
      tick_zombie_brain(s, 0, p);
      ck(s.enemies[0].modes == std::array<uint8_t, 4>{1, 1, 2, 3} &&
             !s.enemies[0].stair_index,
         "Original stair eligibility gate failed");
    }
    for (uint32_t flags : {0u, 3u, 0x81u}) {
      s = fixture();
      s.collision->miscellaneous = {stair(), stair()};
      p = player();
      p.floor = 3;
      auto &other = s.enemies[1];
      other.flags = flags;
      other.id = 1;
      other.zombie_work.emplace();
      other.zombie_flags = 0x400000;
      tick_zombie_brain(s, 0, p);
      ck(s.enemies[0].modes[2] == 8,
         "Ineligible competing zombie prevented stairs");
    }
    s = fixture();
    s.collision->miscellaneous = {stair(), stair()};
    p = player();
    p.floor = 3;
    s.enemies[0].flags |= 0x200000;
    tick_zombie_brain(s, 0, p);
    ck(s.enemies[0].zombie_flags == 0 && s.enemies[0].modes[2] == 2 &&
           s.enemies[0].stair_index == 0 &&
           s.enemies[0].zombie_work->stair_target == 0 &&
           !(s.collision->miscellaneous[0].attribute & 0x400000) &&
           !(s.collision->miscellaneous[1].attribute & 0x400000),
       "Source primary200000 must release new reservation and clear notice "
       "only");
    s = fixture();
    s.collision->miscellaneous = {stair()};
    p = player();
    p.floor = 3;
    bad([&] { tick_zombie_brain(s, 0, p); });
    ck(s.enemies[0].zombie_work->awareness == 31 && !s.enemies[0].stair_index &&
           s.enemies[0].zombie_work->target_x == 99 &&
           s.collision->miscellaneous[0].attribute == 0,
       "Malformed stair pair partially committed Brain00 state");
    std::cout << "Native zombie pursuit controller tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
