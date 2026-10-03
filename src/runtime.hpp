#pragma once
#include "room.hpp"
#include "scenario.hpp"
namespace ashford {
struct RoomRuntime {
  Scene scene;
  RoomCollision collision;
  std::vector<RoomCut> cuts;
  PlayerWorldState world;
  std::vector<JointPose> player_pose;
  Scene player_scene;
  ScenarioState scenario;
  uint16_t previous_pad = 0;
};
// Healthy unarmed normal door entry; armed/save/introduction paths are pending.
// Builds a complete replacement without mutating the previous runtime.
RoomRuntime load_room_runtime(const Bytes &data, uint8_t stage, uint8_t room,
                              size_t entry, ScenarioState persistent,
                              const PlayerPackage &player);
// Represented baseline frame sequence. Unsupported entity controllers fail
// explicitly; effect/light/audio and interaction dispatch remain pending.
void tick_room_runtime(RoomRuntime &runtime, const PlayerPackage &player,
                       const PlayerTranslationTables &translations,
                       uint16_t pad);
// Merge prepared geometry into one depth-tested scene, preserving room hide
// indices and actor indices. Caller supplies original drawing/camera flags.
Scene compose_room_runtime(const RoomRuntime &runtime, uint32_t draw_flags,
                           uint32_t camera_flags);
// bhCheckExmAtari baseline: original probe/floor/facing selection, direct
// door requests, script-only examination and standing pickup requests.
// The pickup interface and room transition consume these requests separately.
bool examine_room_interaction(RoomRuntime &runtime);
} // namespace ashford
