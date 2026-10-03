#include "runtime.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void check(bool value, int line) {
  if (!value)
    throw std::runtime_error("Interaction check failed at " +
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
int main() {
  try {
    RoomRuntime runtime;
    runtime.world.player.position = {0, 0, 0};
    runtime.world.front_anchor = {0, 12.5f, 0};
    CollisionRecord trigger;
    trigger.flags = 1;
    trigger.floor = 0;
    trigger.type = 0;
    trigger.position = {-1, 0, -5};
    trigger.size = {2, 0, 1};
    trigger.parameters = {2, 3, 4, 5};
    trigger.attribute = 0x80;
    runtime.scenario.interactions = {trigger};
    ck(examine_room_interaction(runtime));
    ck(runtime.scenario.door && runtime.scenario.door->stage == 2 &&
       runtime.scenario.door->room == 3 &&
       runtime.scenario.door->position == 4 &&
       runtime.scenario.door->type == 5 &&
       runtime.scenario.door->attribute == 0x80 &&
       (runtime.scenario.flags[10][0] & 0x101) == 0x101 &&
       (runtime.scenario.flags[5][0] & 4));
    runtime.scenario.door.reset();
    runtime.scenario.flags[10][0] = 0;
    runtime.scenario.interactions[0].attribute = 0x1000;
    ck(!examine_room_interaction(runtime) &&
       !(runtime.scenario.flags[10][0] & 0x100));
    runtime.scenario.interactions[0].attribute = 0;
    runtime.world.floor = 1;
    ck(!examine_room_interaction(runtime));
    runtime.world.floor = 0;
    runtime.scenario.flags[11][0] = 1;
    ck(examine_room_interaction(runtime) && !runtime.scenario.door);
    runtime.scenario.flags[11][0] = 0;
    runtime.scenario.interactions[0].type = 3;
    runtime.scenario.interactions[0].position.z = -7;
    ck(examine_room_interaction(runtime) && !runtime.scenario.door &&
       runtime.scenario.interaction_index == 0 &&
       (runtime.scenario.flags[10][0] & 0x100));
    runtime.scenario.items.resize(1);
    runtime.scenario.items[0].flags = 1;
    runtime.scenario.items[0].id = 42;
    runtime.scenario.interactions[0].type = 4;
    runtime.scenario.interactions[0].position.z = -3;
    runtime.scenario.interactions[0].parameters[0] = 0;
    ck(examine_room_interaction(runtime) &&
       runtime.scenario.selected_item_id == 42 &&
       (runtime.scenario.flags[10][0] & 0x10));
    constexpr uint32_t directions[4] = {0x1000, 0x800, 0x400, 0x2000};
    runtime.scenario.interactions[0].type = 0;
    runtime.scenario.interactions[0].position = {-10, 0, -10};
    runtime.scenario.interactions[0].size = {20, 0, 20};
    for (size_t quadrant = 0; quadrant < 4; ++quadrant) {
      runtime.world.player.yaw = uint16_t(quadrant * 16384);
      runtime.scenario.interactions[0].attribute = directions[quadrant];
      ck(!examine_room_interaction(runtime));
      runtime.scenario.interactions[0].attribute = 0;
      ck(examine_room_interaction(runtime));
    }
    runtime.world.player.yaw = 0;
    runtime.scenario.interactions[0].attribute = 0x1000;
    auto allowed = runtime.scenario.interactions[0];
    allowed.attribute = 0;
    runtime.scenario.interactions.push_back(allowed);
    ck(examine_room_interaction(runtime) &&
       runtime.scenario.interaction_index == 1);
    runtime.scenario.interactions[0].flags = 0;
    runtime.scenario.interactions[0].type = 255;
    runtime.scenario.interactions[1].flags = 0;
    ck(!examine_room_interaction(runtime) &&
       runtime.scenario.interaction_index == 1 &&
       !(runtime.scenario.flags[10][0] & 0x100));
    runtime.scenario.interactions[0].flags = 1;
    bad([&] { examine_room_interaction(runtime); });
    runtime.scenario.interactions = {allowed};
    runtime.world.front_anchor = {10, 12.5f, 10};
    runtime.world.front_offset = 2;
    runtime.scenario.interactions[0].position = {9, 0, 7};
    runtime.scenario.interactions[0].size = {2, 0, 1};
    ck(examine_room_interaction(runtime));
    runtime.scenario.interactions[0].type = 4;
    ck(!examine_room_interaction(runtime));
    runtime.world.front_offset = std::numeric_limits<float>::infinity();
    bad([&] { examine_room_interaction(runtime); });
    std::cout << "Native examine interaction checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
