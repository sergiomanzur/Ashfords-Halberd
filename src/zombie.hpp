#pragma once
#include "scenario.hpp"
namespace ashford {
struct ZombiePersonality {
  float attack_distance = 0, notice_distance = 0;
  int32_t view_angle = 0, health_percent = 0, attack_bonus = 0;
};
// en01.c initialization lookup preserves its original 30-entry loop bound.
uint8_t zombie_personality_index(uint8_t model_version);
const ZombiePersonality &zombie_personality(uint8_t model_version);
// Startup health roll, before type-specific or linked-body health adjustments.
int32_t roll_zombie_health(ScenarioState &state, uint8_t model_version);
// bhEne01_GetWalkMotion for unlinked root work. Stores the chosen pair index;
// earlier roots must already have initialized their own extended work.
int32_t select_zombie_walk(ScenarioState &state, size_t work);
uint32_t zombie_walk_motion(int32_t choice, bool linked_body);
// All 16 bhEne01_InitType table entries. Caller performs the surrounding
// work allocation, health roll, cloned-body setup, pose and collision init.
void initialize_zombie_type(ScenarioState &state, size_t work);
} // namespace ashford
