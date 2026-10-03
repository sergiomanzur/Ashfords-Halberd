#include "zombie.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace ashford;
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F fn) {
  bool rejected = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Invalid close-pursuit input accepted");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  s.collision.emplace();
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t number : {0u, 9u, 40u, 41u, 117u, 118u, 119u, 125u, 200u, 240u,
                          241u, 317u, 325u}) {
    Motion m;
    m.frames = number == 117 || number == 317   ? 60
               : number == 125 || number == 325 ? 70
                                                : 80;
    bank->at(number) = m;
  }
  auto &e = s.enemies[0];
  e.flags = 0x2040001;
  e.id = 1;
  e.motion_number = 0;
  e.modes = {1, 1, 2, 0};
  e.zombie_work.emplace();
  e.zombie_work->body = 1;
  e.zombie_work->personality = 1;
  e.zombie_work->player_distance = 60;
  e.zombie_work->target_x = e.zombie_work->target_z = -10;
  e.zombie_work->pursuit_phase_word = 999;
  e.zombie_work->pursuit_transition_timer = 888;
  e.zombie_walk_choice = 2;
  e.zombie_flags = 0x300040f;
  e.motions = bank;
  e.motion_number = 9;
  e.frame = 123;
  e.control0 = 999;
  e.control1 = 998;
  e.control2 = 997;
  e.control3 = 100;
  auto &b = s.enemies[1];
  b.flags = 0x2040081;
  b.id = 1;
  b.zombie_work.emplace();
  b.zombie_flags = 0x83000000;
  b.motion_number = 201;
  b.motions = bank;
  return s;
}
int main(int argc, char **argv) {
  try {
    ScenarioEntity player;
    player.position = {-10, 0, -10};
    auto s = fixture();
    if (argc > 1) {
      if (std::string(argv[1]) == "--change") {
        change_zombie_walk_motion(s, 0);
        ck(s.enemies[0].motion_number == 40 &&
               s.enemies[1].motion_number == 240,
           "Source walking transition missing");
      } else {
        s.enemies[2].flags = 1;
        s.enemies[2].id = 1;
        s.enemies[2].zombie_work.emplace();
        s.enemies[2].zombie_secondary_flags = 0x80;
        ck(zombie_fast_walk_active(s, 0), "Source fast-root scan missing");
      }
      std::cout << "Source pursuit dependency passed\n";
      return 0;
    }
    tick_zombie_close_pursuit(s, 0, player);
    const auto &e = s.enemies[0];
    ck(e.motion_number == 40 && e.frame == 0 && e.blend_count == 8 &&
           e.turn_speed == 256 && e.control0 == 0 && e.control1 == 1 &&
           e.control2 == 150 && e.control3 == 14 && e.angles[1] == 256 &&
           e.modes[3] == 1 && e.zombie_flags == 0x402 &&
           e.zombie_work->pursuit_phase_word == 0 &&
           e.zombie_work->pursuit_transition_timer == 0 &&
           s.enemies[1].motion_number == 240 && s.enemies[1].blend_count == 8 &&
           s.enemies[1].zombie_flags == 0x80000000 &&
           s.random_seed == 662824084u,
       "Source MV02 startup/fallthrough/RNG/linked motion failed");
    s = fixture();
    s.enemies[0].zombie_work->player_distance = 10;
    tick_zombie_close_pursuit(s, 0, player);
    ck(s.enemies[0].motion_number == 117 && s.enemies[0].blend_count == 20 &&
           s.enemies[1].motion_number == 118 && s.enemies[1].frame == 0 &&
           s.enemies[1].blend_count == 10 && s.enemies[0].turn_speed == 512 &&
           s.enemies[0].control0 == 31 && s.enemies[0].control3 == 183 &&
           s.enemies[0].zombie_work->attack_delay == 5 &&
           s.enemies[0].zombie_work->pursuit_phase_word == 1 &&
           s.enemies[0].zombie_work->pursuit_transition_timer == 17 &&
           s.random_seed == 3295386429u && s.pending_enemy_sounds.size() == 1 &&
           s.pending_enemy_sounds[0].code == 16847671,
       "Nearby pursuit acceleration and source tail order failed");
    s = fixture();
    s.enemies[0].motion_number = 0;
    s.enemies[1].motion_number = 200;
    s.enemies[0].frame = 7 * 65536;
    change_zombie_walk_motion(s, 0);
    ck(s.enemies[0].motion_number == 117 && s.enemies[0].frame == 60 * 65536 &&
           s.enemies[1].motion_number == 118 && s.enemies[1].frame == 0,
       "Legacy FrameNum clamp and second shift must remain literal");
    for (auto pair : {std::array<uint32_t, 4>{117, 118, 117, 317},
                      {117, 317, 40, 119},
                      {125, 118, 125, 325},
                      {125, 325, 40, 119},
                      {40, 240, 117, 118},
                      {9, 201, 40, 240}}) {
      s = fixture();
      s.enemies[0].motion_number = pair[0];
      s.enemies[1].motion_number = pair[1];
      s.enemies[0].frame = 0;
      change_zombie_walk_motion(s, 0);
      ck(s.enemies[0].motion_number == pair[2] &&
             s.enemies[1].motion_number == pair[3],
         "Source paired walking transition selection failed");
    }
    s = fixture();
    s.enemies[0].motion_number = 40;
    s.enemies[1].motion_number = 240;
    s.enemies[0].zombie_secondary_flags = 0x80;
    s.enemies[0].frame = 0;
    change_zombie_walk_motion(s, 0);
    ck(s.enemies[0].motion_number == 125,
       "Fast flag must select alternate transition");
    s = fixture();
    s.enemies[0].modes[3] = 99;
    s.enemies[0].motion_number = 117;
    s.enemies[1].motion_number = 118;
    s.enemies[0].frame = 0;
    s.enemies[0].zombie_work->pursuit_transition_timer = 1;
    tick_zombie_close_pursuit(s, 0, player);
    ck(s.enemies[1].motion_number == 317 &&
           s.enemies[0].zombie_work->pursuit_transition_timer == 0 &&
           s.random_seed == 1,
       "Transition tail must execute even for an unmatched submode");
    s = fixture();
    auto &other = s.enemies[2];
    other.flags = 1;
    other.id = 1;
    other.zombie_work.emplace();
    other.zombie_secondary_flags = 0x80;
    ck(zombie_fast_walk_active(s, 0), "Active other fast root not found");
    for (uint32_t flags : {0u, 3u, 0x81u}) {
      other.flags = flags;
      ck(!zombie_fast_walk_active(s, 0), "Excluded fast work blocked scan");
    }
    other.flags = 1;
    other.type = 5;
    ck(!zombie_fast_walk_active(s, 0),
       "Type5 must not block another fast root");
    other.type = 0;
    other.zombie_work.reset();
    ck(!zombie_fast_walk_active(s, 0),
       "Uninitialized work blocked fast-root scan");
    s = fixture();
    s.enemies[0].modes[3] = 1;
    s.enemies[0].turn_speed = 256;
    s.enemies[0].control1 = 150;
    s.enemies[0].control2 = 150;
    s.enemies[0].zombie_work->pursuit_phase_word = 2;
    s.enemies[0].zombie_work->pursuit_transition_timer = 0;
    s.random_seed = 2;
    tick_zombie_close_pursuit(s, 0, player);
    ck(s.enemies[0].angles[1] == -256 && s.enemies[0].control1 == 151 &&
           s.random_seed == 2207042835u,
       "Source corrective steering/RNG window failed");
    for (bool blocked : {false, true}) {
      s = fixture();
      auto &root = s.enemies[0];
      root.modes[3] = 1;
      root.motion_number = 117;
      root.frame = 0;
      s.enemies[1].motion_number = 317;
      root.zombie_work->pursuit_phase_word = 1;
      root.zombie_work->pursuit_transition_timer = 0;
      root.zombie_work->player_distance = 10;
      root.zombie_secondary_flags = 0x80;
      root.flags = 0x2000001;
      root.zombie_flags = 0x400;
      root.turn_speed = 512;
      s.random_seed = 6;
      if (blocked) {
        auto &second = s.enemies[2];
        second.flags = 1;
        second.id = 1;
        second.zombie_work.emplace();
        second.zombie_secondary_flags = 0x80;
      }
      tick_zombie_close_pursuit(s, 0, player);
      ck(root.motion_number == (blocked ? 117u : 125u) &&
             s.random_seed == 2326136519u &&
             (blocked ||
              (s.enemies[1].motion_number == 325 && root.blend_count == 3)),
         "Fast pursuit chance/other-root exclusion/source RNG failed");
    }
    s = fixture();
    s.enemies[0].modes[3] = 2;
    s.enemies[0].zombie_work->pursuit_transition_timer = 0;
    tick_zombie_close_pursuit(s, 0, player);
    ck(s.enemies[0].modes[3] == 1,
       "Pursuit wall recovery must return to tracking");
    s = fixture();
    s.enemies[0].zombie_work->target_x =
        std::numeric_limits<float>::quiet_NaN();
    const auto flags = s.enemies[0].flags;
    bad([&] { tick_zombie_close_pursuit(s, 0, player); });
    ck(s.random_seed == 1 && s.enemies[0].flags == flags &&
           s.enemies[0].motion_number == 9 &&
           s.enemies[1].motion_number == 201 && s.pending_enemy_sounds.empty(),
       "Late pursuit failure partially committed paired work/RNG");
    s = fixture();
    s.enemies[0].zombie_work->body.reset();
    bad([&] { change_zombie_walk_motion(s, 0); });
    ck(s.enemies[0].motion_number == 9, "Missing transition body mutated root");
    s = fixture();
    s.enemies[0].type = 5;
    s.enemies[0].floor = 1;
    s.enemies[0].zombie_work->path_blocked = true;
    tick_zombie_close_pursuit(s, 0, player);
    ck(s.enemies[0].motion_number == 125 && s.enemies[1].motion_number == 118 &&
           s.enemies[0].zombie_secondary_flags == 0x80 &&
           s.enemies[0].zombie_work->pursuit_phase_word == 1 &&
           s.random_seed == 3295386429u,
       "Type5 must use original startup pair then unconditional close "
       "acceleration");
    for (bool wider : {false, true})
      for (bool outside : {false, true}) {
        s = fixture();
        auto &r = s.enemies[0];
        r.modes[3] = 1;
        r.motion_number = 40;
        r.frame = 0;
        s.enemies[1].motion_number = 240;
        r.flags = 1;
        r.zombie_flags = 0x400;
        r.zombie_secondary_flags = 0x80 | (wider ? 2 : 0);
        r.turn_speed = 0;
        r.zombie_work->pursuit_phase_word = 0;
        r.zombie_work->pursuit_transition_timer = 0;
        r.zombie_work->player_distance =
            (wider ? 25.f : 20.f) + (outside ? .01f : 0);
        tick_zombie_close_pursuit(s, 0, player);
        ck(r.zombie_work->pursuit_phase_word == (outside ? 0 : 1),
           "Source inclusive20/25 acceleration boundary failed");
      }
    for (bool wider : {false, true})
      for (bool outside : {false, true}) {
        s = fixture();
        auto &r = s.enemies[0];
        r.modes[3] = 1;
        r.motion_number = 117;
        r.frame = 0;
        s.enemies[1].motion_number = 317;
        r.flags = 1;
        r.zombie_flags = 0x400;
        r.zombie_secondary_flags = 0x80 | (wider ? 2 : 0);
        r.turn_speed = 0;
        r.control0 = 0;
        r.zombie_work->pursuit_phase_word = 1;
        r.zombie_work->pursuit_transition_timer = 0;
        r.zombie_work->player_distance =
            (wider ? 30.f : 25.f) + (outside ? .01f : 0);
        s.flags[5][0] = 0x80000000;
        tick_zombie_close_pursuit(s, 0, player);
        ck(r.zombie_work->pursuit_phase_word == (outside ? 0 : 1) &&
               s.random_seed == 1 &&
               (!outside ||
                (r.motion_number == 40 && s.enemies[1].motion_number == 119 &&
                 r.turn_speed == 256 &&
                 r.zombie_work->pursuit_transition_timer == 17)),
           "Source strict25/30 disengage boundary or room RNG gate failed");
      }
    for (bool opposite_edge : {false, true}) {
      s = fixture();
      auto &r = s.enemies[0];
      r.modes[3] = 1;
      r.motion_number = 40;
      r.frame = 0;
      s.enemies[1].motion_number = 240;
      r.flags = 1;
      r.zombie_flags = 0x400;
      r.zombie_secondary_flags = 0x80;
      r.turn_speed = 0;
      r.angles[1] = opposite_edge ? 16384 : -16384;
      r.zombie_work->target_x = 0;
      r.zombie_work->target_z = -10;
      r.zombie_work->pursuit_phase_word = 0;
      r.zombie_work->pursuit_transition_timer = 0;
      r.zombie_work->player_distance = 10;
      auto forward = player;
      forward.position = {0, 0, -10};
      tick_zombie_close_pursuit(s, 0, forward);
      ck(r.zombie_work->pursuit_phase_word == (opposite_edge ? 1 : 0),
         "Ikou3 source cone boundary asymmetry changed");
    }
    s = fixture();
    s.enemies[0].modes[3] = 99;
    s.enemies[0].flags |= 0x10000;
    s.enemies[0].control3 = 0;
    s.enemies[0].zombie_work->pursuit_transition_timer = 0;
    s.enemies[0].zombie_work->personality = 255;
    s.enemies[0].motions.reset();
    s.enemies[0].position.x = std::numeric_limits<float>::quiet_NaN();
    tick_zombie_close_pursuit(s, 0, player);
    ck(s.pending_enemy_sounds.empty() && s.random_seed == 1103527590u &&
           s.enemies[0].control3 == 210,
       "Suppressed timed sound must ignore unused inputs, retain timer RNG");
    s = fixture();
    s.enemies[0].zombie_work->player_distance = 10;
    auto broken = std::make_shared<MotionBank>(*s.enemies[0].motions);
    broken->at(117)->frames = 0;
    s.enemies[0].motions = broken;
    bad([&] { tick_zombie_close_pursuit(s, 0, player); });
    ck(s.random_seed == 1 && s.pending_enemy_sounds.empty() &&
           s.enemies[0].motion_number == 9 && s.enemies[1].motion_number == 201,
       "Failed walking phase must roll back staged sound/RNG/pairs");
    std::cout
        << "Original close pursuit, linked walk transitions and RNG passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
