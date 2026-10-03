#include "zombie.hpp"
#include <cmath>
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
ScenarioState startup_fixture() {
  ScenarioState state;
  state.enemies.resize(128);
  state.collision = RoomCollision{};
  ActorModel model;
  model.model.resize(16 + 18 * 52);
  auto put = [&](size_t p, uint32_t v) {
    for (size_t i = 0; i < 4; ++i)
      model.model[p + i] = uint8_t(v >> (8 * i));
  };
  model.model[0] = 'M';
  model.model[1] = 'D';
  model.model[2] = 'L';
  model.model[3] = 128;
  model.model[4] = 16;
  model.model[6] = 18;
  put(8, 0xffffffff);
  for (size_t joint = 0; joint < 18; ++joint) {
    const size_t p = 16 + joint * 52;
    put(p, 6);
    put(p + 4, 0xffffffff);
    if (joint)
      put(p + 12, 0x3f800000); // Unit Y bind links.
    put(p + 44, joint == 17 ? 0xffffffff : uint32_t((joint + 1) * 52));
    put(p + 48, 0xffffffff);
  }
  auto bank = std::make_shared<ActorModelBank>();
  bank->models = {model, model};
  auto motions = std::make_shared<MotionBank>(512);
  for (uint32_t number : {2u, 202u}) {
    Motion motion;
    motion.frames = 3;
    motion.flags = 0x10;
    motion.tracks.resize(number == 2 ? 18 : 10);
    for (auto &track : motion.tracks)
      track.angles.assign(3, std::array<uint16_t, 3>{});
    motion.tracks[0].positions.assign(3, number == 2 ? Vec3{2, 0, 0}
                                                     : Vec3{0, 4, 0});
    (*motions)[number] = motion;
  }
  for (size_t slot : {0u, 2u}) {
    auto &root = state.enemies[slot];
    root.flags = 1;
    root.id = 1;
    root.model_version = slot ? 32 : 9;
    root.model_bank = bank;
    root.model = std::shared_ptr<const ActorModel>(bank, &bank->models[0]);
    root.motions = motions;
    root.position = {100, 200, 300};
  }
  state.enemies[1].flags = 1;
  state.enemies[1].id = 91;
  return state;
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
    auto startup = startup_fixture();
    startup.player_work_flags = 0xff;
    startup.enemies[0].damage.fill(99);
    initialize_zombie_work(startup, 0);
    auto &root = startup.enemies[0];
    ck(root.zombie_work && root.zombie_work->body == 3 && root.flags == 0x979);
    ck(root.health == 72 && root.zombie_work->personality == 19 &&
       root.zombie_flags == 0x10000001 &&
       root.zombie_work->material_color == 0xffb2b2b2);
    ck(root.frame == 65536 && root.motion_number == 2 && root.modes[0] == 1 &&
       startup.random_seed == 2524885223u && startup.player_work_flags == 0xfb);
    ck(root.damage[0] == 0 && root.damage[63] == 0 && root.combo_flags == 2 &&
       root.look_joint == 10);
    ck(root.geometry.radius == 3.8f && root.geometry.collision_radius == 2.5f &&
       root.geometry.collision_height == 10);
    ck(root.clip_joints == std::array<int8_t, 8>{11, 14, 17, 4, 7, 12, 15, 1});
    ck(root.zombie_work->children[4] == 4 &&
       root.zombie_work->children[0] == 5 &&
       root.zombie_work->children[1] == 6 &&
       root.zombie_work->children[2] == 7 &&
       root.zombie_work->children[3] == 8);
    ck(startup.enemies[9].id == 38 && startup.enemies[9].flags == 1 &&
       !startup.enemies[9].linked_parent && startup.enemies[9].pose_owner == 0);
    ck(root.zombie_work->shadow->joints ==
           std::vector<int8_t>({0, 3, 4, 10, -1}) &&
       root.zombie_work->shadow->anchor_joint == 1);
    initialize_zombie_work(startup, 2);
    ck(startup.enemies[2].zombie_work->body == 10 &&
       startup.enemies[2].zombie_walk_choice ==
           (*startup.enemies[0].zombie_walk_choice + 2) % 6);
    ck(startup.enemies[15].id == 33 && !(startup.enemies[16].flags & 1));
    initialize_zombie_work(startup, 3);
    ck(startup.enemies[3].flags == 0x89d1 &&
       startup.enemies[3].health == startup.enemies[0].health &&
       startup.enemies[3].zombie_flags == 0x80000001 &&
       startup.enemies[3].motion_number == 202);
    ck(startup.enemies[3].frame == 65536 &&
       startup.enemies[3].pose_owner == 0 &&
       startup.enemies[3].joint_pose.empty() &&
       startup.enemies[0].joint_pose[8].position->y == 4);
    ck(startup.enemies[3].zombie_work->shadow->joints ==
           std::vector<int8_t>({8, 10, -1}) &&
       startup.enemies[3].zombie_work->shadow->anchor_joint == 10);
    ck(startup.enemies[0].geometry.capsule_start.y == 213 &&
       startup.enemies[0].geometry.capsule_end.y == 205.5f &&
       startup.enemies[0].geometry.capsule_radius == 2);
    ck(startup.enemies[3].geometry.height == 13 &&
       startup.enemies[3].geometry.collision_height == 13);
    auto frozen_runtime = startup;
    auto &frozen_joint = frozen_runtime.enemies[0].model_work->joints[8];
    frozen_joint.flags = 1;
    frozen_joint.matrix[3] = 400;
    frozen_joint.matrix[7] = 500;
    frozen_joint.matrix[11] = 600;
    initialize_zombie_work(frozen_runtime, 3);
    ck(frozen_runtime.enemies[0].model_work->joints[8].matrix[7] == 500 &&
       frozen_runtime.enemies[0].model_work->joints[10].matrix[7] == 502 &&
       frozen_runtime.enemies[0].geometry.capsule_start.y == 502 &&
       frozen_runtime.enemies[3].geometry.height == 302);
    ck(startup.enemies[0].model_work->joints[8].matrix[7] == 211 &&
       startup.enemies[0].model_work->joints[8].flags == 0);
    const auto startup_seed = startup.random_seed;
    ck(startup.enemy_callbacks[31]==EnemyCallbackKind::ZombieHead &&
       startup.enemy_callbacks[32]==EnemyCallbackKind::ZombieArm &&
       startup.enemy_callbacks[33]==EnemyCallbackKind::ZombieLeg &&
       startup.enemy_callbacks[38]==EnemyCallbackKind::ZombieManager);
    for (size_t slot : {4u, 5u, 6u, 7u, 8u, 9u}) {
      startup.enemies[slot].state_flags = 0xffffffff;
      initialize_zombie_auxiliary(startup, slot);
      const auto &part = startup.enemies[slot];
      ck(part.modes == std::array<uint8_t, 4>{1, 0, 0, 0} &&
         part.state_flags == 0 && (part.flags & 0x8000) &&
         part.pose_owner == 0 && (part.model_flags & 1));
      if (part.id == 38)
        ck(!part.zombie_part_work && !part.linked_parent &&
           !(part.flags & 0x80));
      else {
        ck(part.linked_parent == 0 && (part.flags & 0x80) &&
           part.zombie_part_work->words.size() == (part.id == 31 ? 8u : 28u));
        ck(part.zombie_part_work->words[0] == 0 &&
           part.zombie_part_work->words[1] == 0 &&
           part.zombie_part_work->words[2] == (part.id == 31 ? 0u : 2u) &&
           part.zombie_part_work->words[5] == 0);
        startup.enemies[slot].zombie_part_work->words[3] = 77;
        startup.enemies[slot].zombie_part_work->words[0] = 99;
        initialize_zombie_auxiliary(startup, slot);
        ck(startup.enemies[slot].zombie_part_work->words[3] == 77 &&
           startup.enemies[slot].zombie_part_work->words[0] == 0);
      }
    }
    ck(startup.random_seed == startup_seed);
    bad([&] { initialize_zombie_auxiliary(startup, 0); });
    bad([&] { initialize_zombie_auxiliary(startup, 128); });
    startup.enemies[4].zombie_part_work->words.resize(7);
    startup.enemies[4].modes = {0, 3, 2, 1};
    startup.enemies[4].state_flags = 33;
    bad([&] { initialize_zombie_auxiliary(startup, 4); });
    ck(startup.enemies[4].state_flags == 33 &&
       startup.enemies[4].modes == std::array<uint8_t, 4>{0, 3, 2, 1});
    auto isolated = startup;
    auto edited = std::make_shared<MotionBank>(*isolated.enemies[3].motions);
    (*edited)[202]->tracks[0].positions[1] = {0, 8, 0};
    isolated.enemies[3].motions = edited;
    advance_zombie_motion(isolated, 3);
    ck(isolated.enemies[0].joint_pose[8].position->y == 8 &&
       startup.enemies[0].joint_pose[8].position->y == 4 &&
       isolated.enemies[3].frame == 131072 &&
       startup.enemies[3].frame == 65536);
    const auto children = startup.enemies[0].zombie_work->children;
    startup.enemies[0].zombie_work->children[7] = 12;
    startup.enemies[0].zombie_work->awareness = 0x5e;
    startup.enemies[0].zombie_work->awareness_timeout = 15;
    startup.enemies[0].zombie_flags = 0x7fffffff;
    initialize_zombie_work(startup, 0);
    ck(startup.enemies[0].zombie_work->body == 3 &&
       startup.enemies[0].zombie_work->children == children &&
       !(startup.enemies[16].flags & 1));
    ck(startup.enemies[0].zombie_flags == 0x10000001);
    ck(startup.enemies[0].zombie_work->awareness == 0 &&
       startup.enemies[0].zombie_work->awareness_timeout == 0);
    auto exhausted = startup_fixture();
    for (auto &enemy : exhausted.enemies)
      enemy.flags |= 1;
    bad([&] { initialize_zombie_work(exhausted, 0); });
    ck(exhausted.random_seed == 1 && !exhausted.enemies[0].zombie_work &&
       exhausted.enemies[0].modes[0] == 0);
    auto invalid = startup_fixture();
    invalid.enemies[0].model_version = 5;
    bad([&] { initialize_zombie_work(invalid, 0); });
    auto alternate = startup_fixture();
    alternate.enemies[0].model_version = 10;
    initialize_zombie_work(alternate, 0);
    ck(alternate.enemies[0].health == 63 &&
       alternate.enemies[0].zombie_work->personality == 20 &&
       alternate.enemies[0].motion_number == 2);
    auto alternate_body = *alternate.enemies[0].zombie_work->body;
    initialize_zombie_work(alternate, alternate_body);
    ck(alternate.enemies[alternate_body].health == 63 &&
       alternate.enemies[alternate_body].pose_owner == 0);
    ck(invalid.random_seed == 1 && !invalid.enemies[0].zombie_work);
    invalid = startup_fixture();
    invalid.enemies[0].motions = std::make_shared<MotionBank>(512);
    initialize_zombie_work(invalid, 0);
    // bhSetMotion permits a null record and retains the frame counter.
    ck(invalid.enemies[0].frame == 0 && invalid.enemies[0].zombie_work);
    invalid = startup_fixture();
    invalid.enemies[0].motions.reset();
    bad([&] { initialize_zombie_work(invalid, 0); });
    ck(invalid.random_seed == 1 && !invalid.enemies[0].zombie_work);
    std::cout << "Zombie personality health and walking passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
