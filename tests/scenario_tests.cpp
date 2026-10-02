#include "scenario.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool value) {
  if (!value)
    throw std::runtime_error("Scenario check failed");
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
void put(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
int main() {
  try {
    Bytes b(1300);
    put(b, 0, 0x41200000);
    put(b, 16, 128);
    put(b, 28, 1200);
    put(b, 32, 1250);
    put(b, 1200, 16);
    put(b, 1204, 24);
    put(b, 1208, 30);
    put(b, 1212, 30);
    b[1216] = 5;
    b[1224] = 51;
    b[1230] = 55;
    auto scenario = decode_room_scenario(b);
    ck(scenario.entry_count() == 4 && scenario.program(0).size() == 34);
    ck(scenario.program(0)[0] == 5 && scenario.program(1)[0] == 51);
    ck(scenario.event(0)[0] == 55 && scenario.event(1).size() == 20);
    ck(scenario.event(0).size() == scenario.event(1).size());
    bad([&] { scenario.event(2); });
    auto c = b;
    put(c, 1200, 15);
    bad([&] { decode_room_scenario(c); });
    c = b;
    put(c, 1204, 12);
    bad([&] { decode_room_scenario(c); });
    c = b;
    put(c, 1204, 50);
    bad([&] { decode_room_scenario(c); });
    c = b;
    put(c, 1200, 0xfffffff0);
    bad([&] { decode_room_scenario(c); });
    c = b;
    put(c, 28, 1298);
    bad([&] { decode_room_scenario(c); });
    put(b, 28, 0);
    ck(decode_room_scenario(b).entry_count() == 0);
    ScenarioState state;
    Bytes code{5, 1, 31, 0, 0, 0, 1,  10, 4,    1, 31, 0, 0, 0, 55, 1,
               2, 6, 55, 2, 3, 0, 51, 0,  0x80, 0, 0,  2, 0, 1, 0,  0};
    run_scenario(code, state);
    ck(state.flags[1][0] == 1 && state.room_case == 1 && state.door);
    ck(state.door->stage == 0 && state.door->room == 2 &&
       state.door->position == 0 && state.door->type == 1 &&
       state.door->attribute == 0x80);
    code[5] = 1;
    run_scenario(code, state);
    ck(state.room_case == 2 && state.flags[1][0] == 0);
    bad([&] { run_scenario(Bytes{55}, state); });
    bad([&] { run_scenario(Bytes{1, 255, 0, 0}, state); });
    bad([&] { run_scenario(Bytes{255, 0}, state); });
    for (uint8_t operation = 0; operation < 6; ++operation) {
      state.flags[1][0] = 0x12345678;
      uint32_t mask = (operation < 3 ? 0x80000000u : 0xffffffffu) >> 4;
      uint32_t expected = operation % 3 == 0   ? (0x12345678u | mask)
                          : operation % 3 == 1 ? (0x12345678u & ~mask)
                                               : (0x12345678u ^ mask);
      run_scenario(Bytes{5, 1, 4, 0, 0, operation, 0, 0}, state);
      ck(state.flags[1][0] == expected);
    }
    bad([&] { run_scenario(Bytes{5, 8, 0, 1, 0, 0, 0, 0}, state); });
    bad([&] { run_scenario(Bytes{5, 1, 0, 0, 0, 6, 0, 0}, state); });
    bad([&] { run_scenario(Bytes{2, 0}, state); });
    state.flags[10][0] = 0x100;
    state.interaction_index = 3;
    run_scenario(Bytes{4, 10, 23, 0, 3, 0, 55, 9, 0, 0}, state);
    ck(state.room_case == 9);
    Bytes shared(1300);
    put(shared, 0, 0x41200000);
    put(shared, 16, 128);
    put(shared, 28, 1200);
    put(shared, 32, 1226);
    put(shared, 1200, 12);
    put(shared, 1204, 18);
    put(shared, 1208, 24);
    Bytes tail{5, 1, 0, 0, 0, 0, 5, 1, 1, 0, 0, 0, 0, 0};
    std::copy(tail.begin(), tail.end(), shared.begin() + 1212);
    auto overlapping = decode_room_scenario(shared);
    ScenarioState shared_state;
    run_scenario(overlapping.program(0), shared_state);
    ck(shared_state.flags[1][0] == 0xc0000000);
    run_scenario(Bytes{5, 1, 255, 3, 0, 0, 0, 0}, state);
    ck(state.flags[1][31] == 1);
    // Source G_Sp is replaced by the inner IF. A second false condition
    // decrements that cursor to byte 8, not the outer target at byte 22.
    ScenarioState nested_state;
    run_scenario(Bytes{1, 20, 1, 8, 4, 1,  0, 0, 0, 0,  55, 9, 4,
                       1, 0,  0, 0, 0, 55, 8, 0, 0, 55, 7,  0, 0},
                 nested_state);
    ck(nested_state.room_case == 0);
    ScenarioState entities;
    entities.interactions.resize(2);
    entities.items.resize(2);
    entities.objects.resize(5);
    entities.enemies.resize(2);
    entities.enemy_slots = {1};
    entities.interactions[0].flags = 1;
    entities.interactions[0].parameters[0] = 1;
    entities.flags[7][0] = 0x80000000;
    run_scenario(Bytes{35, 1, 0, 0, 0, 0, 0, 0}, entities);
    ck(!(entities.interactions[0].flags & 1) &&
       (entities.items[1].state_flags & 0x1000000));
    entities.flags[7][0] = 0;
    run_scenario(Bytes{35, 1, 0, 0, 0, 0, 0, 0}, entities);
    ck(entities.interactions[0].flags & 1);
    run_scenario(
        Bytes{36, 1, 0, 0, 36, 2, 4, 0, 36, 3, 0, 0, 36, 0, 0, 0, 0, 0},
        entities);
    ck((entities.enemies[1].flags & 0x8000) &&
       (entities.enemies[1].state_flags & 0x1000000));
    ck((entities.objects[4].state_flags & 0x1000000) &&
       (entities.items[0].state_flags & 0x1000000) &&
       (entities.flags[14][0] & 0x1000000));
    entities.enemies[1].flags |= 0x80000000;
    run_scenario(Bytes{36, 1, 0, 1, 0, 0}, entities);
    ck(!(entities.enemies[1].state_flags & 0x1000000) &&
       (entities.enemies[1].flags & 0x8000));
    entities.enemies[1].flags &= ~0x80000000u;
    run_scenario(Bytes{36, 1, 0, 1, 37, 1, 0x34, 0x12, 1, 2,
                       3,  4, 4, 0, 66, 1, 100,  0,    0, 0},
                 entities);
    ck(!(entities.enemies[1].flags & 0x8000) &&
       entities.items[1].angular_speed == 100);
    ck(entities.interactions[1].attribute == 0x1234 &&
       entities.interactions[1].type == 4 &&
       entities.interactions[1].parameters[3] == 4);
    entities.interaction_index = 0;
    entities.flags[10][0] = 0x800;
    entities.flags[9][0] = 0xffffffff;
    run_scenario(Bytes{14, 0, 31, 0, 0, 0, 0, 0}, entities);
    ck((entities.flags[7][0] & 1) && !(entities.flags[9][0] & 1) &&
       !(entities.flags[10][0] & 0x800));
    entities.flags[7][0] = 0;
    run_scenario(Bytes{14, 0, 30, 0, 0, 0, 0, 0}, entities);
    ck(entities.flags[9][0] & 2);
    bad([&] { run_scenario(Bytes{36, 1, 1, 0, 0, 0}, entities); });
    bad([&] {
      run_scenario(Bytes{37, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, entities);
    });
    entities.stage = 9;
    entities.room = 30;
    entities.room_case = 2;
    run_scenario(Bytes{36, 0, 0, 0, 0, 0}, entities);
    ck(entities.player_sleep_ticks == 2);
    entities.flags[10][0] = 0x800;
    entities.interaction_index = 1;
    run_scenario(Bytes{14, 0, 5, 0, 0, 0, 0, 0}, entities);
    ck(entities.flags[10][0] & 0x800);
    entities.interaction_index = 0;
    entities.interactions[0].flags = 1;
    entities.items[1].state_flags = 0;
    entities.flags[7][0] = 0x80000000;
    run_scenario(Bytes{14, 0, 0, 0, 0, 0, 0, 0}, entities);
    ck((entities.interactions[0].flags & 1) &&
       entities.items[1].state_flags == 0 && !(entities.flags[10][0] & 0x800));
    entities.flags[10][0] = 0x800;
    entities.flags[7][0] = 0;
    run_scenario(Bytes{14, 0, 0, 0, 0, 1, 0, 0}, entities);
    ck((entities.interactions[0].flags & 1) &&
       (entities.items[1].state_flags & 0x1000000));
    entities.interactions[1].flags = 0x81;
    entities.interactions[1].position = {1, 2, 3};
    run_scenario(Bytes{37, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, entities);
    ck(entities.interactions[1].flags == 0x81 &&
       entities.interactions[1].position.z == 3);
    for (auto opcode : {14, 35, 36, 37, 66})
      bad([&] { run_scenario(Bytes{uint8_t(opcode)}, entities); });
    bad([&] { run_scenario(Bytes{66, 3, 0, 0, 0, 0}, entities); });
    entities.flags[7][0] = 0x80000000;
    entities.interactions[0].flags = 1;
    run_scenario(Bytes{35, 1, 0, 2, 0, 0, 0, 0}, entities);
    ck(!(entities.interactions[0].flags & 1));
    state.interaction_index = 2;
    run_scenario(Bytes{4, 10, 23, 0, 3, 0, 55, 8, 0, 0}, state);
    ck(state.room_case == 9);
    std::cout << "Scenario entry boundaries passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
