#include "locomotion.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool b) {
  if (!b)
    throw std::runtime_error("Locomotion check failed");
}
template <class F> void bad(F f) {
  bool fail = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    fail = true;
  }
  ck(fail);
}
int main() {
  try {
    PlayerPackage package;
    package.motions.resize(100);
    for (auto pair : {std::pair<size_t, uint32_t>{0, 36},
                      {4, 20},
                      {16, 36},
                      {39, 12},
                      {42, 10},
                      {9, 20},
                      {10, 20}}) {
      Motion m;
      m.frames = pair.second;
      package.motions[pair.first] = m;
    }
    PlayerTranslationTables tables(11, std::vector<float>(37, .25f));
    tables[3].assign(21, 1);
    tables[6].assign(37, -.2f);
    PlayerLocomotion state;
    state.position = {0, 0, 0};
    state.visual_anchor = state.position;
    auto next = advance_player_locomotion(state, package, tables, 1);
    ck(next.motion == 0 && next.sampled_frame == 5 * 65536 &&
       next.frame == 6 * 65536 && std::fabs(next.position.z + .25f) < .00001f);
    state = next;
    next = advance_player_locomotion(state, package, tables, 9);
    ck(next.sampled_frame == 6 * 65536 && next.yaw == uint16_t(-728));
    state = next;
    next = advance_player_locomotion(state, package, tables, 0x401);
    ck(next.mode == 6 && next.entering);
    state = next;
    next = advance_player_locomotion(state, package, tables, 0x401);
    ck(next.motion == 4 && next.sampled_frame == 20 && next.frame == 65556);
    state = next;
    next = advance_player_locomotion(state, package, tables, 1);
    ck(next.motion == 4 && next.mode == 3 && next.entering);
    state = next;
    next = advance_player_locomotion(state, package, tables, 1);
    ck(next.motion == 0 && next.sampled_frame == 36);
    state = {};
    next = advance_player_locomotion(state, package, tables, 2);
    ck(next.motion == 16 && next.sampled_frame == 25 * 65536 &&
       std::fabs(next.position.z - .2f) < .00001f);
    state = {};
    next = advance_player_locomotion(state, package, tables, 8);
    ck(next.motion == 39 && next.yaw == uint16_t(-1310) &&
       next.frame == 11 * 65536);
    state = {};
    next = advance_player_locomotion(state, package, tables, 3);
    ck(next.motion == 42 && next.position.z == 0);
    ck(remap_player_frame(36, 20, 6 * 65536) ==
       20); // Retail legacy clamp verified from ELF.
    bad([&] { remap_player_frame(0, 20, 0); });
    bad([&] { remap_player_frame(1, 20, 2147483647); });
    state = {};
    for (int i = 0; i < 301; ++i)
      state = advance_player_locomotion(state, package, tables, 0);
    ck(state.mode == 13 && state.locked && state.entering);
    state = advance_player_locomotion(state, package, tables, 0);
    ck(state.motion == 9 && state.sampled_frame == 0);
    for (int i = 0; i < 19; ++i)
      state = advance_player_locomotion(state, package, tables, 0);
    ck(state.motion == 10 && state.phase == 2);
    state = advance_player_locomotion(state, package, tables, 1);
    ck(!state.locked && state.previous_pad == 0);
    state = advance_player_locomotion(state, package, tables, 1);
    ck(state.motion == 0 && state.sampled_frame == 5 * 65536);
    RoomCollision room;
    room.height = 30;
    RoomCut first;
    first.flags = 1;
    first.areas.push_back({0, 0, 0, 0, -10, -10, 10, 0});
    RoomCut second = first;
    second.areas[0].min_z = -30;
    second.areas[0].max_z = -10;
    PlayerWorldState world;
    world.player.position = {0, 0, -1};
    world.player.visual_anchor = world.player.position;
    world.front_anchor = {0, 12.5f, -1};
    world.cut = 0;
    for (int i = 0; i < 60; ++i)
      world = advance_player_world(world, package, tables, room,
                                   {first, second}, 1);
    ck(world.cut == 1 && std::fabs(world.player.position.z + 16) < .001f &&
       world.player.position.y == 0);
    ck(std::fabs(world.front_anchor.z + 18) < .001f);
    state = {};
    state.position = {0, 0, 0};
    state.visual_anchor = {0, 0, -2};
    state = advance_player_locomotion(state, package, tables, 0);
    ck(std::fabs(state.position.z + .6f) < .00001f &&
       state.sampled_blend_count == 5);
    world = {};
    world.player.position = {0, 0, -1};
    world.player.visual_anchor = world.player.position;
    world.front_anchor = {0, 12.5f, -1};
    CollisionRecord water;
    water.flags = 1;
    water.type = 3;
    water.parameters[0] = 1;
    water.position = {-10, -1, -30};
    water.size = {20, 2, 40};
    room.floors = {water};
    world =
        advance_player_world(world, package, tables, room, {first, second}, 1);
    ck(world.player.water &&
       std::fabs(world.player.position.z + 1.25f) < .00001f);
    world =
        advance_player_world(world, package, tables, room, {first, second}, 1);
    ck(std::fabs(world.player.position.z + 1.45f) < .00001f);
    room.floors.clear();
    world =
        advance_player_world(world, package, tables, room, {first, second}, 1);
    ck(!world.player.water &&
       std::fabs(world.player.position.z + 1.65f) < .00001f);
    world =
        advance_player_world(world, package, tables, room, {first, second}, 1);
    ck(std::fabs(world.player.position.z + 1.9f) < .00001f);
    CollisionRecord wall;
    wall.flags = 1;
    wall.position = {-10, 0, -10};
    wall.size = {20, 30, 1};
    room.walls = {wall};
    world = {};
    world.player.position = {0, 0, -1};
    world.player.visual_anchor = world.player.position;
    world.front_anchor = {0, 12.5f, -1};
    for (int i = 0; i < 60; ++i)
      world = advance_player_world(world, package, tables, room,
                                   {first, second}, 1);
    ck(std::fabs(world.player.position.z + 3.5f) < .001f &&
       std::fabs(world.front_anchor.z + 5.5f) < .001f);
    package.motions[0].reset();
    bad([&] { advance_player_locomotion({}, package, tables, 1); });
    std::cout << "Locomotion checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
