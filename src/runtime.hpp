#pragma once
#include "audio_room.hpp"
#include "room.hpp"
#include "scenario.hpp"
#include <functional>
namespace ashford {
using RoomSoundResolver = std::function<std::shared_ptr<const RoomSoundAssets>(
    uint8_t, uint8_t, int32_t)>;
struct RoomRuntime {
  Scene scene;
  RoomCollision collision;
  std::vector<RoomCut> cuts;
  PlayerWorldState world;
  std::vector<JointPose> player_pose;
  Scene player_scene;
  ScenarioEntity player_work;
  ScenarioState scenario;
  std::shared_ptr<const RoomSoundAssets> sound_assets;
  uint16_t previous_pad = 0;
};
// Retained healthy unarmed work/caches consumed by native enemy callbacks.
// Model/motion resources are bound once; normal-anchor timing is caller-owned.
void synchronize_runtime_player(RoomRuntime &runtime,
                                const PlayerPackage &player);
// Healthy unarmed normal door entry; armed/save/introduction paths are pending.
// Builds a complete replacement without mutating the previous runtime.
RoomRuntime load_room_runtime(const Bytes &data, uint8_t stage, uint8_t room,
                              size_t entry, ScenarioState persistent,
                              const PlayerPackage &player,
                              const ScenarioEntity *previous_player = nullptr,
                              const RoomSoundResolver &sounds = {});
// Consume an authored door request after destination assets validate. Failure
// retains the old runtime, progress and pending request for recovery.
void transition_room_runtime(RoomRuntime &runtime, const Bytes &destination,
                             const PlayerPackage &player,
                             const RoomSoundResolver &sounds = {});
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
