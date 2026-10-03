#include "enemy_navigation.hpp"
#include "runtime.hpp"
#include "script_motion_fixture.hpp"
#include <iostream>
using namespace ashford;
using script_fixture::bad;
using script_fixture::ck;
ScenarioState fixture() {
  ScenarioState s;
  s.collision.emplace();
  CollisionRecord r;
  r.flags = 0xa5;
  r.floor = 0;
  r.position = {-10, 0, -10};
  r.size = {20, 10, 20};
  r.attribute = 0x1234;
  s.collision->walls.assign(3, r);
  s.collision->miscellaneous.assign(3, r);
  s.collision->floors.assign(3, r);
  s.interactions = s.collision->miscellaneous;
  s.items.resize(1);
  s.items[0].flags = 1;
  s.items[0].id = 42;
  return s;
}
const CollisionRecord &record(const ScenarioState &s, uint8_t opcode) {
  return opcode == 10   ? s.collision->walls[2]
         : opcode == 11 ? s.collision->miscellaneous[2]
                        : s.collision->floors[2];
}
void commands() {
  for (uint8_t opcode : {11, 10, 12}) {
    for (uint8_t disable : {0, 1, 2, 255}) {
      auto s = fixture();
      run_scenario(Bytes{opcode, 2, disable, 219, 55, 7, 0}, s);
      const auto &r = record(s, opcode);
      ck(r.flags == (disable ? 0xa4 : 0xa5) && r.position.x == -10 &&
             r.size.z == 20 && r.attribute == 0x1234 && s.room_case == 7,
         "Original activation must change only flag1 and consume four bytes");
      if (opcode == 11)
        ck(s.interactions[2].flags == r.flags,
           "Miscellaneous activation must update actual interaction view");
    }
    for (const auto &code :
         {Bytes{opcode}, Bytes{opcode, 2, 1}, Bytes{opcode, 3, 1, 0, 0}}) {
      auto s = fixture();
      bad([&] { run_scenario(code, s); });
      ck(record(s, opcode).flags == 0xa5 && s.interactions[2].flags == 0xa5,
         "Invalid extent/index must reject before activation mutation");
    }
    auto missing = fixture();
    missing.collision.reset();
    bad([&] { run_scenario(Bytes{opcode, 2, 1, 0, 0}, missing); });
    ck(missing.interactions[2].flags == 0xa5,
       "Missing collision binding must retain interaction state");
  }
  auto mismatch = fixture();
  mismatch.interactions.resize(2);
  bad([&] { run_scenario(Bytes{11, 2, 1, 0, 0}, mismatch); });
  ck(mismatch.collision->miscellaneous[2].flags == 0xa5,
     "Malformed paired miscellaneous binding must not publish a flag");
}
void live_stair_reservations() {
  for (uint8_t opcode : {11, 14, 35}) {
    auto s = fixture();
    s.enemies.resize(128);
    s.collision->miscellaneous[2].type = 1;
    s.interactions = s.collision->miscellaneous;
    ck(enemy_stair_trigger(*s.collision, {}, {}, 0, 0, 0) == 2,
       "Unreserved original stair must be available to actual enemy query");
    reserve_actor_stairs(s, 0, 2);
    ck(!enemy_stair_trigger(*s.collision, {}, {}, 0, 0, 0),
       "Actual actor reservation must exclude occupied stair");
    s.interaction_index = 2;
    s.flags[10][0] = 0x800;
    s.flags[7][0] = opcode == 35 ? 0x80000000 : 0;
    const Bytes code = opcode == 11   ? Bytes{11, 2, 0, 0, 0}
                       : opcode == 14 ? Bytes{14, 0, 0, 0, 2, 0, 0}
                                      : Bytes{35, 0, 0, 0, 2, 0, 0};
    run_scenario(code, s);
    ck(s.collision->miscellaneous[2].attribute == (0x1234 | 0x400000),
       "Visibility/item command must preserve live stair reservation");
    run_scenario(Bytes{11, 2, 0, 0, 0}, s);
    ck(!enemy_stair_trigger(*s.collision, {}, {}, 0, 0, 0),
       "Enabling an occupied stair must not expose it to another actor");
    release_actor_stairs(s, 0);
    run_scenario(Bytes{11, 2, 0, 0, 0}, s);
    ck(s.collision->miscellaneous[2].attribute == 0x1234 &&
           s.interactions[2].attribute == 0x1234 &&
           enemy_stair_trigger(*s.collision, {}, {}, 0, 0, 0) == 2,
       "Visibility command must not resurrect a released stair reservation");
  }
}
void consumers() {
  live_stair_reservations();
  auto s = fixture();
  auto &wall = s.collision->walls[0];
  wall.flags = 1;
  wall.position = {};
  wall.size = {10, 10, 10};
  s.collision->walls.resize(1);
  CollisionBody body;
  body.position = {-.5f, 0, 5};
  body.radius = 1;
  body.height = 2;
  ck(collide_player_walls(*s.collision, body).hits == 1,
     "Original fixture wall must participate before disable");
  run_scenario(Bytes{10, 0, 1, 0, 0}, s);
  ck(collide_player_walls(*s.collision, body).hits == 0,
     "Disabled wall must disappear from real collision query");
  run_scenario(Bytes{10, 0, 0, 0, 0}, s);
  ck(collide_player_walls(*s.collision, body).hits == 1,
     "Enabled wall must return to real collision query");
  for (auto &r : s.collision->miscellaneous)
    r.flags = 0;
  auto &step = s.collision->miscellaneous[2];
  step.flags = 1;
  step.type = 2;
  step.parameters[0] = 1;
  s.interactions = s.collision->miscellaneous;
  ck(query_step_area(*s.collision, {}, 0, 0, 0) == 2,
     "Original step fixture must participate");
  run_scenario(Bytes{11, 2, 1, 0, 0}, s);
  ck(!query_step_area(*s.collision, {}, 0, 0, 0),
     "Disabled miscellaneous area must disappear from step query");
  for (auto &r : s.collision->floors)
    r.flags = 0;
  auto &water = s.collision->floors[2];
  water.flags = 1;
  water.type = 3;
  water.parameters[0] = 1;
  ck(query_water(*s.collision, {}, {}) == 2,
     "Original water floor fixture must participate");
  run_scenario(Bytes{12, 2, 1, 0, 0}, s);
  ck(!query_water(*s.collision, {}, {}),
     "Disabled floor must disappear from real water query");
  RoomRuntime runtime;
  runtime.scenario = fixture();
  for (auto &r : runtime.scenario.interactions)
    r.flags = 0;
  auto &pickup = runtime.scenario.interactions[2];
  pickup.flags = 1;
  pickup.attribute = 0;
  pickup.type = 4;
  runtime.scenario.collision->miscellaneous = runtime.scenario.interactions;
  ck(examine_room_interaction(runtime) &&
         runtime.scenario.selected_item_id == 42,
     "Actual index2 pickup must dispatch before disable");
  run_scenario(Bytes{11, 2, 1, 0, 0}, runtime.scenario);
  ck(!examine_room_interaction(runtime),
     "Original-shaped11,2,1,0 must disable actual examination dispatch");
}
void mutations() {
  auto s = fixture();
  s.flags[7][0] = 0x80000000;
  run_scenario(Bytes{35, 0, 0, 0, 2, 0, 0}, s);
  ck(s.interactions[2].flags == 0xa4 &&
         s.collision->miscellaneous[2].flags == 0xa4 &&
         s.items[0].state_flags == 0x1000000,
     "Original item defeat updates same miscellaneous table");
  s = fixture();
  s.interaction_index = 2;
  s.flags[10][0] = 0x800;
  run_scenario(Bytes{14, 0, 0, 0, 2, 0, 0}, s);
  ck(s.interactions[2].flags == 0xa4 &&
         s.collision->miscellaneous[2].flags == 0xa4 &&
         s.flags[7][0] == 0x80000000 && s.flags[10][0] == 0,
     "Original pickup progress updates same miscellaneous table");
  s = fixture();
  run_scenario(Bytes{37, 2, 0xcd, 0xab, 1, 2, 3, 4, 2, 219, 0}, s);
  const auto &r = s.collision->miscellaneous[2];
  ck(r.attribute == 0xabcd && r.parameters[0] == 1 && r.parameters[3] == 4 &&
         r.type == 2 && r.flags == 0xa5 && r.position.x == -10 &&
         query_step_area(*s.collision, {}, 0, 0, 0) == 2,
     "Existing miscellaneous replacement must reach real query consumers");
  for (uint8_t opcode : {14, 35, 37}) {
    auto invalid = fixture();
    invalid.collision->miscellaneous.resize(2);
    invalid.interaction_index = 2;
    invalid.flags[10][0] = 0x800;
    invalid.flags[7][0] = opcode == 35 ? 0x80000000 : 0;
    const auto old_flags = invalid.flags;
    Bytes code = opcode == 14   ? Bytes{14, 0, 0, 0, 2, 0, 0}
                 : opcode == 35 ? Bytes{35, 0, 0, 0, 2, 0, 0}
                                : Bytes{37, 2, 0, 0, 1, 2, 3, 4, 2, 0, 0};
    bad([&] { run_scenario(code, invalid); });
    ck(invalid.flags == old_flags && invalid.items[0].state_flags == 0 &&
           invalid.interactions[2].flags == 0xa5 &&
           invalid.interactions[2].type == 0,
       "Malformed shared binding must reject before progress/item/table "
       "changes");
  }
}
void runtime_consumer() {
  RoomRuntime runtime;
  PlayerPackage player;
  const auto model = script_fixture::fixture().enemies[2].model;
  player.models.push_back(*model);
  player.motions.resize(100);
  for (auto &slot : player.motions) {
    Motion motion;
    motion.frames = 36;
    motion.flags = 0x10;
    motion.tracks.resize(18);
    for (auto &track : motion.tracks)
      track.angles.assign(36, {});
    motion.tracks[0].positions.assign(36, {});
    slot = std::move(motion);
  }
  runtime.collision.walls.resize(1);
  auto &wall = runtime.collision.walls[0];
  wall.flags = 1;
  wall.attribute = 1;
  wall.position = {};
  wall.size = {10, 20, 10};
  runtime.scenario.collision = runtime.collision;
  runtime.scenario.collision->walls[0].flags = 0;
  script_fixture::event(runtime.scenario, Bytes{0});
  runtime.scenario.flags[6][0] = 1;
  runtime.scenario.player_work_flags = 0x118;
  runtime.scenario.player_health = 160;
  runtime.world.player.position = {-.5f, 0, 5};
  runtime.world.player.visual_anchor = runtime.world.player.position;
  runtime.world.front_anchor = runtime.world.player.position;
  runtime.player_pose =
      sample_player_locomotion_pose(player, runtime.world.player);
  synchronize_runtime_player(runtime, player);
  tick_room_runtime(runtime, player,
                    PlayerTranslationTables(11, std::vector<float>(37, .25f)),
                    0);
  std::cout << "runtime hits=" << runtime.world.wall_hits
            << " position=" << runtime.world.player.position.x << ','
            << runtime.world.player.position.z << '\n';
  ck(runtime.world.wall_hits == 0 && runtime.world.player.position.x == -.5f,
     "Runtime locomotion must use current scenario walls, not decoded "
     "snapshot");
}
int main(int argc, char **argv) {
  try {
    const std::string mode = argc == 2 ? argv[1] : "commands";
    if (mode == "consumers")
      consumers();
    else if (mode == "mutations")
      mutations();
    else if (mode == "runtime")
      runtime_consumer();
    else
      commands();
    std::cout << "Scenario collision " << mode << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
