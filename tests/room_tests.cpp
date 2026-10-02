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
    auto runtime =
        load_room_runtime(runtime_data, 0, 1, 1, persistent, entry_player);
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
    auto saved_entry = persistent;
    saved_entry.flags[10][0] |= 0x800000;
    bad([&] {
      load_room_runtime(runtime_data, 0, 1, 0, saved_entry, entry_player);
    });
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
