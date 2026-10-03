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
const ZombiePersonality &zombie_personality_at(uint8_t index);
// Startup health roll, before type-specific or linked-body health adjustments.
int32_t roll_zombie_health(ScenarioState &state, uint8_t model_version);
// bhEne01_GetWalkMotion for unlinked root work. Stores the chosen pair index;
// earlier roots must already have initialized their own extended work.
int32_t select_zombie_walk(ScenarioState &state, size_t work);
uint32_t zombie_walk_motion(int32_t choice, bool linked_body);
// All 16 bhEne01_InitType table entries. Caller performs the surrounding
// work allocation, health roll, cloned-body setup, pose and collision init.
void initialize_zombie_type(ScenarioState &state, size_t work);
// Main Init state/linked allocation/motion/capsule path for the selected
// graveyard models. Startup shadow requests are retained as native metadata.
// Movement, damage, detached drawing and the wider scheduler remain separate.
// On success this transactional startup replaces state; reacquire references.
void initialize_zombie_work(ScenarioState &state, size_t work);
// Apply this work's motion to its ML_WORK owner, retaining separate counters.
// This transactional diagnostic helper also replaces state on success.
void advance_zombie_motion(ScenarioState &state, size_t work);
// Original head/arm/leg and hidden parent-manager mode-0 startup. Does not
// dispatch detached-part damage.
void initialize_zombie_auxiliary(ScenarioState &state, size_t work);
// en01sub.c source mode0/1 callbacks for head/arm/leg and parent manager.
// Root AI updates the cached player distance consumed by manager policy.
void tick_zombie_auxiliary(ScenarioState &state, size_t work);
// en01.c::bhEne01_EneSearch. Uses cached neck joint11 and retained EXP0
// personality, preserving packed byte counters and retail unsigned timeout.
void tick_zombie_awareness(ScenarioState &state, size_t work, Vec3 player);
} // namespace ashford
