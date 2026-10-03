#pragma once
#include "actor.hpp"
#include "camera.hpp"
#include "collision.hpp"
#include "player.hpp"
namespace ashford {
// Healthy, unarmed baseline locomotion from player.c. Attack/damage, pushing,
// stairs and quick-turn states belong to the wider player controller.
struct PlayerLocomotion {
  Vec3 position, visual_anchor, stop_delta;
  uint16_t yaw = 0, previous_pad = 0;
  uint8_t mode = 0, phase = 0;
  bool entering = true, locked = false, moving = false,
       preserve_walk_phase = false;
  size_t motion = 42;
  int32_t frame = 0, sampled_frame = 0;
  uint16_t blend_rate = 0;
  uint8_t blend_count = 0;
  uint8_t sampled_blend_count = 0;
  uint32_t idle_ticks = 0;
  bool water = false;
};
// Healthy unarmed primary-model posing with the sampled frame/blend fields
// retained by advance_player_locomotion. Keep the returned pose between ticks.
// Player.c marks joints 7/11 for FastHokan's alternate angle interpolation.
// A runtime supplies its shared angle scratch; isolated probes may omit it.
std::vector<JointPose>
sample_player_locomotion_pose(const PlayerPackage &package,
                              const PlayerLocomotion &state,
                              const std::vector<JointPose> &previous = {},
                              std::array<uint16_t, 3> *angle_scratch = nullptr);
// Retains the retail legacy clamp to a raw new frame count. Confirmed against
// the user's USA ELF at bhGetFrameNum, not assumed from its partial decompile.
int32_t remap_player_frame(uint32_t old_count, uint32_t new_count,
                           int32_t frame);
PlayerLocomotion
advance_player_locomotion(PlayerLocomotion state, const PlayerPackage &package,
                          const PlayerTranslationTables &tables, uint16_t pad);
struct PlayerWorldState {
  PlayerLocomotion player;
  Vec3 front_anchor;
  float front_offset = 0;
  int8_t floor = 0;
  std::optional<size_t> cut;
  size_t wall_hits = 0;
};
// Normal baseline sequence: motion-linked displacement, smoothed front probe,
// two previous-position collision passes, floor effects and camera selection.
PlayerWorldState advance_player_world(PlayerWorldState state,
                                      const PlayerPackage &package,
                                      const PlayerTranslationTables &tables,
                                      const RoomCollision &room,
                                      const std::vector<RoomCut> &cuts,
                                      uint16_t pad);
} // namespace ashford
