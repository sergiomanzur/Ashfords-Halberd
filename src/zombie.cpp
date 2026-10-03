// Native startup parameters from MIT recvx-decomp en01.c.
#include "zombie.hpp"
#include <stdexcept>
namespace ashford {
namespace {
constexpr ZombiePersonality personalities[] = {
    {30, 70, 14563, 0, 0},   {30, 50, 10922, 0, 5},   {30, 70, 14563, 10, 10},
    {50, 100, 14563, 10, 5}, {50, 90, 10922, 10, 8},  {40, 80, 10922, 40, 10},
    {50, 100, 14563, 10, 5}, {50, 90, 10922, 10, 8},  {40, 80, 10922, 40, 10},
    {40, 80, 10922, 20, 10}, {30, 70, 14563, 0, 0},   {30, 50, 10922, 0, 0},
    {30, 70, 14563, 30, 5},  {30, 50, 10922, 30, 5},  {30, 70, 14563, 0, 5},
    {40, 90, 14563, 20, 10}, {40, 80, 10922, 20, 10}, {40, 90, 14563, 40, 10},
    {40, 80, 10922, 50, 10}, {20, 50, 14563, -20, 0}, {20, 50, 10922, -30, 0},
    {30, 70, 14563, 0, 0},   {20, 50, 10922, 30, 0},  {30, 70, 14563, 50, 10},
    {30, 70, 14563, 0, 10},  {30, 70, 14563, 20, 10}, {20, 50, 10922, 30, 0}};
bool valid_choice(int32_t choice) {
  return choice >= 0 && choice <= 4 && !(choice & 1);
}
} // namespace
uint8_t zombie_personality_index(uint8_t model_version) {
  static constexpr uint8_t lookup[][2] = {
      {0, 0},   {1, 1},   {31, 2},  {18, 0},  {2, 3},   {3, 4},   {4, 5},
      {32, 6},  {33, 7},  {34, 8},  {21, 9},  {19, 3},  {5, 10},  {6, 11},
      {35, 12}, {36, 13}, {22, 14}, {20, 10}, {7, 15},  {8, 16},  {37, 17},
      {38, 18}, {24, 15}, {9, 19},  {10, 20}, {25, 21}, {39, 22}, {40, 23},
      {41, 24}, {42, 25}, {43, 26}};
  // bhEne01_Init iterates only the first 30 of its 31 table rows.
  for (size_t i = 0; i < 30; ++i)
    if (lookup[i][0] == model_version)
      return lookup[i][1];
  return 0;
}
const ZombiePersonality &zombie_personality(uint8_t model_version) {
  return personalities[zombie_personality_index(model_version)];
}
int32_t roll_zombie_health(ScenarioState &state, uint8_t model_version) {
  static constexpr int32_t health[2][16] = {
      {60, 60, 60, 60, 60, 90, 90, 90, 90, 90, 120, 120, 120, 120, 120, 120},
      {20, 20, 40, 40, 40, 40, 40, 40, 40, 40, 60, 60, 60, 60, 60, 60}};
  auto base = health[state.game_mode == 2 ? 1 : 0][scenario_random(state) % 16];
  return base + (base * zombie_personality(model_version).health_percent) / 100;
}
int32_t select_zombie_walk(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size() ||
      state.enemies[work].id != 1 || !(state.enemies[work].flags & 1) ||
      (state.enemies[work].flags & 0x80))
    throw std::runtime_error("Invalid unlinked zombie walking context");
  const ScenarioEntity *previous = nullptr;
  for (const auto &enemy : state.enemies)
    if (enemy.id == 1 && (enemy.flags & 1) && !(enemy.flags & 0x80)) {
      if (&enemy == &state.enemies[work])
        break;
      previous = &enemy;
    }
  int32_t choice;
  if (previous) {
    if (!previous->zombie_walk_choice ||
        !valid_choice(*previous->zombie_walk_choice))
      throw std::runtime_error("Earlier zombie walking work is uninitialized");
    choice = (*previous->zombie_walk_choice + 2) % 6;
  } else
    choice = int32_t(scenario_random(state) % 3) * 2;
  state.enemies[work].zombie_walk_choice = choice;
  return choice;
}
uint32_t zombie_walk_motion(int32_t choice, bool linked_body) {
  if (!valid_choice(choice))
    throw std::runtime_error("Zombie walking choice outside original pairs");
  static constexpr uint32_t motions[] = {0, 200, 40, 240, 41, 241};
  return motions[choice + (linked_body ? 1 : 0)];
}
void initialize_zombie_type(ScenarioState &state, size_t work) {
  if (state.enemies.size() != 128 || work >= state.enemies.size())
    throw std::runtime_error("Invalid zombie type work pool");
  auto &zombie = state.enemies[work];
  if (zombie.id != 1 || !(zombie.flags & 1) || zombie.type >= 16)
    throw std::runtime_error("Zombie type outside original initializer table");
  const bool linked = zombie.flags & 0x80;
  const ScenarioEntity *parent = nullptr;
  if (linked) {
    if (!zombie.linked_parent ||
        *zombie.linked_parent >= state.enemies.size() ||
        *zombie.linked_parent == work ||
        !(state.enemies[*zombie.linked_parent].flags & 1))
      throw std::runtime_error(
          "Linked zombie type requires active parent work");
    parent = &state.enemies[*zombie.linked_parent];
  }
  const auto type = zombie.type;
  const bool half =
      type == 2 || type == 3 || type == 6 || type == 7 || type == 9;
  // Validate/read the preceding walking work before mutating other fields.
  if (!linked && type != 2 && type != 3 && type != 9)
    select_zombie_walk(state, work);
  zombie.modes = {1, 0, uint8_t(type == 6 || type == 7 || type == 15 ? 4 : 0),
                  0};
  if (half) {
    zombie.health = linked ? parent->health : zombie.health / 2;
    zombie.zombie_flags &= ~0xfu;
    if (!linked)
      zombie.zombie_flags |= 1;
    zombie.motion_number = linked ? 203 : 3;
  } else
    zombie.motion_number = linked ? 202 : 2;
  if (type == 10) {
    zombie.flags &= ~0x20u;
    zombie.zombie_flags &= ~0x10000000u;
  }
  if (type == 11 || type == 15)
    zombie.zombie_secondary_flags |= 0x10;
  if (type == 15) {
    zombie.motion_number = linked ? 213 : 13;
    zombie.motion_step = 0;
    if (!linked)
      zombie.zombie_flags = (zombie.zombie_flags & ~0xfu) | 0x40001;
  }
}
} // namespace ashford
