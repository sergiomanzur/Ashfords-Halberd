#include "zombie.hpp"
#include <cmath>
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
  ck(hit, "Invalid walk context did not reject");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  s.collision->height = 30;
  auto &e = s.enemies[0];
  e.id = 1;
  e.flags = 0x2040001;
  e.modes = {1, 0, 1, 0};
  e.zombie_work.emplace();
  e.zombie_work->personality = 1;
  e.zombie_work->body = 1;
  e.zombie_work->player_distance = 60;
  e.zombie_walk_choice = 2;
  e.zombie_flags = 0x3000400;
  e.motion_number = 9;
  e.frame = 17;
  e.blend_count = 17;
  e.blend_rate = 123;
  e.control1 = 999;
  e.control2 = 333;
  e.geometry.radius = 0;
  e.geometry.height = 2;
  e.previous_angles[1] = 777;
  auto &b = s.enemies[1];
  b.id = 1;
  b.flags = 0x2040081;
  b.zombie_work.emplace();
  b.zombie_flags = 0x83000000;
  b.motion_number = 201;
  return s;
}
CollisionRecord front() {
  CollisionRecord w;
  w.flags = 1;
  w.position = {-.5f, 0, -7};
  w.size = {1, 10, 2};
  return w;
}
int main() {
  try {
    ScenarioEntity player;
    player.position = {-10, 999, 0};
    auto s = fixture();
    tick_zombie_walk(s, 0, player);
    auto &e = s.enemies[0];
    auto &b = s.enemies[1];
    ck(e.motion_number == 40 && b.motion_number == 240 && e.frame == 0 &&
           b.frame == 0 && e.motion_step == 65536 && e.motion_mode == 0x20 &&
           e.blend_count == 8 && b.blend_count == 8 && e.blend_rate == 0 &&
           e.flags == 0x40001 && e.zombie_flags == 0x400 &&
           b.zombie_flags == 0x80000000,
       "Original root/body walk motion selection failed");
    ck(e.turn_speed == 256 && e.control0 == 237 && e.control1 == 0 &&
           e.control2 == 333 && e.control3 == 14 &&
           e.modes == std::array<uint8_t, 4>{1, 0, 1, 1} &&
           s.random_seed == 2524885223u && e.zombie_work->target_x == -10 &&
           e.zombie_work->target_z == 0 && e.previous_angles[1] == 777,
       "Walk startup RNG/fallthrough/scalars failed");
    s = fixture();
    s.enemies[0].type = 5;
    s.enemies[0].zombie_walk_choice.reset();
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].motion_number == 125 && s.enemies[1].motion_number == 325 &&
           s.enemies[0].blend_count == 15 && s.enemies[0].turn_speed == 512,
       "Type5 walk selection failed");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = 20;
    s.enemies[0].control3 = 1;
    s.enemies[0].zombie_work->player_distance = 29.99f;
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].modes[1] == 1 && s.enemies[0].modes[2] == 2 &&
           s.enemies[0].modes[3] == 0 && s.enemies[0].control0 == 20 &&
           s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == 16847671 && s.random_seed == 1,
       "Strict attack transition/sound failed");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = 20;
    s.enemies[0].control3 = 1;
    s.enemies[0].zombie_work->player_distance = 30;
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].modes[3] == 1 && s.enemies[0].control0 == 19 &&
           s.pending_enemy_sounds.empty(),
       "Attack equality must stay walking");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].flags |= 0x200000;
    s.enemies[0].control3 = 1;
    s.enemies[0].zombie_work->player_distance = 15;
    s.enemies[0].zombie_work->personality = 255;
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].modes[3] == 3 && s.enemies[0].control0 == 138 &&
           s.random_seed == 1103527590,
       "Primary200000 must bypass personality and allow close turn");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = 20;
    s.enemies[0].control3 = 1;
    s.collision->walls = {front()};
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].modes[3] == 2 && s.enemies[0].turn_speed == 256 &&
           s.enemies[0].previous_angles[1] == 16384 &&
           s.enemies[0].control0 == 20,
       "Front wall tie should use player bearing and retain ayp");
    player.position.x = 10;
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control3 = 1;
    s.collision->walls = {front()};
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].turn_speed == -256 &&
           s.enemies[0].previous_angles[1] == 49152,
       "Front wall opposite tie sign failed");
    auto left = front();
    left.position = {5, 0, -1};
    left.size = {2, 10, 2};
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control3 = 1;
    s.collision->walls = {front(), left};
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].turn_speed == 256 && s.enemies[0].previous_angles[1] == 777,
       "Side hit must choose side without ayp overwrite");
    auto right = left;
    right.position.x = -7;
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control3 = 1;
    s.dynamic_walls = {front(), right};
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].turn_speed == -256, "Dynamic right wall sign failed");
    for (auto choice : {0, 2, 4})
      for (int leg = 0; leg < 2; ++leg) {
        const int frames[6] = {13, 42, 10, 45, 10, 43};
        s = fixture();
        s.enemies[0].modes[3] = 1;
        s.enemies[0].zombie_walk_choice = choice;
        s.enemies[0].control0 = 0;
        s.enemies[0].control3 = 1;
        s.enemies[0].frame = frames[choice + leg] * 65536;
        s.enemies[0].flags |= 0x80000;
        tick_zombie_walk(s, 0, player);
        ck(s.enemies[0].modes == std::array<uint8_t, 4>{1, 0, 0, 0} &&
               bool(s.enemies[0].flags & 0x80000) == !leg &&
               s.enemies[0].control0 == -1,
           "Authored stop window/80000 leg failed");
      }
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = 0;
    s.enemies[0].control3 = 1;
    s.enemies[0].frame = 9 * 65536;
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].modes[3] == 1 && s.enemies[0].control0 == -1,
       "Outside stop windows must continue");
    s = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].turn_speed = -256;
    s.enemies[0].control3 = 1;
    s.collision->walls = {front()};
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].angles[1] == -256 && s.enemies[0].modes[3] == 2,
       "Blocked turn must retain signed yaw");
    s.collision->walls.clear();
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].modes[3] == 1, "Clear wall should end avoidance");
    s = fixture();
    s.enemies[0].modes[3] = 3;
    s.enemies[0].control0 = 0;
    s.enemies[0].control3 = 1;
    s.enemies[0].turn_speed = 256;
    s.collision.reset();
    player.position.x = -10;
    tick_zombie_walk(s, 0, player);
    ck(s.enemies[0].angles[1] == 256 &&
           s.enemies[0].zombie_work->target_x == -10 &&
           s.enemies[0].modes[3] == 1 && s.enemies[0].control0 == 238 &&
           s.random_seed == 1103527590,
       "Close turning refresh/RNG failed");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].zombie_work->player_distance = 1;
    s.enemies[0].control3 = 0;
    tick_zombie_walk(s, 0, player);
    ck(s.pending_enemy_sounds.size() == 2 && s.enemies[0].control3 == 210 &&
           s.random_seed == 1103527590,
       "Attack plus cadence must retain two requests");
    s = fixture();
    s.enemies[0].modes[3] = 99;
    s.enemies[0].control3 = 0;
    s.enemies[0].flags |= 0x10000;
    s.enemies[0].zombie_work->personality = 255;
    s.collision.reset();
    tick_zombie_walk(s, 0, player);
    ck(s.pending_enemy_sounds.empty() && s.enemies[0].control3 == 210,
       "Silent unknown mode still advances cadence RNG");
    s = fixture();
    s.enemies[0].zombie_work->body = 999;
    bad([&] { tick_zombie_walk(s, 0, player); });
    ck(s.random_seed == 1 && s.enemies[0].motion_number == 9 &&
           s.enemies[0].modes[3] == 0,
       "Invalid body partially committed startup");
    s = fixture();
    s.collision.reset();
    bad([&] { tick_zombie_walk(s, 0, player); });
    ck(s.random_seed == 1 && s.enemies[0].control1 == 999 &&
           s.enemies[1].motion_number == 201,
       "Missing wall context must roll back fallthrough");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].control0 = 0;
    s.enemies[0].control3 = 1;
    s.enemies[0].zombie_walk_choice = 3;
    bad([&] { tick_zombie_walk(s, 0, player); });
    ck(s.enemies[0].control0 == 0 && s.enemies[0].control3 == 1,
       "Invalid stop table choice partially mutated countdown");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].zombie_work->player_distance = 1;
    s.enemies[0].control3 = 0;
    s.enemies[0].position.x = std::numeric_limits<float>::infinity();
    bad([&] { tick_zombie_walk(s, 0, player); });
    ck(s.pending_enemy_sounds.empty() && s.random_seed == 1 &&
           s.enemies[0].modes[2] == 1,
       "Invalid sound must roll back attack and RNG");
    std::cout << "Original MV01 startup, pursuit/attack/walls/stop gates, "
                 "sounds and rollback passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
