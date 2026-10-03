#include "enemy_navigation.hpp"
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
  ck(failed, "Malformed stair context did not reject");
}
CollisionRecord stair(float x = 0, float z = 0) {
  CollisionRecord r;
  r.flags = 1;
  r.type = 1;
  r.floor = 2;
  r.attribute = 0x100;
  r.position = {x, 10000, z};
  r.size = {10, -99999, 10};
  r.parameters = {0, 0, 4, 255};
  return r;
}
int main() {
  try {
    RoomCollision room;
    room.miscellaneous = {stair()};
    ck(enemy_stair_trigger(room, {}, {5, NAN, 10.5f}, 0, 2, 1) == 0,
       "Original look-behind and ignored Y failed");
    ck(!enemy_stair_trigger(room, {}, {5, 0, 10.5f}, 0, 2, -1),
       "Signed stair probe offset changed");
    ck(enemy_stair_trigger(room, {}, {.5f, 0, 5}, 0, 2, 0) == 0 &&
           enemy_stair_trigger(room, {}, {9.5f, 0, 5}, 0, 2, 0) == 0 &&
           !enemy_stair_trigger(room, {}, {.49f, 0, 5}, 0, 2, 0) &&
           !enemy_stair_trigger(room, {}, {9.51f, 0, 5}, 0, 2, 0),
       "Half-unit inclusive alignment inset changed");
    room.miscellaneous[0].parameters[1] = 2;
    ck(enemy_stair_trigger(room, {}, {5, 0, 10}, 0, 2, 0) == 0,
       "Direction2 must align on X, not Z");
    room.miscellaneous[0].parameters[1] = 1;
    ck(enemy_stair_trigger(room, {}, {0, 0, .5f}, 0, 2, 0) == 0 &&
           !enemy_stair_trigger(room, {}, {5, 0, .49f}, 0, 2, 0),
       "Other direction must align on Z");
    room.miscellaneous[0] = stair();
    ck(enemy_stair_trigger(room, {}, {1.5f, 0, 5}, 16384, 2, 1) == 0 &&
           !enemy_stair_trigger(room, {}, {.5f, 0, 5}, 16384, 2, 1),
       "Original quarter-wave yaw probe changed");
    auto dynamic = std::vector<CollisionRecord>{stair()};
    ck(enemy_stair_trigger(room, dynamic, {5, 0, 5}, 0, 2, 0) == 0,
       "Static stairs must win before dynamic");
    for (int filter = 0; filter < 4; ++filter) {
      room.miscellaneous[0] = stair();
      if (filter == 0)
        room.miscellaneous[0].flags = 0;
      if (filter == 1)
        room.miscellaneous[0].type = 2;
      if (filter == 2)
        room.miscellaneous[0].floor = 1;
      if (filter == 3)
        room.miscellaneous[0].attribute |= 0x400000;
      ck(enemy_stair_trigger(room, dynamic, {5, 0, 5}, 0, 2, 0) == 1,
         "Stair eligibility filtering/dynamic unified index failed");
    }
    room.miscellaneous = {stair(), stair(-1, -1)};
    room.miscellaneous[1].size = {2, 0, 2};
    ck(enemy_stair_trigger(room, {}, {.25f, 0, 5}, 0, 2, 0) == 1,
       "Source overwrites probe before failed alignment; later candidates must "
       "see it");
    room.miscellaneous = {stair()};
    bad([&] { enemy_stair_trigger(room, {}, {INFINITY, 0, 5}, 0, 2, 0); });
    bad([&] { enemy_stair_trigger(room, {}, {5, 0, 5}, 0, 2, NAN); });
    room.miscellaneous[0].size.x = INFINITY;
    bad([&] { enemy_stair_trigger(room, {}, {5, 0, 5}, 0, 2, 0); });

    ScenarioState state;
    state.enemies.resize(128);
    state.collision.emplace();
    state.collision->miscellaneous = {stair(), stair()};
    reserve_actor_stairs(state, 0, 0);
    ck(state.enemies[0].stair_index == 0 &&
           state.collision->miscellaneous[0].attribute == 0x400100 &&
           state.collision->miscellaneous[1].attribute == 0x400100,
       "Adjacent uphill stair reservation failed");
    release_actor_stairs(state, 0);
    ck(state.enemies[0].stair_index == 0 &&
           state.collision->miscellaneous[0].attribute == 0x100 &&
           state.collision->miscellaneous[1].attribute == 0x100,
       "Release must clear pair and retain source actor pointer/index");
    state.collision->miscellaneous[1].parameters[0] = 1;
    reserve_actor_stairs(state, 0, 1);
    ck(state.enemies[0].stair_index == 1 &&
           state.collision->miscellaneous[0].attribute == 0x400100,
       "Adjacent downhill pairing failed");
    release_actor_stairs(state, 0);
    state.dynamic_miscellaneous = {stair(), stair()};
    reserve_actor_stairs(state, 0, 2);
    ck(state.enemies[0].stair_index == 2 &&
           state.dynamic_miscellaneous[0].attribute == 0x400100 &&
           state.dynamic_miscellaneous[1].attribute == 0x400100 &&
           state.collision->miscellaneous[0].attribute == 0x100,
       "Dynamic adjacency must stay in dynamic array");
    release_actor_stairs(state, 0);
    state.dynamic_miscellaneous[1].parameters[3] = 0;
    reserve_actor_stairs(state, 0, 3);
    ck(state.dynamic_miscellaneous[1].attribute == 0x400100 &&
           state.collision->miscellaneous[0].attribute == 0x400100 &&
           state.dynamic_miscellaneous[0].attribute == 0x100,
       "Explicit dynamic pair must reference static room array");
    release_actor_stairs(state, 0);
    state.enemies[0].stair_index.reset();
    release_actor_stairs(state, 0);
    state.dynamic_miscellaneous[1].parameters[3] = 255;
    const auto flags = state.dynamic_miscellaneous[1].attribute;
    bad([&] { reserve_actor_stairs(state, 0, 3); });
    ck(!state.enemies[0].stair_index &&
           state.dynamic_miscellaneous[1].attribute == flags,
       "Invalid adjacent pair partially committed reservation");
    state.collision->miscellaneous[0].parameters[0] = 1;
    bad([&] { reserve_actor_stairs(state, 0, 0); });
    ck(state.collision->miscellaneous[0].attribute == 0x100,
       "Adjacent pair crossed from start of static array");
    state.dynamic_miscellaneous[1].parameters[3] = 2;
    bad([&] { reserve_actor_stairs(state, 0, 3); });
    state.enemies[0].stair_index = 3;
    bad([&] { release_actor_stairs(state, 0); });
    ck(state.dynamic_miscellaneous[1].attribute == flags,
       "Malformed release partially cleared attributes");
    bad([&] { reserve_actor_stairs(state, 128, 0); });
    std::cout << "Native stair query and reservation tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
