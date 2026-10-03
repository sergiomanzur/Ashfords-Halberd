#include "runtime.hpp"
#include <stdexcept>
namespace ashford {
void tick_room_runtime(RoomRuntime &runtime, const PlayerPackage &player,
                       const PlayerTranslationTables &translations,
                       uint16_t pad) {
  auto &scenario = runtime.scenario;
  tick_scenario_actors(scenario);
  if (scenario.flags[6][0] & 1) {
    if (scenario.equipped_weapon != 0)
      throw std::runtime_error("Armed runtime requires its player controller");
    runtime.world = advance_player_world(runtime.world, player, translations,
                                         runtime.collision, runtime.cuts, pad);
    runtime.player_pose = sample_player_locomotion_pose(
        player, runtime.world.player, runtime.player_pose);
    runtime.player_scene =
        pose_actor(player.models.at(0), runtime.player_pose,
                   runtime.world.player.position, runtime.world.player.yaw);
  }
  scenario.player_frame = runtime.world.player.frame;
  tick_scenario_props(scenario);
  control_scenario(scenario);
}
RoomRuntime load_room_runtime(const Bytes &data, uint8_t stage, uint8_t room,
                              size_t entry, ScenarioState persistent,
                              const PlayerPackage &player) {
  RoomRuntime result;
  if (persistent.flags[10][0] & 0x800000)
    throw std::runtime_error(
        "Save restoration requires a separate room entry path");
  auto records = decode_room_records(data);
  result.scene = decode_room_scene(data);
  result.collision = decode_room_collision(data);
  result.cuts = decode_room_cuts(data);
  result.world = enter_room(records, entry, result.collision, result.cuts);
  // Healthy unarmed entry: bhInitRoomChangePlayer samples the initial pose
  // before advancing the motion counter visible to initialization scripts.
  if (persistent.equipped_weapon != 0)
    throw std::runtime_error("Armed room entry requires its player controller");
  result.player_pose =
      sample_player_locomotion_pose(player, result.world.player);
  result.player_scene =
      pose_actor(player.models.at(0), result.player_pose,
                 result.world.player.position, result.world.player.yaw);
  uint32_t direction = 0;
  advance_motion_frame(player.motions.at(result.world.player.motion)->frames,
                       65536, 0, result.world.player.frame, direction);
  // Represented bhInitRoomChangeSystem fields. Renderer/audio/effect state
  // has separate owners; persistent item/map/event flag banks are retained.
  persistent.flags[11][0] = (persistent.flags[11][0] | 2u) & 0x9b8c00cb;
  persistent.flags[5][0] = 0;
  persistent.flags[4][0] = 0;
  persistent.flags[10][0] &= 0xaf8000bb;
  persistent.door.reset();
  persistent.stage = stage;
  persistent.room = room;
  persistent.interaction_index = 0;
  persistent.floor_index = 0;
  persistent.player_frame = result.world.player.frame;
  persistent.collision = result.collision;
  persistent.interactions = result.collision.miscellaneous;
  bind_room_enemies(data, result.collision, persistent);
  bind_room_props(data, persistent);
  persistent.room_motions =
      std::make_shared<const MotionBank>(decode_room_motion_bank(data));
  persistent.scenario =
      std::make_shared<const RoomScenario>(decode_room_scenario(data));
  persistent.flags[11][0] &= ~9u; // bhFinishRoom clears previous game gates.
  // bhFinishRoom enables all subsystem gates before bhInitEvent.
  persistent.flags[6][0] = 0xffffffff;
  persistent.flags[11][0] |= 0x800;
  persistent.flags[12][0] |= 0x180;
  initialize_scenario(persistent);
  result.scenario = std::move(persistent);
  return result;
}
} // namespace ashford
