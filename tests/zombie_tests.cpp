#include "zombie.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool value) {
  if (!value)
    throw std::runtime_error("Zombie parameter check failed");
}
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected);
}
int main() {
  try {
    ck(zombie_personality_index(9) == 19);
    ck(zombie_personality_index(32) == 6);
    ck(zombie_personality_index(42) == 25);
    // The last model-to-personality row is excluded by the original loop.
    ck(zombie_personality_index(43) == 0 && zombie_personality_index(255) == 0);
    const auto &frail = zombie_personality(9);
    ck(frail.attack_distance == 20 && frail.notice_distance == 50 &&
       frail.view_angle == 14563 && frail.health_percent == -20 &&
       frail.attack_bonus == 0);
    const auto &stronger = zombie_personality(32);
    ck(stronger.attack_distance == 50 && stronger.health_percent == 10 &&
       stronger.attack_bonus == 5);
    ScenarioState random;
    ck(scenario_random(random) == 1103527590 &&
       random.random_seed == 1103527590);
    ck(scenario_random(random) == 377401575 &&
       random.random_seed == 2524885223u);
    ck(scenario_random(random) == 662824084 && random.random_seed == 662824084);
    random.random_seed = 1;
    ck(roll_zombie_health(random, 9) == 72 && random.random_seed == 1103527590);
    random.random_seed = 1;
    ck(roll_zombie_health(random, 32) == 99);
    random.random_seed = 1;
    random.game_mode = 2;
    ck(roll_zombie_health(random, 9) == 32);
    random.random_seed = 1;
    random.game_mode = 1;
    ck(roll_zombie_health(random, 9) == 72);
    random.random_seed = 1;
    ck(roll_zombie_health(random, 43) == 90);

    ScenarioState state;
    state.enemies.resize(128);
    state.enemies[0].flags = 1;
    state.enemies[0].id = 1;
    state.enemies[1].flags = 0x81;
    state.enemies[1].id = 1;
    state.enemies[2].flags = 1;
    state.enemies[2].id = 91;
    state.enemies[3].flags = 1;
    state.enemies[3].id = 1;
    state.enemies[4].flags = 1;
    state.enemies[4].id = 1;
    ck(select_zombie_walk(state, 0) == 0 && state.random_seed == 1103527590);
    ck(select_zombie_walk(state, 3) == 2 && state.random_seed == 1103527590);
    ck(select_zombie_walk(state, 4) == 4 && state.random_seed == 1103527590);
    state.enemies[5].flags = 1;
    state.enemies[5].id = 1;
    ck(select_zombie_walk(state, 5) == 0 && state.random_seed == 1103527590);
    ck(zombie_walk_motion(0, false) == 0 && zombie_walk_motion(0, true) == 200);
    ck(zombie_walk_motion(2, false) == 40 &&
       zombie_walk_motion(2, true) == 240);
    ck(zombie_walk_motion(4, false) == 41 &&
       zombie_walk_motion(4, true) == 241);
    bad([&] { zombie_walk_motion(1, false); });
    bad([&] { zombie_walk_motion(6, true); });
    bad([&] { select_zombie_walk(state, 128); });
    bad([&] { select_zombie_walk(state, 1); });
    bad([&] { select_zombie_walk(state, 2); });
    state.enemies[3].zombie_walk_choice.reset();
    state.enemies[4].zombie_walk_choice.reset();
    bad([&] { select_zombie_walk(state, 4); });
    ck(!state.enemies[4].zombie_walk_choice && state.random_seed == 1103527590);
    state.enemies[3].flags = 0;
    ck(select_zombie_walk(state, 4) == 2);
    auto copied = state;
    ck(copied.enemies[4].zombie_walk_choice == 2);
    for (uint16_t type = 0; type < 16; ++type) {
      ScenarioState initialized;
      initialized.enemies.resize(128);
      auto &root = initialized.enemies[0];
      root.flags = 0x21;
      root.id = 1;
      root.type = type;
      root.health = 101;
      root.zombie_flags = 0xffffffff;
      root.modes = {7, 8, 9, 10};
      root.frame = 123;
      const bool half =
          type == 2 || type == 3 || type == 6 || type == 7 || type == 9;
      const bool walks = type != 2 && type != 3 && type != 9;
      initialize_zombie_type(initialized, 0);
      ck(root.modes ==
         std::array<uint8_t, 4>{
             1, 0, uint8_t(type == 6 || type == 7 || type == 15 ? 4 : 0), 0});
      ck(root.health == (half ? 50 : 101));
      ck(root.motion_number == uint32_t(type == 15 ? 13 : (half ? 3 : 2)));
      ck(root.motion_step == (type == 15 ? 0u : 65536u) && root.frame == 123);
      ck(root.zombie_flags ==
         (type == 10 ? 0xefffffffu
                     : (half || type == 15 ? 0xfffffff1u : 0xffffffffu)));
      ck(root.zombie_secondary_flags ==
         (type == 11 || type == 15 ? 0x10u : 0u));
      ck(root.flags == (type == 10 ? 1u : 0x21u));
      ck(bool(root.zombie_walk_choice) == walks);
      ck(initialized.random_seed == (walks ? 1103527590u : 1u));

      ScenarioState body_state;
      body_state.enemies.resize(128);
      body_state.enemies[0].flags = 1;
      body_state.enemies[0].id = 1;
      body_state.enemies[0].health = 101;
      auto &body = body_state.enemies[1];
      body.flags = 0xa1;
      body.id = 1;
      body.type = type;
      body.linked_parent = 0;
      body.health = 7;
      body.zombie_flags = 0xffffffff;
      initialize_zombie_type(body_state, 1);
      ck(body.health == (half ? 101 : 7));
      ck(body.motion_number == uint32_t(type == 15 ? 213 : (half ? 203 : 202)));
      ck(body.motion_step == (type == 15 ? 0u : 65536u));
      ck(body.zombie_flags ==
         (type == 10 ? 0xefffffffu : (half ? 0xfffffff0u : 0xffffffffu)));
      ck(body.zombie_secondary_flags ==
         (type == 11 || type == 15 ? 0x10u : 0u));
      ck(body_state.random_seed == 1 && !body.zombie_walk_choice);
      ck(body_state.enemies[0].health == 101);
    }
    bad([&] { initialize_zombie_type(state, 128); });
    state.enemies[0].type = 16;
    bad([&] { initialize_zombie_type(state, 0); });
    ck(state.enemies[0].modes[0] == 0);
    std::cout << "Zombie personality health and walking passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
