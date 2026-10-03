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
    PlayerPackage posed_package;
    ActorModel posed_model;
    posed_model.model.resize(16 + 22 * 52);
    auto write = [&](size_t p, uint32_t value) {
      for(size_t i=0;i<4;++i)posed_model.model[p+i]=uint8_t(value>>(8*i));
    };
    posed_model.model[0]='M';posed_model.model[1]='D';posed_model.model[2]='L';
    posed_model.model[3]=128;posed_model.model[4]=16;posed_model.model[6]=22;
    write(24,0x41200000); // Root bind x=10.
    write(16+52+12,0x40000000); // Child bind y=2.
    posed_package.models.push_back(posed_model);
    Motion posed_motion;
    posed_motion.frames=2;posed_motion.flags=0x80;posed_motion.tracks.resize(22);
    for(auto& track:posed_motion.tracks)track.angles.assign(2,std::array<uint16_t,3>{});
    posed_motion.tracks[0].positions={Vec3{26,0,0},Vec3{34,0,0}};
    posed_motion.tracks[1].positions={Vec3{0,10,0},Vec3{0,14,0}};
    posed_package.motions.push_back(posed_motion);
    PlayerLocomotion posed_state;
    posed_state.motion=0;posed_state.sampled_frame=0;
    posed_state.sampled_blend_count=8;posed_state.blend_rate=42598;
    auto player_pose=sample_player_locomotion_pose(posed_package,posed_state);
    ck(std::fabs(player_pose[0].position->x-15.6f)<.001f &&
       player_pose[1].position->y==2);
    posed_state.sampled_blend_count=0;
    player_pose=sample_player_locomotion_pose(posed_package,posed_state,player_pose);
    ck(player_pose[0].position->x==26 && player_pose[1].position->y==2);
    posed_package.motions[0]->flags=0x90;
    player_pose=sample_player_locomotion_pose(posed_package,posed_state);
    ck(player_pose[1].position->y==10);
    posed_package.motions[0]->flags=0xa0;
    posed_state.sampled_frame=32768;
    player_pose=sample_player_locomotion_pose(posed_package,posed_state);
    ck(player_pose[0].position->x==30 && player_pose[1].position->y==12);
    posed_state.sampled_frame=-1;
    bad([&]{sample_player_locomotion_pose(posed_package,posed_state);});
    posed_state.sampled_frame=0;posed_state.sampled_blend_count=8;
    posed_package.motions[0]->flags=0x80;
    for(size_t joint:{6u,7u,11u}) {
      player_pose[joint].angles=std::array<uint16_t,3>{12000,64000,3000};
      posed_package.motions[0]->tracks[joint].angles.assign(
          2,std::array<uint16_t,3>{50000,9000,62000});
    }
    player_pose=sample_player_locomotion_pose(posed_package,posed_state,player_pose);
    for(size_t joint:{7u,11u}) {
      std::array<int,3> expected{3856,2201,1158};
      for(size_t axis=0;axis<3;++axis)
        ck(std::abs(int((*player_pose[joint].angles)[axis])-expected[axis])<=4);
    }
    ck(player_pose[6].angles!=player_pose[7].angles);
    posed_state.sampled_blend_count = 0;
    posed_package.motions[0]->flags = 0x90;
    posed_package.motions[0]->tracks[1].angles.clear();
    player_pose[1].angles = std::array<uint16_t, 3>{10, 20, 30};
    player_pose = sample_player_locomotion_pose(posed_package, posed_state, player_pose);
    ck(*player_pose[1].angles == std::array<uint16_t, 3>{0, 20, 30});
    posed_package.motions[0]->flags = 0xa0;
    posed_package.motions[0]->tracks.resize(2);
    posed_package.motions[0]->tracks[0].angles.assign(2, std::array<uint16_t, 3>{100, 200, 300});
    posed_state.sampled_blend_count = 1;
    posed_state.blend_rate = 0;
    std::array<uint16_t, 3> shared_scratch{7, 8, 9};
    player_pose = sample_player_locomotion_pose(posed_package, posed_state, player_pose, &shared_scratch);
    ck(shared_scratch == std::array<uint16_t, 3>{0, 200, 300} &&
       *player_pose[1].angles == shared_scratch);
    posed_state.sampled_frame = -1;
    bad([&] { sample_player_locomotion_pose(posed_package, posed_state, player_pose, &shared_scratch); });
    ck(shared_scratch == std::array<uint16_t, 3>{0, 200, 300});
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
