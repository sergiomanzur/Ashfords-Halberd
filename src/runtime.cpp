#include "runtime.hpp"
#include <algorithm>
#include <stdexcept>
namespace ashford {
void transition_room_runtime(RoomRuntime &runtime, const Bytes &destination,
                             const PlayerPackage &player) {
  if (!runtime.scenario.door || !(runtime.scenario.flags[10][0] & 1))
    throw std::runtime_error("No pending native room transition");
  const auto door = *runtime.scenario.door;
  auto replacement = load_room_runtime(destination, door.stage, door.room,
                                       door.position, runtime.scenario, player);
  // bhStartDoorDemo completes after the destination is ready. Door graphics
  // and sound have separate owners; this consumes only the loading request.
  replacement.scenario.flags[10][0] &= ~1u;
  replacement.scenario.flags[6][0] = 0xffffffff;
  runtime = std::move(replacement);
}
Scene compose_room_runtime(const RoomRuntime &runtime, uint32_t draw_flags,
                           uint32_t camera_flags) {
  Scene result;
  size_t triangles = 0;
  auto append = [&](const Scene &scene) {
    size_t offset = result.textures.size();
    if (scene.textures.size() > 65535 - offset ||
        result.meshes.size() + scene.meshes.size() > 16384)
      throw std::runtime_error("Runtime scene resource limit exceeded");
    result.textures.insert(result.textures.end(), scene.textures.begin(),
                           scene.textures.end());
    for (auto mesh : scene.meshes) {
      triangles += mesh.triangles.size();
      if (triangles > 2000000)
        throw std::runtime_error("Runtime scene triangle limit exceeded");
      for (auto &triangle : mesh.triangles)
        if (triangle.texture != 0xffff) {
          if (triangle.texture >= scene.textures.size())
            throw std::runtime_error(
                "Runtime triangle texture outside actor pack");
          triangle.texture = uint16_t(triangle.texture + offset);
        }
      result.meshes.push_back(std::move(mesh));
    }
  };
  auto posed = [&](const ScenarioEntity &entity) {
    if (entity.flags & 0x1080 || entity.model_flags & 6)
      throw std::runtime_error(
          "Unsupported runtime draw matrix or morphing mode");
    if (!entity.rendered_model)
      throw std::runtime_error("Missing prepared runtime entity pose");
    append(*entity.rendered_model);
  };
  auto cut_visible = [&](const ScenarioEntity &entity) {
    if (!(entity.flags & 0x40000000) || runtime.scenario.flags[11][0] & 0x40 ||
        camera_flags & 0x46)
      return true;
    if (!runtime.world.cut)
      return false;
    return std::any_of(entity.hide.begin(), entity.hide.end(), [&](int8_t cut) {
      return cut >= 0 && size_t(cut) == *runtime.world.cut;
    });
  };
  if (draw_flags & 0x20)
    append(runtime.scene);
  if (draw_flags & 2)
    for (const auto &entity : runtime.scenario.enemies)
      if (entity.flags & 1 && !(entity.flags & 0x80000000) &&
          !(entity.state_flags & 0x41000000) && !(entity.model_flags & 1) &&
          ((runtime.scenario.flags[5][0] & 0x20) ||
           !(entity.model_flags & 0x200)))
        posed(entity);
  for (const auto &entity : runtime.scenario.objects) {
    uint32_t gate = entity.id >= 1000 ? (entity.id < 1210 ? 2 : 1) : 4;
    if (draw_flags & 4 && draw_flags & gate && entity.flags & 1 &&
        !(entity.model_flags & 1) &&
        (!(entity.state_flags & 0x1000000) ||
         runtime.scenario.player_sleep_ticks) &&
        !((runtime.scenario.flags[11][0] & 0x4000) &&
          (entity.model_flags & 0x40)) &&
        cut_visible(entity))
      posed(entity);
  }
  if (draw_flags & 8)
    for (const auto &entity : runtime.scenario.items)
      if (entity.flags & 1 && !(entity.state_flags & 0x1000000) &&
          !(entity.model_flags & 1) && cut_visible(entity))
        posed(entity);
  if (draw_flags & 1)
    append(runtime.player_scene);
  return result;
}
void tick_room_runtime(RoomRuntime &runtime, const PlayerPackage &player,
                       const PlayerTranslationTables &translations,
                       uint16_t pad) {
  auto &scenario = runtime.scenario;
  uint16_t pressed = uint16_t(pad & ~runtime.previous_pad);
  runtime.previous_pad = pad;
  tick_scenario_actors(scenario);
  if (scenario.flags[6][0] & 1) {
    if (scenario.equipped_weapon != 0)
      throw std::runtime_error("Armed runtime requires its player controller");
    ActorContacts contacts;
    contacts.player.flags = scenario.player_work_flags;
    contacts.player.state_flags = scenario.player_state_flags;
    contacts.player.secondary_flags = scenario.player_secondary_flags;
    contacts.player.radius = 2.5f;
    contacts.player.height = scenario.player_id == 0 ? 15.5f : 16.5f;
    for (const auto &entity : scenario.enemies) {
      ContactBody body;
      body.position = entity.position;
      body.offset = entity.collision_offset;
      body.radius = entity.geometry.collision_radius;
      body.height = entity.geometry.collision_height;
      body.flags = entity.flags;
      body.state_flags = entity.state_flags;
      body.secondary_flags = entity.secondary_flags;
      body.linked_parent = entity.linked_parent;
      contacts.enemies.push_back(body);
    }
    runtime.world =
        advance_player_world(runtime.world, player, translations,
                             runtime.collision, runtime.cuts, pad, &contacts);
    for (size_t slot = 0; slot < scenario.enemies.size(); ++slot)
      scenario.enemies[slot].state_flags = contacts.enemies[slot].state_flags;
    runtime.player_pose = sample_player_locomotion_pose(
        player, runtime.world.player, runtime.player_pose,
        &scenario.motion_angle_scratch);
    runtime.player_scene =
        pose_actor(player.models.at(0), runtime.player_pose,
                   runtime.world.player.position, runtime.world.player.yaw);
    if ((pressed & 0x200) && !(scenario.flags[10][0] & 0x4017) &&
        !(scenario.player_state_flags & 0x10080))
      examine_room_interaction(runtime);
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
  result.player_pose = sample_player_locomotion_pose(
      player, result.world.player, {}, &persistent.motion_angle_scratch);
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
  persistent.drawing_flags = 0xffffffff;
  persistent.flags[10][0] &= 0xaf8000bb;
  persistent.door.reset();
  persistent.stage = stage;
  persistent.room = room;
  persistent.interaction_index = 0;
  persistent.floor_index = 0;
  persistent.player_frame = result.world.player.frame;
  persistent.player_state_flags &= 0x78280000;
  // player.c::bhInitRoomChangePlayer primary collision/movement flags.
  persistent.player_work_flags =
      (persistent.player_work_flags & 0x20119) | 0x118;
  persistent.collision = result.collision;
  persistent.dynamic_miscellaneous.clear();
  persistent.routes =
      std::make_shared<const RoomRoutes>(decode_room_routes(data));
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
