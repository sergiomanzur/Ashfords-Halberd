#pragma once
#include "scenario.hpp"
#include <functional>
namespace ashford {
// Original bhEne01 frame sequence. Unimplemented ordinary states fail explicitly.
void tick_zombie_controller(ScenarioState &state, size_t work,
                            ScenarioEntity &player);
// en01.c PlayerLink's authored XZ trajectory for four characters/clips1/2/4/5.
// Frame is an integer authored frame index; returned Y is zero.
Vec3 zombie_player_link_offset(uint8_t player_id, uint32_t motion_number,
                              int32_t frame);
// Complete en01.c PlayerLink anchor/offset/per-work matrix and Wall2 response.
void link_zombie_player(ScenarioState &state, size_t root, ScenarioEntity &player);
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
// en01.c::CalcEnemy: refresh source model caches/capsules/heights only.
// Drawing follows separately; an ordinary root consumes its prior cache.
void calculate_zombie_model(ScenarioState &state, size_t work);
// bhDrawEnemy's cached pose/morph path. Caller owns visibility/lighting gates.
// Does not recalculate matrices, advance motion, or mutate retained assets.
Scene draw_zombie_model(const ScenarioState &state, size_t work);
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
// Synchronous source effect dependencies. Native birth stages an immutable
// pool; SetMtn restores it and damage fields on failure. External backend
// storage must stage its own changes if it can fail. Required for mode0<5; a
// caller must not replace needed source effects with a silent no-op.
struct ZombieMotionEffects {
  std::function<void(ScenarioState &, size_t, int32_t)> kami, water;
};
void tick_zombie_kami_effect(ScenarioState &state, size_t work, int32_t frame);
void tick_zombie_water_effect(ScenarioState &state, size_t work, int32_t frame);
// Complete source NeckBloodEffect/ExpHeadEffect births used by NG02.
// Particle updates and drawing remain separate source callbacks.
void tick_zombie_neck_blood_effect(ScenarioState &state, size_t work, int type);
void tick_zombie_head_fragment_effect(ScenarioState &state, size_t work,
                                      int type);
ZombieMotionEffects native_zombie_motion_effects();
int tick_zombie_motion(ScenarioState &state, size_t work,
                       const ZombieMotionEffects &effects = {},
                       const std::vector<int32_t> &playing = {});
void tick_zombie_idle(ScenarioState &state, size_t work);
// en01.c::MV01 walking state callback; full Move/SetMtn dispatch is separate.
void tick_zombie_walk(ScenarioState &state, size_t work,
                      const ScenarioEntity &player);
// en01.c MV02 close pursuit and its paired walking-transition policy.
void tick_zombie_close_pursuit(ScenarioState &state, size_t work,
                               const ScenarioEntity &player);
void change_zombie_walk_motion(ScenarioState &state, size_t work);
bool zombie_fast_walk_active(const ScenarioState &state, size_t work);
// Original EatCheck modes0/1/2 and MV05 lunge into synchronized grab state.
// Player work includes its source cached joints and capsule, not render bounds.
bool zombie_capture_check(const ScenarioState &state, size_t work,
                          const ScenarioEntity &player, int32_t cone,
                          float distance, int mode);
void tick_zombie_lunge(ScenarioState &state, size_t work,
                       ScenarioEntity &player,
                       const std::vector<int32_t> &playing = {});
// Complete en01.c::MV06 capture retry and MV07 authored turn callbacks.
void tick_zombie_capture_retry(ScenarioState &state, size_t work,
                               ScenarioEntity &player);
void tick_zombie_turn(ScenarioState &state, size_t work,
                      const ScenarioEntity &player);
// Complete en01.c::MV08 reserved stair traversal and walking handoff.
void tick_zombie_stairs(ScenarioState &state, size_t work,
                        const ScenarioEntity &player);
// Complete en01.c frame contact/push/posture wall/floor/water collision.
void tick_zombie_collision(ScenarioState &state, size_t work,
                           ScenarioEntity &player);
// PlayerControl: mode2=0 PlyDG00 states0..8, mode2=1 PlyDG01 states0..5.
// Clips share the enemy bank; escape restores the player's retained base bank.
void tick_zombie_player_grab(ScenarioState &state, size_t work,
                             ScenarioEntity &player,
                             const std::vector<int32_t> &playing = {});
// Direct complete PlyDG01 standing-grab callback. Release samples base42.
void tick_zombie_player_standing_grab(ScenarioState &state, size_t work,
                                      ScenarioEntity &player);
void push_zombie_grab_neighbors(ScenarioState &state, size_t work,
                                const ScenarioEntity &player, int mode);
// Complete en01.c::NG00 held/lunge grab damage and wall/cliff release.
// Caller runs player control, motion sampling and cached model updates after.
void tick_zombie_grab(ScenarioState &state, size_t work, ScenarioEntity &player,
                      const std::vector<int32_t> &playing = {});
// Complete en01.c::NG01 failed-grab recoil, wall/cliff recovery and paired
// stop.
void tick_zombie_failed_grab(ScenarioState &state, size_t work,
                             const std::vector<int32_t> &playing = {});
// Complete en01.c::NG02 standing grab, damage, authored head effects and death.
void tick_zombie_standing_grab(ScenarioState &state, size_t work,
                               ScenarioEntity &player,
                               const std::vector<int32_t> &playing = {});
void tick_zombie_neck(ScenarioState &state, size_t work,
                      const ActorModelWork &player);
} // namespace ashford
