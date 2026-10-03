#include "zombie.hpp"
#include <cmath>
#include <iostream>
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
  ck(failed, "Malformed frame collision accepted");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  s.collision->height = 10;
  auto &e = s.enemies[0];
  e.flags = 1;
  e.id = 1;
  e.zombie_work.emplace();
  e.position = {2, 0, 5};
  e.previous_position = {-4, 0, 5};
  e.geometry.radius = e.geometry.collision_radius = 1;
  e.geometry.height = e.geometry.collision_height = 2;
  e.joint_pose.resize(18);
  e.model_work.emplace();
  e.model_work->joints.resize(18);
  for (size_t i = 0; i < 18; ++i) {
    e.joint_pose[i].position = Vec3{};
    e.joint_pose[i].angles = std::array<int32_t, 3>{};
    e.model_work->joints[i].matrix[3] = -4;
    e.model_work->joints[i].matrix[11] = 5;
  }
  return s;
}
ScenarioEntity player() {
  ScenarioEntity p;
  p.flags = 9;
  p.position = {100, 0, 100};
  p.geometry.collision_radius = 1;
  p.geometry.collision_height = 2;
  return p;
}
CollisionRecord box() {
  CollisionRecord r;
  r.flags = 1;
  r.size = {10, 10, 10};
  return r;
}
void body(ScenarioState &s) {
  auto &b = s.enemies[2];
  b.flags = 0x81;
  b.id = 1;
  b.linked_parent = 0;
  b.pose_owner = 0;
  b.zombie_work.emplace();
  b.zombie_flags = 0x80000000;
  s.enemies[0].zombie_work->body = 2;
}
void contacts() {
  {
    auto s = fixture();
    auto p = player();
    body(s);
    auto &e = s.enemies[0];
    e.flags = 0x49;
    e.position = {};
    e.model_work->joints[0].matrix[3] = 0;
    e.model_work->joints[0].matrix[11] = 0;
    p.flags = 1;
    p.player_work.emplace();
    p.player_work->normal_anchor = {1, 0, 0};
    p.player_work->movement_anchor = {88, 0, 88};
    tick_zombie_collision(s, 2, p);
    ck(e.position.x == -.5f && p.position.x == 100.5f,
       "CheckPlayer must use source nlxb, not player position/movement anchor");
  }
  for (bool force : {false, true}) {
    auto s = fixture();
    auto p = player();
    body(s);
    auto &e = s.enemies[0];
    e.position = {};
    e.flags = 0x49;
    e.zombie_flags = force ? 0x40000 : 0;
    e.model_work->joints[0].matrix[3] = 0;
    e.model_work->joints[0].matrix[11] = 0;
    p.position = {1, 0, 0};
    tick_zombie_collision(s, 2, p);
    ck(e.position.x == -.5f && p.position.x == 1.5f && (e.state_flags & 4) &&
           (p.state_flags & 2),
       "Linked body parent/player contact");
    ck(bool(e.secondary_flags & 1) == force,
       "Parent40000 source secondary1 result");
    ck(s.enemies[2].position.x == 0,
       "Linked body work must not replace parent position");
  }
  {
    auto s = fixture();
    auto p = player();
    body(s);
    s.enemies[0].flags |= 2;
    s.enemies[0].model_work.reset();
    s.collision.reset();
    p.position = {NAN, NAN, NAN};
    tick_zombie_collision(s, 2, p);
    ck(s.enemies[0].position.x == 2, "Paused parent consumed collision data");
  }
  {
    auto s = fixture();
    auto p = player();
    body(s);
    s.enemies[0].flags = 0x49;
    s.enemies[0].position = {};
    s.enemies[0].zombie_flags = 0x40;
    p.position = {1, 0, 0};
    tick_zombie_collision(s, 2, p);
    ck(p.position.x == 1 && s.enemies[0].position.x == 0,
       "Source40 gate must suppress player/pool contacts");
  }
}
void push() {
  for (bool wall : {false, true})
    for (bool rear : {false, true}) {
      auto s = fixture();
      auto p = player();
      body(s);
      auto &e = s.enemies[0];
      e.flags = 0x49;
      e.position = {};
      e.zombie_flags = 0x100;
      auto &other = s.enemies[1];
      other.flags = 9;
      other.id = 1;
      other.position = {0, 0, 1};
      other.geometry = e.geometry;
      other.zombie_work.emplace();
      other.modes = {1, 9, 8, 7};
      other.angles[1] = rear ? 32768 : 0;
      other.state_flags = wall ? 3 : 2;
      s.enemies[2].modes = {6, 7, 8, 9};
      tick_zombie_collision(s, 2, p);
      ck(e.position.z == (wall ? -1.f : 0.f),
         "Push whit-conditioned position restoration");
      ck(other.modes ==
                 std::array<uint8_t, 4>{3, 0, uint8_t(rear ? 3 : 4), 0} &&
             (other.zombie_flags & 0x200) &&
             bool(other.zombie_flags & 0x2000) == !rear,
         "Push facing/damage mode source mutation");
      ck(other.damage_velocity.y == 0 &&
             std::abs(other.damage_velocity.z - (rear ? 1.f : -1.f)) < .00001f,
         "Push source negative yaw velocity");
      ck(s.enemies[2].modes == std::array<uint8_t, 4>{3, 7, 8, 9},
         "Push must write pusher body mode0 only");
    }
  {
    auto s = fixture();
    auto p = player();
    body(s);
    auto &e = s.enemies[0];
    e.flags = 0x49;
    e.position = {};
    e.zombie_flags = 0x100;
    auto &o = s.enemies[1];
    o.flags = 9;
    o.id = 1;
    o.geometry = e.geometry;
    o.position = {1, 0, 0};
    o.zombie_work.emplace();
    o.modes = {1, 1, 0, 0};
    tick_zombie_collision(s, 2, p);
    ck(o.modes[2] == 2, "Original EnemyHitChk OR predicate side branch");
  }
  {
    auto s = fixture();
    auto p = player();
    body(s);
    auto &e = s.enemies[0];
    e.flags = 0x49;
    e.position = {};
    e.zombie_flags = 0x100;
    auto &o = s.enemies[1];
    o.flags = 9;
    o.id = 26;
    o.geometry = e.geometry;
    o.position = {0, 0, 1};
    o.zombie_work.emplace();
    o.modes = {1, 1, 0, 0};
    tick_zombie_collision(s, 2, p);
    ck(o.modes[0] == 3 && o.modes[2] == 2, "Source ID26 push damage selection");
  }
  for (uint16_t type : {2, 3, 6, 7, 9, 10}) {
    auto s = fixture();
    auto p = player();
    body(s);
    auto &e = s.enemies[0];
    e.flags = 0x49;
    e.position = {};
    e.zombie_flags = 0x100;
    auto &o = s.enemies[1];
    o.flags = 9;
    o.id = 1;
    o.geometry = e.geometry;
    o.position = {0, 0, 1};
    o.zombie_work.emplace();
    o.type = type;
    o.modes = {1, 1, 0, 0};
    tick_zombie_collision(s, 2, p);
    ck(o.modes[0] == 1 && !(o.zombie_flags & 0x200),
       "Push excluded type changed mode");
  }
}
void shapes() {
  {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.flags = 0x11;
    e.zombie_flags = 0x40000003;
    e.model_work->joints[1].matrix[7] = NAN;
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(e.position.x == -1.5f, "Prior-Y override must ignore cached Y");
  }
  for (uint32_t shape = 0; shape < 7; ++shape) {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.flags = 0x11;
    e.zombie_flags = 0x40000000 | 0x8000000 | shape;
    e.zombie_work->damage_80 = -1;
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(e.position.x == (shape == 0   ? 2.f
                        : shape == 3 ? -1.5f
                        : shape == 6 ? -4.f
                                     : -1.f),
       "Source posture-specific wall displacement");
    ck(e.position.y == 0 && e.position.z == 5,
       "Wall response changed unused components");
  }
  {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.flags = 0x11;
    e.zombie_flags = 0x40000001;
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(e.position.x == -2, "Body correction must precede rebuilt head probe");
  }
  {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.flags = 0x11;
    e.zombie_flags = 0x40000002;
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(e.position.x == -3, "Source paired arm mean-probe wall correction");
  }
  for (uint16_t type : {2, 3, 9}) {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.flags = 0x11;
    e.type = type;
    e.zombie_flags = 0x40000001;
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(e.position.x == -4, "Crawling posture retains two precomputed probes");
    s = fixture();
    auto &w = s.enemies[0];
    w.flags = 0x11;
    w.type = type;
    w.zombie_flags = 0x40020001;
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(w.position.x == -1, "Source20000 suppresses second crawling probe");
  }
  {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.position = {-4, 0, 5};
    e.zombie_flags = 0x40000000;
    e.model_work.reset();
    e.joint_pose.clear();
    p.position = {4, 0, 5};
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(e.zombie_work->path_blocked && e.position.x == -4,
       "Shape0 must update line blockage without pose/cache response");
    s.collision->walls.clear();
    tick_zombie_collision(s, 0, p);
    ck(!e.zombie_work->path_blocked,
       "Clear line must replace remembered blockage");
  }
  {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.flags = 1;
    e.zombie_flags = 0x40000001;
    e.model_work.reset();
    e.joint_pose.clear();
    tick_zombie_collision(s, 0, p);
    ck(e.position.x == 2, "Shape1 without flag10 must not consume pose/cache");
  }
  {
    auto s = fixture();
    auto p = player();
    auto &e = s.enemies[0];
    e.flags = 0x11;
    e.zombie_flags = 0x40000004;
    e.pose_owner = 3;
    auto &owner = s.enemies[3];
    owner.flags = 1;
    owner.joint_pose = e.joint_pose;
    owner.model_work = e.model_work;
    e.model_work.reset();
    e.joint_pose.clear();
    s.collision->walls = {box()};
    tick_zombie_collision(s, 0, p);
    ck(e.position.x == -1, "Posture must consume actual pose/cache owner");
  }
}
void floors() {
  auto s = fixture();
  auto p = player();
  auto &e = s.enemies[0];
  e.position = {};
  CollisionRecord floor;
  floor.flags = 1;
  floor.type = 3;
  floor.position = {-1, NAN, -1};
  floor.size = {2, NAN, 2};
  floor.parameters[0] = 2;
  s.collision->floors = {floor};
  s.flags[5][0] = 0x100;
  e.model_flags = 0x10;
  p.position = {NAN, NAN, NAN};
  tick_zombie_collision(s, 0, p);
  ck(e.model_flags == 0x50, "Inclusive floor-effect source match ignores Y");
  floor.parameters[0] = 0;
  s.collision->floors.insert(s.collision->floors.begin(), floor);
  tick_zombie_collision(s, 0, p);
  ck(e.model_flags == 0x10,
     "Floor effect first match must suppress later wet surface");
  s.collision->floors.clear();
  floor.parameters[0] = 2;
  s.dynamic_floors = {floor};
  tick_zombie_collision(s, 0, p);
  ck(e.model_flags == 0x50, "Dynamic floor-effect source match");
  s.flags[5][0] = 0x40;
  floor.parameters[0] = 1;
  floor.floor = 7;
  floor.position.y = -1;
  floor.size.y = 1;
  s.dynamic_floors = {floor};
  tick_zombie_collision(s, 0, p);
  ck(e.state_flags & 0x100000,
     "Water border/current-floor-independent predicate");
  e.position = {NAN, NAN, NAN};
  s.collision.reset();
  s.dynamic_floors.clear();
  tick_zombie_collision(s, 0, p);
  ck(e.state_flags & 0x100000,
     "Already-in-water gate consumed missing room/position");
  {
    auto x = fixture();
    auto q = player();
    body(x);
    x.enemies[0].flags |= 2;
    x.flags[5][0] = 0x140;
    floor.position = {-1, -1, -1};
    floor.size = {2, 2, 2};
    x.collision->floors = {floor};
    q.position = {NAN, NAN, NAN};
    tick_zombie_collision(x, 2, q);
    ck((x.enemies[2].state_flags & 0x100000) &&
           !(x.enemies[0].state_flags & 0x100000),
       "Water callback must update incoming body rather than collision parent");
  }
}
void rollback() {
  auto s = fixture();
  auto p = player();
  body(s);
  p.position = {3, 0, 5};
  auto &e = s.enemies[0];
  e.flags = 0x59;
  e.zombie_flags = 1;
  e.model_work->joints[0].matrix[3] = e.position.x;
  e.model_work->joints[0].matrix[11] = e.position.z;
  s.collision->walls = {box()};
  {
    auto success = s;
    auto q = p;
    tick_zombie_collision(success, 2, q);
    ck(q.position.x == 3.5f && success.enemies[0].position.x < 0,
       "Rollback fixture must first exercise real contact and wall response");
  }
  e.model_work->joints.resize(
      1); // Fail only the head cache, after contact/wall.
  const auto original = p.position;
  bad([&] { tick_zombie_collision(s, 2, p); });
  ck(e.position.x == 2 && p.position.x == original.x && !e.state_flags &&
         !p.state_flags && s.collision->walls[0].attribute == 0 &&
         !s.effect_pool,
     "Late posture failure committed contact/wall/player state");
  bad([&] { tick_zombie_collision(s, 128, p); });
  bad([&] { tick_zombie_collision(s, 0, s.enemies[0]); });
  s.enemies[2].linked_parent = 127;
  bad([&] { tick_zombie_collision(s, 2, p); });
}
int main() {
  try {
    ScenarioState state;
    state.enemies.resize(128);
    state.enemies[0].flags = 1;
    state.enemies[0].id = 1;
    state.enemies[0].zombie_work.emplace();
    ScenarioEntity player;
    tick_zombie_collision(state, 0, player);
    contacts();
    push();
    shapes();
    floors();
    rollback();
    std::cout << "Native zombie frame collision checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
