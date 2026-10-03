#pragma once
#include "scenario.hpp"
#include <functional>
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
// Original Brain00 pursuit and KaidanCheck reservation policy. The caller
// supplies current player work; movement/attack dispatch remains separate.
void tick_zombie_brain(ScenarioState &state, size_t work,
                       const ScenarioEntity &player);
int change_enemy_motion(ScenarioEntity &enemy, uint32_t number, int32_t frame,
                        int32_t rate);
std::optional<int32_t> choose_zombie_sound(const ScenarioEntity &enemy,
                                           uint32_t &seed, int32_t code,
                                           bool already_playing);
// Current slot's active sound codes. ChechPlayEnemySe suppresses positive
// queries whenever any sound occupies that slot; other queries match the code.
std::optional<int32_t> choose_zombie_sound(const ScenarioEntity &enemy,
                                           uint32_t &seed, int32_t code,
                                           const std::vector<int32_t> &playing);
void tick_zombie_motion_table(ScenarioState &state, size_t work, int32_t frame,
                              const std::vector<int32_t> &playing = {});
// Synchronous source effect dependencies. Hooks append requests/update RNG;
// backend storage must stage its own changes if it can fail. Required for
// mode0<5; a caller must not replace needed source effects with a silent no-op.
struct ZombieMotionEffects {
  std::function<void(ScenarioState &, size_t, int32_t)> kami, water;
};
int tick_zombie_motion(ScenarioState &state, size_t work,
                       const ZombieMotionEffects &effects = {},
                       const std::vector<int32_t> &playing = {});
void tick_zombie_idle(ScenarioState &state, size_t work);
// en01.c::MV01 walking state callback; full Move/SetMtn dispatch is separate.
void tick_zombie_walk(ScenarioState &state, size_t work,
                      const ScenarioEntity &player);
void tick_zombie_neck(ScenarioState &state, size_t work,
                      const ActorModelWork &player);
} // namespace ashford
