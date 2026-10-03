#include "scenario.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Unsupported enemy frame did not fail");
}
ScenarioState manager_fixture() {
  ScenarioState state;
  state.enemies.resize(128);
  state.flags[6][0] = 2;
  state.enemy_callbacks[38] = EnemyCallbackKind::ZombieManager;
  auto &manager = state.enemies[0];
  manager.id = 38;
  manager.flags = 1;
  for (size_t i = 1; i <= 3; ++i) {
    auto &root = state.enemies[i];
    root.id = 1;
    root.flags = 0x200001;
    root.state_flags = 0x40000000; // Root callback remains separate; manager
                                   // source ignores this bit.
    root.zombie_work.emplace();
    root.zombie_secondary_flags = 0x42;
    root.zombie_work->player_distance = i == 1 ? 30 : i == 2 ? 40 : 10;
    root.floor = i == 3 ? 0 : 2;
  }
  return state;
}
std::shared_ptr<const ActorModel> model(size_t joints) {
  ActorModel actor;
  actor.model.resize(16 + joints * 52);
  actor.model[0] = 'M';
  actor.model[1] = 'D';
  actor.model[2] = 'L';
  actor.model[3] = 128;
  actor.model[4] = 16;
  actor.model[6] = uint8_t(joints);
  auto put = [&](size_t p, uint32_t value) {
    for (size_t i = 0; i < 4; ++i)
      actor.model[p + i] = uint8_t(value >> (8 * i));
  };
  put(8, 0xffffffff);
  for (size_t i = 0; i < joints; ++i) {
    const auto p = 16 + i * 52;
    put(p, 6);
    put(p + 4, 0xffffffff);
    for (size_t axis = 0; axis < 3; ++axis)
      put(p + 32 + axis * 4, 0x3f800000);
    put(p + 44, i + 1 < joints ? uint32_t((i + 1) * 52) : 0xffffffff);
    put(p + 48, 0xffffffff);
  }
  return std::make_shared<const ActorModel>(std::move(actor));
}
int main() {
  try {
    auto state = manager_fixture();
    auto unregistered = state;
    unregistered.enemy_callbacks = {};
    bad([&] { tick_scenario_actors(unregistered); });
    ck(unregistered.enemies[0].modes[0] == 0,
       "Unregistered shared callback ID was treated as zombie work");
    tick_scenario_actors(state);
    ck(state.enemies[0].modes == std::array<uint8_t, 4>{1, 0, 0, 0},
       "Manager startup missing");
    ck(state.enemies[1].zombie_secondary_flags == 0x42,
       "Manager startup ran movement early");
    tick_scenario_actors(state);
    ck(state.enemies[1].zombie_secondary_flags == 0x40 &&
           state.enemies[2].zombie_secondary_flags == 0x40 &&
           state.enemies[3].zombie_secondary_flags == 0x42,
       "Nearest root cached-distance policy missing");
    ck(!(state.enemies[1].flags & 0x200000) &&
           !(state.enemies[2].flags & 0x200000) &&
           (state.enemies[3].flags & 0x200000),
       "Two-root stair restriction missing");
    state.enemies[1].zombie_work->player_distance = 10;
    tick_scenario_actors(state);
    ck((state.enemies[1].zombie_secondary_flags & 2) &&
           !(state.enemies[3].zombie_secondary_flags & 2),
       "Equal distance did not retain first root");
    for (int excluded = 0; excluded < 6; ++excluded) {
      auto skipped = manager_fixture();
      skipped.enemies[0].modes[0] = 1;
      auto &root = skipped.enemies[1];
      root.zombie_work->player_distance = 0;
      if (excluded == 0)
        root.type = 10;
      if (excluded == 1)
        root.state_flags |= 0x1000000;
      if (excluded == 2)
        root.flags |= 0x80;
      if (excluded == 3)
        root.flags |= 2;
      if (excluded == 4)
        root.flags &= ~1u;
      if (excluded == 5)
        root.zombie_work.reset();
      tick_scenario_actors(skipped);
      ck(root.zombie_secondary_flags == 0x42 && (root.flags & 0x200000),
         "Excluded root policy was changed");
      ck(skipped.enemies[3].zombie_secondary_flags & 2,
         "Eligible nearest root lost selection");
    }
    for (auto where : {std::pair<uint8_t, uint8_t>{1, 5}, {0, 5}, {1, 6}}) {
      auto staged = manager_fixture();
      staged.enemies[0].modes[0] = 1;
      staged.stage = where.first;
      staged.room = where.second;
      tick_scenario_actors(staged);
      ck(bool(staged.enemies[1].flags & 0x200000) ==
             (where.first == 1 && where.second == 5),
         "Stair policy did not preserve stage-OR-room match");
    }
    state.enemies[2].floor = 0;
    tick_scenario_actors(state);
    ck(!(state.enemies[3].flags & 0x200000),
       "Stair restriction persisted below floor limit");
    auto malformed = manager_fixture();
    malformed.enemies[0].modes[0] = 1;
    malformed.enemies[3].zombie_work->player_distance =
        std::numeric_limits<float>::quiet_NaN();
    bad([&] { tick_scenario_actors(malformed); });
    ck(malformed.enemies[1].zombie_secondary_flags == 0x42 &&
           (malformed.enemies[1].flags & 0x200000),
       "Invalid distance partially committed manager policy");
    auto overflow = manager_fixture();
    overflow.enemies[0].modes[0] = 1;
    for (size_t i = 4; i < 14; ++i) {
      overflow.enemies[i] = overflow.enemies[3];
      overflow.enemies[i].zombie_work->player_distance = float(i);
    }
    bad([&] { tick_scenario_actors(overflow); });
    ck(overflow.enemies[1].zombie_secondary_flags == 0x42,
       "Stair scratch overflow partially committed nearest bit");
    for (uint16_t id : {31, 32, 33}) {
      ScenarioState parts;
      parts.enemies.resize(128);
      parts.flags[6][0] = 2;
      auto &part = parts.enemies[1];
      part.id = id;
      parts.enemy_callbacks[id] = id == 31   ? EnemyCallbackKind::ZombieHead
                                  : id == 32 ? EnemyCallbackKind::ZombieArm
                                             : EnemyCallbackKind::ZombieLeg;
      part.flags = 1;
      tick_scenario_actors(parts);
      ck(part.modes == std::array<uint8_t, 4>{1, 0, 0, 0} &&
             part.zombie_part_work &&
             part.zombie_part_work->words.size() == (id == 31 ? 8 : 28),
         "Part startup missing in scheduler");
      part.modes[1] = 2;
      part.modes[2] = 3;
      part.modes[3] = 4;
      part.zombie_part_work->words[3] = 99;
      tick_scenario_actors(parts);
      ck(part.modes == std::array<uint8_t, 4>{1, 2, 3, 4} &&
             part.zombie_part_work->words[3] == 99,
         "Original empty part idle callback changed work");
      part.modes[0] = 2;
      part.damage.fill(77);
      bad([&] { tick_scenario_actors(parts); });
      ck(part.damage[0] == 77, "Unsupported damage callback performed cleanup");
      part.flags |= 0x80;
      part.linked_parent = 0;
      parts.enemies[0].state_flags = 0x1000000;
      tick_scenario_actors(
          parts); // Source suppresses callback and cleanup for a hidden parent.
      ck(part.damage[0] == 77,
         "Hidden linked parent did not suppress dispatch");
      part.linked_parent.reset();
      bad([&] { tick_scenario_actors(parts); });
    }
    ScenarioState common;
    common.flags[6][0] = 2;
    common.enemies.resize(1);
    auto &held = common.enemies[0];
    held.flags = 1;
    held.id = 67;
    held.modes = {1, 0, 0, 3};
    held.model = model(3);
    held.motions = std::make_shared<const MotionBank>(MotionBank{std::nullopt});
    held.damage.fill(99);
    held.combo_timeout = 2;
    held.combo_flags = 0xff;
    held.secondary_flags = 0xf;
    held.state_flags = 0x20;
    tick_scenario_actors(common);
    ck(held.damage[0] == 0 && held.damage[2] == 0 && held.damage[3] == 99 &&
           held.combo_timeout == 1 && held.combo_flags == 0x8f &&
           held.secondary_flags == 0xb && held.state_flags == 0x20,
       "Common post-controller cleanup missing");
    held.combo_timeout = -1;
    tick_scenario_actors(common);
    ck(held.combo_timeout == -2,
       "Negative combo timeout did not retain source decrement");
    held.combo_timeout = std::numeric_limits<int32_t>::min();
    tick_scenario_actors(common);
    ck(held.combo_timeout == std::numeric_limits<int32_t>::max(),
       "Combo timeout word wrap missing");
    ScenarioState aliases;
    aliases.enemies.resize(128);
    aliases.flags[6][0] = 2;
    aliases.enemies[0].model = model(5);
    auto &part = aliases.enemies[1];
    aliases.enemy_callbacks[31] = EnemyCallbackKind::ZombieHead;
    part.id = 31;
    part.flags = 1;
    part.pose_owner = 0;
    part.modes[0] = 1;
    part.damage.fill(88);
    part.combo_timeout = 3;
    part.secondary_flags = 4;
    tick_scenario_actors(aliases);
    ck(part.damage[0] == 0 && part.damage[4] == 0 && part.damage[5] == 88 &&
           part.combo_timeout == 2 && part.secondary_flags == 0,
       "Aliased auxiliary cleanup did not use parent model object count");
    part.pose_owner.reset();
    part.damage.fill(88);
    tick_scenario_actors(aliases);
    ck(part.damage[0] == 88 && part.combo_timeout == 1,
       "Null ML_WORK cleanup incorrectly cleared damage");
    part.pose_owner = 0;
    aliases.enemies[0].model = model(65);
    bad([&] { tick_scenario_actors(aliases); });
    ck(part.damage[0] == 88 && part.combo_timeout == 1,
       "Over-capacity model partially performed cleanup");
    std::cout << "Native auxiliary and common enemy frame dispatch passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
