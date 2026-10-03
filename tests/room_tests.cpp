#include "room.hpp"
#include "runtime.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void check(bool value, int line) {
  if (!value)
    throw std::runtime_error("Room entry check failed at line " +
                             std::to_string(line));
}
#define ck(value) check((value), __LINE__)
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected);
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void flt(Bytes &b, size_t p, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, 4);
  put(b, p, bits);
}
int main() {
  try {
    ck(room_archive_index(0, 1, 0) == 1);
    ck(room_archive_index(0, 2, 0) == 2);
    ck(room_archive_index(0, 2, 1) == 3);
    ck(room_archive_index(1, 0, 2) == 21);
    ck(room_archive_index(9, 37, 0) == 204);
    bad([&] { room_archive_index(0, 1, 1); });
    bad([&] { room_archive_index(10, 0, 0); });
    bad([&] { room_archive_index(0, 100, 0); });
    bad([&] { room_archive_index(0, 0, 10); });
    Bytes b(1500);
    flt(b, 0, 10);
    put(b, 16, 128);
    put(b, 128 + 0x24, 1200);
    put(b, 128 + 0xa4, 2);
    flt(b, 1200, 15);
    flt(b, 1204, 21);
    flt(b, 1208, -7);
    put(b, 1212, 0xffff8000);
    flt(b, 1216, -10);
    flt(b, 1220, 0);
    flt(b, 1224, 2);
    put(b, 1228, 65536 + 123);
    put(b, 128 + 8, 1240);
    put(b, 128 + 0x88, 1);
    put(b, 1240, 0x80000001);
    put(b, 1244, 0x12345678);
    b[1248] = 255;
    b[1249] = 3;
    b[1250] = 7;
    b[1251] = 254;
    flt(b, 1252, 1);
    flt(b, 1256, 2);
    flt(b, 1260, 3);
    put(b, 1264, 0x8000ffff);
    put(b, 1268, 0xfffe4000);
    b[1272] = 255;
    b[1273] = 128;
    b[1274] = 127;
    b[1275] = 0;
    auto room = decode_room_records(b);
    ck(room.positions.size() == 2 && room.positions[0].yaw == 0xffff8000);
    const auto &e = room.enemies.at(0);
    ck(e.flags == 0x80000001 && e.id == 0x5678 && e.type == 0x1234 &&
       e.floor == -1);
    ck(e.model_version == 3 && e.work_number == 7 && e.parameter == -2);
    ck(e.rotation_x == -1 && e.rotation_z == -32768 && e.rotation_y == 16384 &&
       e.angular_speed == -2);
    ck(e.hide[0] == -1 && e.hide[1] == -128 && e.hide[2] == 127);
    RoomCollision collision;
    collision.ground[3] = 20;
    std::vector<RoomCut> cuts;
    auto state = enter_room(room, 0, collision, cuts);
    ck(state.player.position.x == 15 && state.player.position.y == 20 &&
       state.floor == 1);
    ck(state.player.yaw == 32768 && state.front_anchor.y == 32.5f &&
       state.front_offset == 0);
    ck(state.player.visual_anchor.y == 20 && state.player.entering &&
       !state.cut);
    ck(enter_room(room, 1, collision, cuts).player.yaw == 123);
    auto runtime_data = b;
    runtime_data.resize(2500);
    put(runtime_data, 128 + 0x28, 2200);
    put(runtime_data, 128 + 0x2c, 2250);
    put(runtime_data, 128 + 0xa8, 1);
    flt(runtime_data, 2208, -50);
    flt(runtime_data, 2220, 100);
    flt(runtime_data, 2224, 100);
    flt(runtime_data, 2228, 100);
    runtime_data[2250] = 255; // Same-region queries do not read the diagonal.
    put(runtime_data, 264, 0); // No authored enemies in this fixture.
    put(runtime_data, 20, 1800);
    put(runtime_data, 1800, 1850);
    runtime_data[1850] = 'M';
    runtime_data[1851] = 'D';
    runtime_data[1852] = 'L';
    runtime_data[1853] = 128;
    runtime_data[1854] = 16;
    runtime_data[1856] = 1;
    put(runtime_data, 1858, 0xffffffff);
    put(runtime_data, 1866, 6);
    put(runtime_data, 1870, 0xffffffff);
    put(runtime_data, 1910, 0xffffffff);
    put(runtime_data, 1914, 0xffffffff);
    put(runtime_data, 32, 1950);
    put(runtime_data, 1950, 1);
    put(runtime_data, 1954, 2000);
    put(runtime_data, 2000, 0xffffffff);
    put(runtime_data, 28, 2100);
    put(runtime_data, 2100, 8);
    put(runtime_data, 2104, 17);
    Bytes frame_check{56, 0, 0, 1, 1, 0, 55, 7, 0};
    std::copy(frame_check.begin(), frame_check.end(),
              runtime_data.begin() + 2108);
    PlayerPackage entry_player;
    ActorModel entry_model;
    entry_model.model =
        Bytes(runtime_data.begin() + 1850, runtime_data.begin() + 1918);
    entry_player.models.push_back(entry_model);
    entry_player.motions.resize(43);
    Motion entry_idle;
    entry_idle.flags = 0x10;
    entry_idle.frames = 2;
    entry_idle.tracks.resize(1);
    entry_idle.tracks[0].positions.resize(2);
    entry_idle.tracks[0].angles.resize(2);
    entry_player.motions[42] = entry_idle;
    ScenarioState persistent;
    persistent.flags[1][0] = 123;
    persistent.inventory[0] = 99;
    persistent.flags[4][0] = persistent.flags[5][0] = 0xffffffff;
    persistent.flags[10][0] = 0xff7fffff;
    persistent.flags[11][0] = 0xffffffff;
    persistent.flags[6][0] = 0;
    persistent.tasks[3].active = true;
    persistent.tasks[3].counts[0] = 17;
    persistent.objects.resize(4);
    persistent.objects[0].id = 77;
    persistent.door = ScenarioDoor{};
    persistent.player_frame = 99;
    auto old_routes = std::make_shared<RoomRoutes>();
    old_routes->regions.resize(2);
    persistent.routes = old_routes;
    persistent.dynamic_miscellaneous.resize(2);
    persistent.dynamic_walls.resize(2);
    persistent.pending_enemy_sounds.push_back({0,{1,2,3},99});
    persistent.pending_enemy_vibrations.push_back({0,1,10});
    auto runtime =
        load_room_runtime(runtime_data, 0, 1, 1, persistent, entry_player);
    ck(runtime.scenario.routes && runtime.scenario.routes != old_routes &&
       runtime.scenario.routes->regions.size() == 1 &&
       room_route_id(*runtime.scenario.routes, {0, 0, 0}) == 0 &&
       persistent.routes == old_routes);
    ck(runtime.scenario.dynamic_miscellaneous.empty() &&
       persistent.dynamic_miscellaneous.size() == 2);
    ck(runtime.scenario.dynamic_walls.empty() && persistent.dynamic_walls.size() == 2);
    ck(runtime.scenario.pending_enemy_sounds.empty() &&
       persistent.pending_enemy_sounds.size() == 1);
    ck(runtime.scenario.pending_enemy_vibrations.empty() &&
       persistent.pending_enemy_vibrations.size() == 1);
    ck(runtime.world.player.position.x == -10 &&
       runtime.world.player.yaw == 123 && runtime.world.player.entering &&
       runtime.scenario.stage == 0 && runtime.scenario.room == 1 &&
       runtime.scenario.inventory[0] == 99 &&
       runtime.scenario.flags[1][0] == 123 &&
       runtime.scenario.flags[4][0] == 0 && runtime.scenario.flags[5][0] == 0 &&
       runtime.scenario.flags[10][0] == 0xaf0000bb &&
       runtime.scenario.flags[11][0] == 0x9b8c08c2 &&
       runtime.scenario.flags[6][0] == 0xffffffef &&
       (runtime.scenario.flags[12][0] & 0x180) == 0x180 &&
       !runtime.scenario.door && runtime.scenario.objects[0].id == 77 &&
       !runtime.scenario.tasks[3].active &&
       runtime.scenario.tasks[3].counts[0] == 17 &&
       runtime.scenario.player_frame == 65536 &&
       runtime.world.player.frame == 65536 &&
       runtime.world.player.sampled_frame == 0 &&
       runtime.scenario.room_case == 7 && runtime.player_pose.size() == 1);
    bad([&] {
      load_room_runtime(runtime_data, 0, 1, 2, persistent, entry_player);
    });
    ck(persistent.door && persistent.tasks[3].active &&
       persistent.flags[4][0] == 0xffffffff);
    auto transition = runtime;
    transition.scenario.door = ScenarioDoor{0x80, 2, 3, 0, 1};
    transition.scenario.flags[10][0] |= 1;
    transition.previous_pad = 0x200;
    transition.scenario.random_seed = 42;
    bad([&] { transition_room_runtime(transition, Bytes{}, entry_player); });
    const auto current_routes = transition.scenario.routes;
    auto bad_routes = runtime_data;
    put(bad_routes, 128 + 0x2c, 2500);
    bad([&] { transition_room_runtime(transition, bad_routes, entry_player); });
    ck(transition.scenario.routes == current_routes);
    ck(transition.scenario.room == 1 && transition.scenario.door &&
       transition.world.player.position.x == -10 &&
       transition.scenario.inventory[0] == 99 && transition.previous_pad == 0x200);
    transition_room_runtime(transition, runtime_data, entry_player);
    ck(transition.scenario.routes && transition.scenario.routes != current_routes &&
       transition.scenario.routes->regions.size() == 1);
    ck(transition.scenario.stage == 2 && transition.scenario.room == 3 &&
       !transition.scenario.door && !(transition.scenario.flags[10][0] & 1) &&
       transition.scenario.inventory[0] == 99 &&
       transition.scenario.flags[1][0] == 123 &&
       transition.scenario.random_seed == 42 && transition.previous_pad == 0 &&
       transition.scenario.flags[6][0] == 0xffffffff &&
       transition.scenario.drawing_flags == 0xffffffff &&
       transition.world.player.position.x == 15);
    bad([&] { transition_room_runtime(transition, runtime_data, entry_player); });
    auto& scheduled = transition.scenario.tasks[1];
    scheduled.program = transition.scenario.scenario->program(0);
    scheduled.active = true;
    scheduled.pc = 0;
    const auto before_timer = transition.scenario.event_timer;
    tick_scenario_tasks(transition.scenario);
    ck(scheduled.pc == 8 && transition.scenario.event_timer == before_timer + 1);
    auto saved_entry = persistent;
    saved_entry.flags[10][0] |= 0x800000;
    bad([&] {
      load_room_runtime(runtime_data, 0, 1, 0, saved_entry, entry_player);
    });
    Bytes control_frame{56, 0, 0, 1, 1, 0, 55, 9, 0};
    std::copy(control_frame.begin(), control_frame.end(),
              runtime_data.begin() + 2117);
    auto running =
        load_room_runtime(runtime_data, 0, 1, 0, persistent, entry_player);
    PlayerTranslationTables entry_tables(11, std::vector<float>(3, 0));
    tick_room_runtime(running, entry_player, entry_tables, 0);
    ck(running.scenario.room_case == 9 && running.world.player.frame == 65536 &&
       running.world.player.sampled_frame == 0 &&
       !running.world.player.entering &&
       running.scenario.player_frame == running.world.player.frame);
    auto touching = running;
    touching.scenario.flags[6][0] &= ~2u; // Isolate the original player contact pass.
    auto &obstacle = touching.scenario.enemies[0];
    obstacle.flags = 0x49;
    obstacle.state_flags = 0;
    obstacle.secondary_flags = 1;
    obstacle.geometry.collision_radius = 2.5f;
    obstacle.geometry.collision_height = 10;
    obstacle.position = touching.world.player.position;
    obstacle.position.x -= 2;
    obstacle.position.z += 1;
    obstacle.collision_offset.x = 2;
    const auto before_contact = touching.world.player.position;
    tick_room_runtime(touching, entry_player, entry_tables, 0);
    ck(std::abs(touching.world.player.position.z - (before_contact.z - 4)) < .002f &&
       obstacle.state_flags == 4 && obstacle.secondary_flags == 1);
    auto immovable = running;
    immovable.scenario.flags[6][0] &= ~2u;
    immovable.scenario.enemies[0] = obstacle;
    immovable.scenario.enemies[0].state_flags = 0;
    immovable.scenario.player_secondary_flags = 1;
    tick_room_runtime(immovable, entry_player, entry_tables, 0);
    ck(immovable.world.player.position.z == before_contact.z &&
       immovable.scenario.enemies[0].state_flags == 0);
    auto frozen = running.world.player.frame;
    running.scenario.flags[6][0] &= ~1u;
    tick_room_runtime(running, entry_player, entry_tables, 0);
    ck(running.world.player.frame == frozen);
    running.scenario.flags[6][0] |= 1;
    running.scenario.flags[10][0] = 0;
    CollisionRecord examine_trigger;
    examine_trigger.flags = 1;
    examine_trigger.type = 3;
    examine_trigger.position = {-100, 0, -100};
    examine_trigger.size = {200, 0, 200};
    examine_trigger.floor = running.world.floor;
    running.scenario.interactions = {examine_trigger};
    tick_room_runtime(running, entry_player, entry_tables, 0x200);
    ck(running.scenario.flags[10][0] & 0x100);
    running.scenario.flags[10][0] &= ~0x100u;
    tick_room_runtime(running, entry_player, entry_tables, 0x200);
    ck(!(running.scenario.flags[10][0] & 0x100));
    tick_room_runtime(running, entry_player, entry_tables, 0);
    tick_room_runtime(running, entry_player, entry_tables, 0x200);
    ck(running.scenario.flags[10][0] & 0x100);
    RoomRuntime drawing;
    Scene fragment;
    fragment.textures.resize(1);
    Mesh mesh;
    mesh.object = 512;
    mesh.triangles.resize(1);
    fragment.meshes.push_back(mesh);
    drawing.scene = fragment;
    drawing.scene.meshes[0].object = 0;
    drawing.player_scene = fragment;
    drawing.scenario.enemies.resize(1);
    drawing.scenario.enemies[0].flags = 1;
    drawing.scenario.enemies[0].rendered_model = fragment;
    drawing.scenario.items.resize(1);
    drawing.scenario.items[0].flags = 1;
    drawing.scenario.items[0].rendered_model = fragment;
    auto combined = compose_room_runtime(drawing, 0xffffffff, 0);
    ck(combined.meshes.size() == 4 && combined.textures.size() == 4 &&
       combined.meshes[0].triangles[0].texture == 0 &&
       combined.meshes[3].triangles[0].texture == 3);
    ck(compose_room_runtime(drawing, 0x20, 0).meshes.size() == 1);
    ck(compose_room_runtime(drawing, 1, 0).meshes.size() == 1);
    drawing.scenario.items[0].state_flags = 0x1000000;
    ck(compose_room_runtime(drawing, 0xffffffff, 0).meshes.size() == 3);
    drawing.scenario.items[0].state_flags = 0;
    drawing.scenario.items[0].flags |= 0x40000000;
    drawing.world.cut = 1;
    ck(compose_room_runtime(drawing, 0xffffffff, 0).meshes.size() == 3);
    drawing.scenario.items[0].hide[0] = 1;
    ck(compose_room_runtime(drawing, 0xffffffff, 0).meshes.size() == 4);
    drawing.player_scene.meshes[0].triangles[0].texture = 0xffff;
    auto sentinel = compose_room_runtime(drawing, 0xffffffff, 0);
    ck(sentinel.meshes.back().triangles[0].texture == 0xffff &&
       drawing.scene.meshes[0].triangles[0].texture == 0 &&
       drawing.scenario.items[0]
               .rendered_model->meshes[0]
               .triangles[0]
               .texture == 0);
    drawing.player_scene.meshes[0].triangles[0].texture = 1;
    bad([&] { compose_room_runtime(drawing, 0xffffffff, 0); });
    drawing.player_scene.meshes[0].triangles[0].texture = 0;
    drawing.scenario.objects.resize(1);
    auto &draw_object = drawing.scenario.objects[0];
    draw_object.flags = 1;
    draw_object.rendered_model = fragment;
    draw_object.id = 999;
    ck(compose_room_runtime(drawing, 4, 0).meshes.size() == 1);
    draw_object.id = 1000;
    ck(compose_room_runtime(drawing, 4, 0).meshes.empty());
    ck(compose_room_runtime(drawing, 6, 0).meshes.size() == 2);
    draw_object.id = 1210;
    ck(compose_room_runtime(drawing, 6, 0).meshes.size() == 1);
    ck(compose_room_runtime(drawing, 5, 0).meshes.size() == 2);
    drawing.scene.textures.resize(65535);
    ck(compose_room_runtime(drawing, 0x20, 0).textures.size() == 65535);
    bad([&] { compose_room_runtime(drawing, 0x21, 0); });
    RoomCut cut;
    cut.flags = 1;
    CutArea area;
    area.floor = 1;
    area.min_x = 10;
    area.max_x = 20;
    area.min_z = -10;
    area.max_z = 0;
    cut.areas.push_back(area);
    cuts.push_back(cut);
    ck(enter_room(room, 0, collision, cuts).cut == 0);
    ck(!enter_room(room, 1, collision, cuts).cut);
    CollisionRecord water;
    water.flags = 1;
    water.type = 3;
    water.parameters[0] = 1;
    water.position = {10, 20, -10};
    water.size = {10, 5, 10};
    collision.floors.push_back(water);
    ck(enter_room(room, 0, collision, cuts).player.water);
    ck(!enter_room(room, 1, collision, cuts).player.water);
    PlayerPackage package;
    package.motions.resize(43);
    Motion walk;
    walk.frames = 36;
    package.motions[0] = walk;
    package.motions[42] = walk;
    PlayerTranslationTables tables(11, std::vector<float>(37, .25f));
    auto wet_entry = enter_room(room, 0, collision, cuts);
    auto wet_step =
        advance_player_world(wet_entry, package, tables, collision, cuts, 1);
    ck(std::fabs(wet_step.player.position.z + 6.8f) < .00001f);
    bad([&] { enter_room(room, 2, collision, cuts); });
    auto corrupt = b;
    put(corrupt, 128 + 0x24, 1496);
    bad([&] { decode_room_records(corrupt); });
    corrupt = b;
    put(corrupt, 128 + 0x88, 100001);
    bad([&] { decode_room_records(corrupt); });
    corrupt = b;
    put(corrupt, 1200, 0x7f800000);
    bad([&] { decode_room_records(corrupt); });
    corrupt = b;
    put(corrupt, 1252, 0x7fc00000);
    bad([&] { decode_room_records(corrupt); });
    corrupt = b;
    put(corrupt, 128 + 8, 1480);
    bad([&] { decode_room_records(corrupt); });
    corrupt = b;
    put(corrupt, 128 + 0x24, 0);
    bad([&] { decode_room_records(corrupt); });
    collision.ground[3] = INFINITY;
    bad([&] { enter_room(room, 0, collision, cuts); });
    std::cout << "Room records and normal entry passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
