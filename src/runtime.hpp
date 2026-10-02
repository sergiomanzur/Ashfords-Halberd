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
};
// Healthy unarmed normal door entry; armed/save/introduction paths are pending.
// Builds a complete replacement without mutating the previous runtime.
RoomRuntime load_room_runtime(const Bytes &data, uint8_t stage, uint8_t room,
                              size_t entry, ScenarioState persistent,
                              const PlayerPackage &player);
} // namespace ashford
