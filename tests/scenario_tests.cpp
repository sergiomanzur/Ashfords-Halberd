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
    ck(state.flags[6][0] == 0x48 && (state.flags[10][0] & 1) &&
       state.player_state_flags == 0x80010000);
    for (bool event_timer : {false, true})
      for (bool examination : {false, true}) {
        ScenarioState door_state;
        door_state.flags[6][0] = event_timer ? 0xffffffff : 0xfffffdff;
        door_state.flags[5][0] = 0xffffffff;
        door_state.flags[11][0] = examination ? 0x800c0 : 0x80000;
        run_scenario(Bytes{51, 0, 0x80, 0, 0, 2, 0, 1, 0}, door_state);
        ck(door_state.flags[6][0] == (event_timer ? 0x248u : 0x48u));
        ck(door_state.flags[5][0] == (examination ? 0xff3ffdffu : 0xffbffdffu));
        ck(door_state.flags[11][0] == (examination ? 0u : 0x80000u));
        ck(door_state.flags[12][0] == 0);
        ck(bool(door_state.drawing_flags & 1) == examination);
      }
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
    ScenarioState inventory;
    inventory.equipped_weapon = 1;
    run_scenario(Bytes{38, 1, 55, 4, 0, 0}, inventory);
    ck(inventory.room_case == 4);
    run_scenario(Bytes{38, 2, 55, 5, 0, 0}, inventory);
    ck(inventory.room_case == 4);
    inventory.player_id = 1;
    inventory.inventory[16] = 31u << 16;
    inventory.inventory[28] = 31u << 16;
    run_scenario(Bytes{17, 31, 55, 6, 0, 0}, inventory);
    ck(inventory.room_case == 4);
    inventory.inventory[17] = 31u << 16;
    run_scenario(Bytes{17, 31, 55, 6, 0, 0}, inventory);
    ck(inventory.room_case == 6);
    inventory.inventory[27] = 0x01480001;
    run_scenario(Bytes{17, 72, 0, 0}, inventory);
    ck(inventory.flags[4][0] & 0x10000);
    inventory.player_health = 100;
    run_scenario(Bytes{182, 7, 0, 0}, inventory);
    ck(inventory.room_sound_case == 7);
    run_scenario(Bytes{7, 0, 5, 2, 100, 0, 55, 7, 0, 0}, inventory);
    ck(inventory.room_case == 7);
    run_scenario(Bytes{7, 0, 5, 3, 100, 0, 55, 8, 0, 0}, inventory);
    ck(inventory.room_case == 7);
    Bytes loop_room(1300);
    put(loop_room, 0, 0x41200000);
    put(loop_room, 16, 128);
    put(loop_room, 28, 1200);
    put(loop_room, 32, 1240);
    put(loop_room, 1200, 12);
    put(loop_room, 1204, 14);
    put(loop_room, 1208, 16);
    Bytes for_code{250, 0, 3, 0, 55, 1, 254, 251, 0, 55, 2, 255, 0};
    std::copy(for_code.begin(), for_code.end(), loop_room.begin() + 1216);
    ScenarioState loops;
    loops.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(loop_room));
    loops.flags[6][0] = 0x10;
    start_scenario_event(loops, 0, 0);
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].counts[0] == 3 && loops.tasks[0].count2 == 3 &&
       loops.tasks[0].count3 == 3);
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].counts[0] == 2 && loops.tasks[0].count2 == 2);
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].counts[0] == 1 && loops.tasks[0].count2 == 1);
    tick_scenario_tasks(loops);
    ck(loops.room_case == 2 && loops.tasks[0].loop == -1 &&
       !loops.tasks[0].active);
    Bytes while_code{252, 4, 38, 1, 254, 253, 55, 9, 255, 0};
    std::copy(while_code.begin(), while_code.end(), loop_room.begin() + 1216);
    loops.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(loop_room));
    loops.tasks = {};
    loops.equipped_weapon = 1;
    loops.room_case = 0;
    start_scenario_event(loops, 0, 0);
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].pc == 5);
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].pc == 5 && loops.room_case == 0);
    loops.equipped_weapon = 0;
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].pc == 6 && loops.tasks[0].loop == -1);
    tick_scenario_tasks(loops);
    ck(loops.room_case == 9 && !loops.tasks[0].active);
    loops.tasks[0].counts[0] = 99;
    loops.tasks[0].count2 = 77;
    start_scenario_event(loops, 0, 0);
    ck(loops.tasks[0].counts[0] == 99 && loops.tasks[0].count2 == 77);
    loops.tasks[0].loop_bodies[0] = 1234;
    loops.tasks[0].count3 = 88;
    start_scenario_event(loops, 0, 0);
    ck(loops.tasks[0].loop_bodies[0] == 1234 && loops.tasks[0].count3 == 88);
    Bytes nested_for{250, 0, 2, 0, 248, 249, 1, 0, 251, 0, 55, 2, 255, 0};
    std::copy(nested_for.begin(), nested_for.end(), loop_room.begin() + 1216);
    loops.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(loop_room));
    loops.tasks = {};
    start_scenario_event(loops, 0, 0);
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].loop == 0 && loops.tasks[0].counts[0] == 2 &&
       loops.tasks[0].pc == 8);
    tick_scenario_tasks(loops);
    ck(loops.tasks[0].counts[0] == 1 && loops.tasks[0].pc == 8);
    tick_scenario_tasks(loops);
    ck(!loops.tasks[0].active && loops.tasks[0].loop == -1);
    for (uint16_t count : {uint16_t(0), uint16_t(65535)}) {
      Bytes endless{250, 0, uint8_t(count), uint8_t(count >> 8), 55, 2, 251, 0,
                    255, 0};
      std::copy(endless.begin(), endless.end(), loop_room.begin() + 1216);
      loops.scenario =
          std::make_shared<RoomScenario>(decode_room_scenario(loop_room));
      loops.tasks = {};
      start_scenario_event(loops, 0, 0);
      bad([&] { tick_scenario_tasks(loops); });
      ck(loops.tasks[0].loop == 0 && loops.tasks[0].counts[0] < 0);
    }
    Bytes too_deep;
    for (int i = 0; i < 7; ++i) {
      too_deep.push_back(252);
      too_deep.push_back(2);
    }
    too_deep.push_back(255);
    too_deep.push_back(0);
    std::copy(too_deep.begin(), too_deep.end(), loop_room.begin() + 1216);
    loops.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(loop_room));
    loops.tasks = {};
    start_scenario_event(loops, 0, 0);
    bad([&] { tick_scenario_tasks(loops); });
    ck(loops.tasks[0].loop == 5);
    start_scenario_event(loops, 0, 0);
    loops.tasks[0].loop = -2;
    bad([&] { tick_scenario_tasks(loops); });
    ck(loops.tasks[0].loop == -2);
    inventory.player_health = 100;
    for (uint8_t operation = 0; operation < 6; ++operation) {
      inventory.room_case = 0;
      run_scenario(Bytes{7, 0, 5, operation, 100, 0, 55, 9, 0, 0}, inventory);
      ck(inventory.room_case ==
         ((operation == 0 || operation == 2 || operation == 4) ? 9 : 0));
    }
    inventory.player_health = 32768;
    run_scenario(Bytes{7, 0, 5, 0, 0, 128, 55, 8, 0, 0}, inventory);
    ck(inventory.room_case == 8);
    inventory.enemy_slots = {1};
    inventory.enemies.resize(2);
    inventory.enemies[1].health = 200;
    run_scenario(Bytes{7, 0, 10, 0, 200, 0, 55, 7, 0, 0}, inventory);
    ck(inventory.room_case == 7);
    bad([&] { run_scenario(Bytes{7, 1, 10, 0, 200, 0, 0, 0}, inventory); });
    inventory.player_id = 23;
    inventory.inventory[369] = 72u << 16;
    inventory.flags[4][0] = 0;
    run_scenario(Bytes{17, 72, 55, 6, 0, 0}, inventory);
    ck(inventory.room_case == 6 && inventory.flags[4][0] == 0);
    inventory.player_id = 24;
    bad([&] { run_scenario(Bytes{17, 72, 0, 0}, inventory); });
    Bytes scheduled(1300);
    put(scheduled, 0, 0x41200000);
    put(scheduled, 16, 128);
    put(scheduled, 28, 1200);
    put(scheduled, 32, 1240);
    put(scheduled, 1200, 12);
    put(scheduled, 1204, 14);
    put(scheduled, 1208, 16);
    Bytes task_code{55, 1, 248, 249, 3, 0, 55, 2, 254, 55, 3, 255, 0};
    std::copy(task_code.begin(), task_code.end(), scheduled.begin() + 1216);
    ScenarioState scheduler;
    scheduler.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(scheduled));
    scheduler.flags[6][0] = 0x10;
    run_scenario(Bytes{20, 0, 16, 0, 0, 0}, scheduler);
    ck(scheduler.tasks[0].active && scheduler.tasks[0].pc == 0);
    tick_scenario_tasks(scheduler);
    ck(scheduler.room_case == 1 && scheduler.tasks[0].pc == 3 &&
       scheduler.event_timer == 1);
    tick_scenario_tasks(scheduler);
    ck(scheduler.tasks[0].pc == 3);
    tick_scenario_tasks(scheduler);
    ck(scheduler.tasks[0].pc == 6 && scheduler.room_case == 1);
    tick_scenario_tasks(scheduler);
    ck(scheduler.tasks[0].pc == 9 && scheduler.room_case == 2);
    tick_scenario_tasks(scheduler);
    ck(!scheduler.tasks[0].active && scheduler.room_case == 3 &&
       scheduler.event_timer == 5);
    tick_scenario_tasks(scheduler);
    ck(scheduler.event_timer == 5);
    auto initialization_room = scheduled;
    put(initialization_room, 32, 1280);
    put(initialization_room, 1204, 32);
    put(initialization_room, 1208, 40);
    Bytes initialization_code{101, 2, 0,  0, 105, 11, 0, 90,
                              0,   0, 20, 0, 0,   0,  0, 0};
    std::copy(initialization_code.begin(), initialization_code.end(),
              initialization_room.begin() + 1212);
    Bytes initialization_event{55, 9, 254, 255, 0};
    std::copy(initialization_event.begin(), initialization_event.end(),
              initialization_room.begin() + 1240);
    Bytes control_code{101, 2, 0, 0, 55, 7, 0, 0};
    std::copy(control_code.begin(), control_code.end(),
              initialization_room.begin() + 1232);
    ScenarioState initialized;
    initialized.scenario = std::make_shared<const RoomScenario>(
        decode_room_scenario(initialization_room));
    initialized.objects.resize(1);
    initialized.tasks[0].pc = 777;
    initialized.tasks[7].active = true;
    initialized.tasks[7].counts[0] = 123;
    initialized.event_timer = 900;
    initialized.flags[6][0] = 0x100;
    initialized.inventory[0] = 42;
    initialize_scenario(initialized);
    ck(initialized.objects[0].angles[1] == int32_t(90 * 182.04445f) &&
       initialized.room_case == 9 && initialized.tasks[0].active &&
       initialized.tasks[0].pc == 3 && initialized.event_timer == 1 &&
       !initialized.tasks[7].active && initialized.tasks[7].counts[0] == 123 &&
       initialized.flags[6][0] == 0x100 && initialized.inventory[0] == 42 &&
       initialized.current_task == 15);
    control_scenario(initialized);
    ck(initialized.room_case == 7 && initialized.tasks[15].work_index == 0 &&
       initialized.tasks[0].active && initialized.tasks[0].pc == 3 &&
       initialized.event_timer == 1);
    initialized.flags[6][0] |= 0x10;
    tick_scenario_tasks(initialized);
    ck(!initialized.tasks[0].active && initialized.event_timer == 2);
    start_scenario_event(scheduler, 3, 0);
    scheduler.flags[6][0] = 0;
    tick_scenario_tasks(scheduler);
    ck(scheduler.tasks[3].pc == 0 && scheduler.event_timer == 5);
    for (size_t slot = 0; slot < 16; ++slot)
      start_scenario_event(scheduler, slot, 0);
    scheduler.tasks[15].pc = 6;
    ck(start_scenario_event(scheduler, 16, 0) == 15 &&
       scheduler.tasks[15].pc == 0);
    run_scenario(Bytes{188, 3, 0, 0}, scheduler);
    ck(!scheduler.tasks[3].active);
    bad([&] { start_scenario_event(scheduler, 0, 1); });
    bad([&] { run_scenario(Bytes{248, 249, 1, 0, 0, 0}, state); });
    // A higher task slot started during iteration runs in the same tick;
    // a lower slot waits until the next scheduler pass.
    Bytes ordering = scheduled;
    put(ordering, 1200, 16);
    put(ordering, 1204, 18);
    put(ordering, 1208, 20);
    put(ordering, 1212, 30);
    Bytes spawn{20, 0, 1, 1, 255, 0};
    std::copy(spawn.begin(), spawn.end(), ordering.begin() + 1220);
    Bytes child{55, 7, 255, 0};
    std::copy(child.begin(), child.end(), ordering.begin() + 1230);
    ScenarioState ordered;
    ordered.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(ordering));
    ordered.flags[6][0] = 0x10;
    start_scenario_event(ordered, 0, 0);
    tick_scenario_tasks(ordered);
    ck(ordered.room_case == 7 && !ordered.tasks[0].active &&
       !ordered.tasks[1].active && ordered.event_timer == 1);
    spawn[2] = 0;
    std::copy(spawn.begin(), spawn.end(), ordering.begin() + 1220);
    ordered.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(ordering));
    ordered.tasks = {};
    ordered.room_case = 0;
    start_scenario_event(ordered, 1, 0);
    tick_scenario_tasks(ordered);
    ck(ordered.room_case == 0 && ordered.tasks[0].active);
    tick_scenario_tasks(ordered);
    ck(ordered.room_case == 7 && !ordered.tasks[0].active);
    Bytes self_spawn{20, 0, 0, 1, 254, 255, 0};
    std::copy(self_spawn.begin(), self_spawn.end(), ordering.begin() + 1220);
    ordered.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(ordering));
    ordered.tasks = {};
    ordered.room_case = 0;
    start_scenario_event(ordered, 0, 0);
    tick_scenario_tasks(ordered);
    ck(ordered.tasks[0].active && ordered.tasks[0].pc == 5 &&
       (*ordered.tasks[0].program)[0] == 20);
    tick_scenario_tasks(ordered);
    ck(!ordered.tasks[0].active && ordered.room_case == 0);
    Bytes conditional{1, 4, 254, 55, 4, 0, 55, 9, 255, 0};
    std::copy(conditional.begin(), conditional.end(), ordering.begin() + 1220);
    ordered.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(ordering));
    ordered.tasks = {};
    ordered.room_case = 0;
    start_scenario_event(ordered, 0, 0);
    tick_scenario_tasks(ordered);
    ck(ordered.room_case == 9 && !ordered.tasks[0].active &&
       ordered.conditional_depth == 0);
    Bytes conditional_sleep{1, 8, 248, 249, 2, 0, 55, 4, 0, 0, 55, 9, 255, 0};
    std::copy(conditional_sleep.begin(), conditional_sleep.end(),
              ordering.begin() + 1220);
    ordered.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(ordering));
    ordered.tasks = {};
    ordered.room_case = 0;
    start_scenario_event(ordered, 0, 0);
    tick_scenario_tasks(ordered);
    ck(ordered.room_case == 9 && !ordered.tasks[0].active &&
       ordered.tasks[0].counts[0] == 1);
    bad([&] { run_scenario(Bytes{188, 16, 0, 0}, ordered); });
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
    Bytes transform_room(1400);
    put(transform_room, 0, 0x41200000);
    put(transform_room, 16, 128);
    put(transform_room, 28, 1200);
    put(transform_room, 32, 1390);
    put(transform_room, 1200, 12);
    put(transform_room, 1204, 14);
    put(transform_room, 1208, 16);
    Bytes transforms{101,  1,    0,   3,  105, 12,  1,   0,   2,   0,   105,
                     13,   0,    2,   1,  0,   105, 7,   123, 0,   200, 0,
                     0xff, 0xff, 105, 11, 90,  180, 255, 0,   254, 255, 0};
    std::copy(transforms.begin(), transforms.end(),
              transform_room.begin() + 1216);
    ScenarioState transformed;
    transformed.scenario =
        std::make_shared<RoomScenario>(decode_room_scenario(transform_room));
    transformed.enemies.resize(2);
    transformed.enemy_slots = {1};
    transformed.collision = RoomCollision{};
    transformed.enemies[1].motion_mode = 123;
    transformed.enemies[1].model_flags = 456;
    transformed.enemies[1].blend_rate = 16777217;
    transformed.flags[6][0] = 0x10;
    start_scenario_event(transformed, 0, 0);
    tick_scenario_tasks(transformed);
    auto &bound = transformed.enemies[1];
    ck(bound.position.x == -1.23f && bound.position.y == 2.0f &&
       bound.position.z == -655.35f);
    ck(bound.previous_position.z == bound.position.z &&
       bound.floor == player_floor_number(*transformed.collision, 2));
    ck(bound.angles[0] == int32_t(90 * 182.04445f) &&
       bound.angles[1] == int32_t(-180 * 182.04445f) &&
       bound.previous_angles == bound.angles);
    ck(transformed.enemies[0].position.x == 0 &&
       transformed.tasks[0].model_index == 3);
    ck(transformed.tasks[0].motion_mode == 123 &&
       transformed.tasks[0].model_flags == 456 &&
       transformed.tasks[0].blend_rate == 16777217);
    tick_scenario_tasks(transformed);
    ck(!transformed.tasks[0].active);
    auto controller_case = [&](Bytes instructions, bool rejected) {
      std::fill(transform_room.begin() + 1216, transform_room.begin() + 1390,
                0);
      std::copy(instructions.begin(), instructions.end(),
                transform_room.begin() + 1216);
      transformed.scenario =
          std::make_shared<RoomScenario>(decode_room_scenario(transform_room));
      start_scenario_event(transformed, 1, 0);
      if (rejected)
        bad([&] { tick_scenario_tasks(transformed); });
      else
        tick_scenario_tasks(transformed);
      transformed.tasks[1].active = false;
    };
    transformed.objects.resize(2);
    transformed.items.resize(2);
    controller_case(Bytes{101, 2, 1, 7, 105, 11, 0, 180, 0, 0, 255, 0}, false);
    ck(transformed.objects[1].angles[1] == int32_t(180 * 182.04445f) &&
       transformed.tasks[1].model_index == 7);
    // Object/item bindings retain the task's cached enemy motion state.
    transformed.tasks[1].motion_mode = 789;
    controller_case(Bytes{101, 3, 1, 7, 255, 0}, false);
    ck(transformed.tasks[1].model_index == 0 &&
       transformed.tasks[1].motion_mode == 789);
    controller_case(Bytes{101, 1, 4, 0}, true);
    controller_case(Bytes{101, 2, 4, 0}, true);
    controller_case(Bytes{101, 3, 4, 0}, true);
    controller_case(Bytes{101, 4, 0, 0}, true);
    controller_case(Bytes{105, 47}, true);
    ActorModel empty_model;
    empty_model.model.resize(68);
    empty_model.model[0] = 'M';
    empty_model.model[1] = 'D';
    empty_model.model[2] = 'L';
    empty_model.model[3] = 128;
    empty_model.model[4] = 16;
    empty_model.model[6] = 1;
    put(empty_model.model, 8, 0xffffffff);
    put(empty_model.model, 16, 6);
    put(empty_model.model, 20, 0xffffffff);
    put(empty_model.model, 60, 0xffffffff);
    put(empty_model.model, 64, 0xffffffff);
    Bytes binding_room(4000);
    put(binding_room, 0, 0x41200000);
    put(binding_room, 16, 128);
    put(binding_room, 136, 1132);
    put(binding_room, 264, 3);
    for (size_t authored : {size_t(1), size_t(2)}) {
      size_t p = 1132 + authored * 36;
      put(binding_room, p, 1);
      put(binding_room, p + 4, authored == 1 ? 0x120043 : 91);
      binding_room[p + 9] = 3;
      binding_room[p + 8] = 9; // Floor is recalculated from room ground.
      binding_room[p + 10] = uint8_t(authored - 1);
      put(binding_room, p + 12, 0x41200000); // x = 10.
      binding_room[p + 24] = 7; // Authored x/z are ignored by bhSetEnemy.
      binding_room[p + 28] = 128;
      binding_room[p + 29] = 255; // Signed yaw = -128.
    }
    binding_room[1142] = 7; // Inactive entry retains its authored work mapping.
    put(binding_room, 20, 1240);
    put(binding_room, 1240, 1300);
    put(binding_room, 1244, 1400);
    put(binding_room, 1248, 3000);
    std::copy(empty_model.model.begin(), empty_model.model.end(),
              binding_room.begin() + 1300);
    put(binding_room, 32, 1800);
    put(binding_room, 1800, 1);
    put(binding_room, 1804, 1900);
    put(binding_room, 1900, 0xffffffff);
    for (size_t p : {size_t(1400)}) {
      put(binding_room, p, uint32_t(empty_model.model.size()));
      std::copy(empty_model.model.begin(), empty_model.model.end(),
                binding_room.begin() + p + 4);
      put(binding_room, p + 4 + empty_model.model.size(), 0xffffffff);
    }
    // ID91's absent-MASK finish-room path addresses O_WORK joints 6..10.
    // Use a valid 11-joint skeleton instead of the old one-joint placeholder.
    auto maskless = empty_model.model;
    maskless.resize(16 + 11 * 52);
    maskless[6] = 11;
    for (size_t joint = 0; joint < 11; ++joint) {
      const size_t p = 16 + joint * 52;
      put(maskless, p, 6);
      put(maskless, p + 4, 0xffffffff);
      put(maskless, p + 44,
          joint < 10 ? uint32_t((joint + 1) * 52) : 0xffffffff);
      put(maskless, p + 48, 0xffffffff);
    }
    put(binding_room, 3000, uint32_t(maskless.size()));
    std::copy(maskless.begin(), maskless.end(), binding_room.begin() + 3004);
    put(binding_room, 3004 + maskless.size(), 0xffffffff);
    RoomCollision binding_collision;
    ScenarioState bound_room;
    bound_room.flags[1][0] = 0x12345678;
    bound_room.inventory[0] = 99;
    bind_room_enemies(binding_room, binding_collision, bound_room);
    ck(bound_room.enemies.size() == 128 &&
       bound_room.enemy_slots == std::vector<size_t>({7, 0, 1}));
    ck(bound_room.enemies[0].id == 67 &&
       bound_room.enemies[0].flags == 0x8001 &&
       bound_room.enemies[0].type == 18 &&
       bound_room.enemies[0].model_version == 3 &&
       bound_room.enemies[0].model_flags == 0x20 &&
       bound_room.enemies[0].position.x == 10 &&
       bound_room.enemies[0].angles == std::array<int32_t, 3>{0, -128, 0} &&
       bound_room.enemies[0].floor == 0 && bound_room.enemies[0].model &&
       bound_room.enemies[0].model_bank->models.size() == 1 &&
       bound_room.enemies[0].motions->size() == 512);
    ck(bound_room.enemies[1].id == 91 &&
       bound_room.enemies[1].model_flags == 0x20 &&
       bound_room.enemies[1].model_work->joints.size() == 11 &&
       (bound_room.enemies[1].model_work->joints[10].flags & 4) &&
       bound_room.enemies[2].flags == 0 && !bound_room.enemies[3].motions &&
       bound_room.flags[1][0] == 0x12345678 && bound_room.inventory[0] == 99);
    auto malformed_binding = binding_room;
    put(malformed_binding, 1248, 1249);
    bad([&] {
      bind_room_enemies(malformed_binding, binding_collision, bound_room);
    });
    ck(bound_room.enemies[0].id == 67 && bound_room.enemy_slots[0] == 7);
    malformed_binding = binding_room;
    malformed_binding[1142] = 128;
    bad([&] {
      bind_room_enemies(malformed_binding, binding_collision, bound_room);
    });
    auto prop_room = binding_room;
    prop_room.resize(4000);
    put(prop_room, 140, 2100);
    put(prop_room, 268, 5);
    put(prop_room, 144, 2300);
    put(prop_room, 272, 1);
    put(prop_room, 1252, 2500);
    put(prop_room, 1256, 2700);
    for (size_t p : {size_t(2500), size_t(2700)})
      std::copy(empty_model.model.begin(), empty_model.model.end(),
                prop_room.begin() + p);
    for (size_t p : {size_t(2244), size_t(2300)}) {
      put(prop_room, p, 0xa0000001);
      put(prop_room, p + 4, 0xab12000a);
      prop_room[p + 8] = 254;
      prop_room[p + 9] = 3;
      prop_room[p + 11] = 255;
      put(prop_room, p + 12, 0x41200000);
      prop_room[p + 24] = 255;
      prop_room[p + 25] = 255;
      prop_room[p + 26] = 2;
      prop_room[p + 28] = 3;
      prop_room[p + 30] = 254;
      prop_room[p + 31] = 255;
      prop_room[p + 32] = 255;
    }
    bound_room.objects.resize(8);
    bound_room.objects[0].id = 77;
    bind_room_props(prop_room, bound_room);
    ck(bound_room.objects.size() == 5 && bound_room.objects[0].id == 77 &&
       !bound_room.objects[0].model && bound_room.items.size() == 1);
    for (const auto *entity : {&bound_room.objects[4], &bound_room.items[0]})
      ck(entity->id == 10 && entity->type == 0x12 &&
         entity->parameter == 0xab && entity->floor == -2 &&
         entity->draw_type == -1 && entity->model_version == 3 &&
         entity->model_flags == 0x50 && entity->flags == 0xa0000001 &&
         entity->position.x == 10 &&
         entity->angles == std::array<int32_t, 3>{-1, 3, 2} &&
         entity->angular_speed == -2 && entity->hide[0] == -1 && entity->model);
    auto invalid_props = prop_room;
    put(invalid_props, 1256, 3999);
    bad([&] { bind_room_props(invalid_props, bound_room); });
    ck(bound_room.objects[0].id == 77 && bound_room.items[0].id == 10 &&
       bound_room.flags[1][0] == 0x12345678 && bound_room.inventory[0] == 99);
    ScenarioState props;
    props.flags[6][0] = 4;
    props.items.resize(1);
    auto &pickup = props.items[0];
    pickup.flags = 1;
    pickup.angular_speed = 100;
    pickup.position = {1, 2, 3};
    pickup.angles = {4, 5, 6};
    pickup.model = std::make_shared<const ActorModel>(empty_model);
    tick_scenario_props(props);
    ck(pickup.control1 == 1 && pickup.control2 == 0 && pickup.control3 == 75 &&
       props.random_seed == 1103527590u && pickup.rendered_model &&
       pickup.previous_position.x == 1 && pickup.previous_angles[1] == 5);
    for (int i = 0; i < 75; ++i)
      tick_scenario_props(props);
    ck(pickup.control1 == 1 && pickup.control2 == 75 &&
       !(pickup.model_flags & 0x80));
    tick_scenario_props(props);
    ck(pickup.control1 == 2 && pickup.control2 == 0 &&
       (pickup.model_flags & 0x80) && pickup.highlight_yaw == 0);
    for (int i = 0; i < 8; ++i)
      tick_scenario_props(props);
    ck(pickup.control1 == 3 && pickup.control2 == 0 &&
       !(pickup.model_flags & 0x80) && pickup.highlight_yaw == 65536 &&
       pickup.angles[1] == 5);
    tick_scenario_props(props);
    ck(props.random_seed == 2524885223u && pickup.control1 == 1 &&
       pickup.control3 == 65);
    pickup.state_flags = 0x1000000;
    props.player_sleep_ticks = 1;
    tick_scenario_props(props);
    ck(pickup.control2 == 0);
    pickup.state_flags = 0;
    props.flags[10][0] = 0x40000000;
    tick_scenario_props(props);
    ck(pickup.control2 == 0);
    props.flags[10][0] = 0;
    props.flags[6][0] = 0;
    tick_scenario_props(props);
    ck(pickup.control2 == 0);
    props.flags[6][0] = 4;
    props.objects.resize(6);
    for (size_t i = 4; i < 6; ++i) {
      props.objects[i].flags = 1;
      props.objects[i].model = pickup.model;
    }
    auto &rotating = props.objects[4];
    rotating.type = 2;
    rotating.angular_speed = -1;
    rotating.angles[1] = -100;
    auto &oscillating = props.objects[5];
    oscillating.type = 3;
    oscillating.control1 = 16384;
    tick_scenario_props(props);
    ck(rotating.angles[1] == 65345 && rotating.previous_angles[1] == -100 &&
       oscillating.angles[2] == 2048 && oscillating.control1 == 17408);
    rotating.state_flags = 0x1000000;
    props.player_sleep_ticks = 0;
    tick_scenario_props(props);
    ck(rotating.angles[1] == 65345);
    props.player_sleep_ticks = 1;
    tick_scenario_props(props);
    ck(rotating.angles[1] == 130790);
    rotating.flags |= 0x200000;
    bad([&] { tick_scenario_props(props); });
    rotating.flags = 0x1001;
    bad([&] { tick_scenario_props(props); });
    props.objects.clear();
    pickup.flags = 0x81;
    bad([&] { tick_scenario_props(props); });
    pickup.flags = 0x1001;
    bad([&] { tick_scenario_props(props); });
    transformed.enemies[1].model =
        std::make_shared<const ActorModel>(empty_model);
    controller_case(Bytes{101, 1, 0, 99, 105, 47, 255, 0}, false);
    ck(transformed.enemies[1].rendered_model.has_value());
    transformed.enemies[1].joint_pose.resize(2);
    controller_case(Bytes{101, 1, 0, 0, 105, 47, 255, 0}, true);
    transformed.enemies[1].joint_pose.clear();
    auto triangle_model = empty_model;
    triangle_model.model.resize(400);
    auto &triangle_bytes = triangle_model.model;
    put(triangle_bytes, 20, 128);
    put(triangle_bytes, 24, 0x41200000); // Root position x = 10.
    put(triangle_bytes, 144, 152);
    put(triangle_bytes, 148, 328);
    triangle_bytes[168] = 51;
    triangle_bytes[170] = 39;
    triangle_bytes[174] = 3;
    put(triangle_bytes, 232, 0x3f800000);
    put(triangle_bytes, 268, 0x3f800000);
    triangle_bytes[328] = 255;
    triangle_bytes[344] = 66;
    triangle_bytes[346] = 11;
    triangle_bytes[348] = 1;
    triangle_bytes[350] = 3;
    triangle_bytes[358] = 1;
    triangle_bytes[364] = 2;
    triangle_bytes[370] = 255;
    transformed.enemies[1].model =
        std::make_shared<const ActorModel>(triangle_model);
    transformed.enemies[1].position = {100, 200, 300};
    transformed.enemies[1].angles = {0, 16384, 0};
    controller_case(Bytes{101, 1, 0, 99, 105, 47, 255, 0}, false);
    auto rebuilt_vertex = transformed.enemies[1]
                              .rendered_model->meshes[0]
                              .triangles[0]
                              .corners[0]
                              .position;
    ck(rebuilt_vertex.x == 100 && rebuilt_vertex.y == 200 &&
       rebuilt_vertex.z == 289);
    for (uint32_t matrix_flag : {0x80u, 0x1000u}) {
      transformed.enemies[1].flags = matrix_flag;
      controller_case(Bytes{101, 1, 0, 0, 105, 47, 255, 0}, true);
    }
    transformed.enemies[1].flags = 0;
    transformed.room_motions =
        std::make_shared<const std::vector<std::optional<Motion>>>(512);
    transformed.enemies[1].motion_number = 0xab00;
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 1, 1, 7, 0, 1, 16, 255, 0},
                    false);
    ck(bound.motion_number == 0xab07 && bound.motion_mode == 0x20 &&
       bound.motion_step == 65536 && bound.blend_count == 16 &&
       bound.control2 == 10 && bound.modes[0] == 1 && bound.modes[1] == 7 &&
       bound.motions == transformed.room_motions);
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 4, 1, 0, 1, 1, 6, 255, 0},
                    false);
    ck(bound.motion_step == 0x5555 && bound.motion_mode == 0x28 &&
       bound.blend_count == 16 && bound.control2 == 6 && bound.modes[0] == 5);
    ck(bound.rendered_model->meshes[0].triangles[0].corners[0].position.z ==
       289);
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 25, 1, 0, 0, 1, 8, 255, 0},
                    false);
    ck(bound.motion_step == 0xaaa && bound.blend_count == 16 &&
       bound.control2 == 8);
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 26, 1, 0, 0, 1, 0}, true);
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 1, 0, 0, 0, 1, 0}, true);
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 1, 1, 0, 0, 2, 0}, true);
    bound.id = 1;
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 1, 1, 0, 0, 1, 0}, true);
    bound.id = 0;
    transformed.room_motions.reset();
    controller_case(Bytes{101, 1, 0, 0, 105, 24, 1, 1, 0, 0, 1, 0}, true);
    bound.frame = 149 * 65536 - 1;
    run_scenario(Bytes{56, 1, 0, 0, 149, 0, 55, 4, 0}, transformed);
    ck(transformed.room_case == 4);
    bound.frame = 149 * 65536;
    run_scenario(Bytes{56, 1, 0, 0, 149, 0, 55, 5, 0}, transformed);
    ck(transformed.room_case == 4);
    run_scenario(Bytes{56, 1, 0, 1, 149, 0, 55, 6, 0}, transformed);
    ck(transformed.room_case == 6);
    transformed.player_frame = -2147483647 - 1;
    run_scenario(Bytes{56, 0, 0, 0, 0, 128, 55, 7, 0}, transformed);
    ck(transformed.room_case == 6);
    run_scenario(Bytes{56, 0, 0, 1, 0, 128, 55, 7, 0}, transformed);
    ck(transformed.room_case == 7);
    transformed.objects[1].frame = -65537;
    run_scenario(Bytes{56, 2, 1, 0, 255, 255, 55, 8, 0}, transformed);
    ck(transformed.room_case == 8);
    bad([&] { run_scenario(Bytes{56, 3, 0, 0, 0, 0}, transformed); });
    bad([&] { run_scenario(Bytes{56, 1, 9, 0, 0, 0}, transformed); });
    bad([&] { run_scenario(Bytes{56, 2, 9, 0, 0, 0}, transformed); });
    bad([&] { run_scenario(Bytes{56, 0, 0, 0, 0}, transformed); });
    auto animated_bank =
        std::make_shared<std::vector<std::optional<Motion>>>(512);
    Motion animation;
    animation.flags = 0x80;
    animation.frames = 150;
    animation.tracks.resize(1);
    animation.tracks[0].positions.assign(150, Vec3{26, 0, 0});
    animation.tracks[0].angles.assign(150, std::array<uint16_t, 3>{});
    animation.attributes.assign(150, 0x1234);
    (*animated_bank)[0] = animation;
    bound.motions = animated_bank;
    bound.id = 67;
    bound.flags = 1;
    bound.modes = {1, 0, 0, 0};
    bound.control2 = 10;
    bound.frame = 0;
    bound.motion_mode = 0x20;
    bound.motion_step = 65536;
    bound.blend_count = 16;
    bound.joint_pose.clear();
    transformed.flags[6][0] = 0;
    tick_scenario_actors(transformed);
    ck(bound.frame == 0);
    transformed.flags[6][0] = 2;
    tick_scenario_actors(transformed);
    ck(bound.frame == 65536 && bound.blend_count == 15 &&
       bound.motion_number == 0 && bound.motion_attribute == 0x1234);
    ck(bound.rendered_model->meshes[0].triangles[0].corners[0].position.z ==
       288);
    for (int tick = 1; tick < 16; ++tick)
      tick_scenario_actors(transformed);
    ck(bound.blend_count == 0 && bound.frame == 16 * 65536 &&
       bound.joint_pose[0].position->x == 26);
    ScenarioState startup;
    startup.flags[6][0] = 2;
    startup.enemies.push_back(bound);
    auto &starting = startup.enemies[0];
    starting.modes = {};
    starting.control2 = 0;
    starting.frame = 42 * 65536;
    starting.motion_number = 123;
    starting.motion_step = 65536;
    starting.blend_count = 7;
    starting.blend_rate = 900;
    starting.frame_direction = 0xffffffff;
    starting.joint_pose.clear();
    tick_scenario_actors(startup);
    ck(starting.modes[0] == 1 && starting.modes[3] == 3 &&
       starting.motion_number == 0 && starting.motion_step == 32768 &&
       starting.motion_mode == 0x20 && starting.frame == 32768 &&
       starting.frame_direction == 0 && starting.blend_count == 0 &&
       starting.blend_rate == 0 && starting.joint_pose[0].position->x == 26);
    tick_scenario_actors(startup);
    ck(starting.frame == 32768 && starting.rendered_model.has_value());
    starting.motions = std::make_shared<const MotionBank>(512);
    starting.modes = {};
    starting.joint_pose.clear();
    tick_scenario_actors(startup);
    ck(starting.frame == 0 && starting.modes[3] == 3 &&
       starting.joint_pose.empty() && !starting.rendered_model->meshes.empty());
    tick_scenario_actors(startup);
    ck(starting.frame == 0);
    starting.motions = animated_bank;
    starting.modes[3] = 0;
    starting.joint_pose.clear();
    tick_scenario_actors(startup);
    ck(starting.frame == 32768 && starting.modes[3] == 1 &&
       starting.blend_count == 7 && starting.joint_pose[0].position->x == 12);
    bound.state_flags = 0x40000000;
    tick_scenario_actors(transformed);
    ck(bound.frame == 16 * 65536);
    auto two_joint_model = triangle_model;
    two_joint_model.model[6] = 2;
    put(two_joint_model.model, 60, 52);
    put(two_joint_model.model, 68, 6);
    put(two_joint_model.model, 72, 0xffffffff);
    put(two_joint_model.model, 80, 0x40000000);
    put(two_joint_model.model, 112, 0xffffffff);
    put(two_joint_model.model, 116, 0xffffffff);
    bound.model = std::make_shared<const ActorModel>(two_joint_model);
    bound.state_flags = 0;
    bound.flags = 1;
    bound.frame = 0;
    bound.modes[3] = 0;
    bound.joint_pose.clear();
    bound.blend_count = 2;
    animation.tracks.resize(2);
    animation.tracks[1].positions.assign(150, Vec3{0, 10, 0});
    animation.tracks[1].angles.assign(150, std::array<uint16_t, 3>{});
    (*animated_bank)[0] = animation;
    tick_scenario_actors(transformed);
    ck(bound.joint_pose[1].position->y == 2);
    animation.flags = 0x90;
    (*animated_bank)[0] = animation;
    bound.frame = 0;
    bound.modes[3] = 0;
    bound.joint_pose.clear();
    bound.blend_count = 2;
    tick_scenario_actors(transformed);
    ck(bound.joint_pose[1].position->y == 6);
    animation.flags = 0xa0;
    animation.frames = 2;
    for (auto &track : animation.tracks) {
      track.positions.resize(2);
      track.angles.resize(2);
    }
    animation.tracks[0].positions = {Vec3{26, 0, 0}, Vec3{34, 0, 0}};
    animation.tracks[1].positions = {Vec3{0, 10, 0}, Vec3{0, 14, 0}};
    animation.attributes.resize(2);
    (*animated_bank)[0] = animation;
    bound.frame = 32768;
    bound.motion_step = 32768;
    bound.joint_pose.clear();
    bound.blend_count = 2;
    tick_scenario_actors(transformed);
    ck(bound.joint_pose[0].position->x == 20 &&
       bound.joint_pose[1].position->y == 7 && bound.frame == 65536);
    animation.tracks[0].angles.assign(2,
                                      std::array<uint16_t, 3>{100, 200, 300});
    animation.tracks[1].angles.clear();
    (*animated_bank)[0] = animation;
    bound.frame = 0;
    bound.blend_count = 1;
    bound.joint_pose = actor_bind_pose(*bound.model);
    tick_scenario_actors(transformed);
    ck(*bound.joint_pose[1].angles == std::array<uint16_t, 3>{0, 200, 300});
    ck(transformed.motion_angle_scratch ==
       std::array<uint16_t, 3>{0, 200, 300});
    auto scratch_copy = transformed;
    scratch_copy.motion_angle_scratch[1] = 99;
    ck(transformed.motion_angle_scratch[1] == 200);
    transformed.flags[6][0] = 0x10;
    transformed.tasks[1].work_index.reset();
    controller_case(Bytes{105, 11, 0, 0, 0, 0}, true);
    transformed.collision.reset();
    controller_case(Bytes{101, 1, 0, 0, 105, 7, 0, 0, 0, 0, 0, 0}, true);
    bad([&] { run_scenario(Bytes{101, 1, 0, 0, 0}, transformed); });
    std::cout << "Scenario entry boundaries passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
