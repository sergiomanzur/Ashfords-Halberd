#include "runtime.hpp"
#include "audio_runtime.hpp"
#include "effects.hpp"
#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ashford {
void synchronize_runtime_player(RoomRuntime &runtime,
                                const PlayerPackage &package) {
  auto &p = runtime.player_work;
  const auto &s = runtime.scenario;
  if (s.player_id >= 4 || s.equipped_weapon != 0)
    throw std::runtime_error("Unsupported runtime player work binding");
  const bool initial = !p.model;
  if (initial) {
    p.model = std::make_shared<const ActorModel>(package.models.at(0));
    p.base_motions = std::make_shared<const MotionBank>(package.motions);
    p.motions = p.base_motions;
    if (!p.player_work)
      p.player_work.emplace();
    p.player_work->status_flags = 1;
  }
  p.flags = s.player_work_flags;
  p.state_flags = s.player_state_flags;
  p.secondary_flags = s.player_secondary_flags;
  p.health = s.player_health;
  p.position = runtime.world.player.position;
  p.angles = {0, runtime.world.player.yaw, 0};
  p.floor = runtime.world.floor;
  p.modes = {1, 0, runtime.world.player.mode, runtime.world.player.phase};
  p.geometry.radius = 3.5f;
  p.geometry.height = s.player_id == 0 ? 16.5f : 17.5f;
  p.geometry.collision_radius = p.geometry.radius - 1;
  p.geometry.collision_height = p.geometry.height - 1;
  p.motion_number = uint32_t(runtime.world.player.motion);
  p.frame = runtime.world.player.frame;
  p.motion_step = 65536;
  p.blend_rate = runtime.world.player.blend_rate;
  p.blend_count = runtime.world.player.blend_count;
  p.joint_pose = runtime.player_pose;
  ActorPlacement placement;
  placement.flags = p.flags;
  placement.position = p.position;
  placement.angles = p.angles;
  auto cached = calculate_actor_model_work(
      *p.model, p.joint_pose, p.model_work.value_or(ActorModelWork{}),
      placement);
  if (cached.joints.size() <= 11)
    throw std::runtime_error("Runtime player lacks original cached joints");
  cached.joints[7].flags |= 8;
  cached.joints[11].flags |= 8;
  auto point = [&](size_t joint) {
    const auto &m = cached.joints[joint].matrix;
    return Vec3{detail::ninja_finite(float(m[3])),
                detail::ninja_finite(float(m[7])),
                detail::ninja_finite(float(m[11]))};
  };
  // Source bhControlPlayer updates watr at the end of a controlled frame;
  // bhInitRoomChangePlayer only calculates its initial model matrices.
  if (!initial) {
    p.geometry.capsule_start = point(4);
    p.geometry.capsule_end = point(0);
    p.geometry.capsule_radius = 2.5f;
  }
  p.model_matrix = cached.base_matrix;
  p.model_work = std::move(cached);
}
void transition_room_runtime(RoomRuntime &runtime, const Bytes &destination,
                             const PlayerPackage &player,
                             const RoomSoundResolver &sounds) {
  if (!runtime.scenario.door || !(runtime.scenario.flags[10][0] & 1))
    throw std::runtime_error("No pending native room transition");
  const auto door = *runtime.scenario.door;
  auto replacement =
      load_room_runtime(destination, door.stage, door.room, door.position,
                        runtime.scenario, player, &runtime.player_work, sounds);
  // bhStartDoorDemo completes after the destination is ready. Door graphics
  // and sound have separate owners; this consumes only the loading request.
  replacement.scenario.flags[10][0] &= ~1u;
  replacement.scenario.flags[6][0] = 0xffffffff;
  runtime = std::move(replacement);
}
Scene compose_room_runtime(const RoomRuntime &runtime, uint32_t draw_flags,
                           uint32_t camera_flags) {
  Scene result;
  if (runtime.scene.ambient && !runtime.scenario.ambient)
    throw std::runtime_error("Missing current runtime ambient work");
  result.ambient = runtime.scenario.ambient;
  result.fog = runtime.scenario.fog.active;
  if (runtime.scene.lighting && !runtime.scenario.lighting)
    throw std::runtime_error("Missing current native runtime lighting");
  result.lighting = runtime.scenario.lighting;
  auto current_light = runtime.scenario.drawing_lighting
                           ? runtime.scenario.drawing_lighting
                           : runtime.scenario.lighting;
  auto light_work = runtime.scenario.drawing_light_work.value_or(
      runtime.scenario.lights.value_or(RoomLights{}));
  const bool easy = runtime.scenario.flags[11][0] & 0x80;
  auto set_light = [&](bool half) {
    if (runtime.scenario.lighting)
      current_light =
          drawing_lights(light_work, *runtime.scenario.lighting, easy, half);
  };
  auto draw_mode = [&](int type) -> uint8_t {
    if (type < 0 || type > 3)
      throw std::runtime_error("Invalid native entity drawing type");
    if (easy && type < 2)
      type += 2;
    constexpr uint8_t table[4] = {1, 3, 0, 2};
    return table[type];
  };
  bool drawing_fog = runtime.scenario.fog.active.enabled;
  size_t triangles = 0;
  auto append = [&](const Scene &scene, AmbientTarget target,
                    bool model_clipping, bool fog_enabled = true,
                    uint8_t mode = 1) {
    size_t offset = result.textures.size();
    if (scene.textures.size() > 65535 - offset ||
        result.meshes.size() + scene.meshes.size() > 16384)
      throw std::runtime_error("Runtime scene resource limit exceeded");
    result.textures.insert(result.textures.end(), scene.textures.begin(),
                           scene.textures.end());
    for (auto mesh : scene.meshes) {
      mesh.ambient_target = target;
      mesh.model_clipping = model_clipping;
      mesh.fog_enabled = fog_enabled;
      mesh.fog_state = drawing_fog && fog_enabled;
      mesh.lighting = current_light;
      mesh.draw_mode = mode;
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
  auto posed = [&](const ScenarioEntity &entity, AmbientTarget target,
                   bool model_clipping, bool suppress_fog = false) {
    if (entity.flags & 0x1080 || entity.model_flags & 6)
      throw std::runtime_error(
          "Unsupported runtime draw matrix or morphing mode");
    if (!entity.rendered_model)
      throw std::runtime_error("Missing prepared runtime entity pose");
    const bool toggle_fog = suppress_fog &&
                            (runtime.scenario.flags[10][0] & 2) &&
                            (entity.model_flags & 0x10);
    if (toggle_fog)
      drawing_fog = false;
    append(*entity.rendered_model, target, model_clipping, !toggle_fog,
           draw_mode(entity.draw_type));
    if (toggle_fog)
      drawing_fog = true; // Original njFogEnable is unconditional here.
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
    append(runtime.scene, AmbientTarget::Room, true, true, easy ? 0 : 1);
  // bhDrawObjItm emits normal objects, half-light objects, special objects,
  // then items before the main enemy/player pass. Material state is shared.
  bool object_clipping = false; // njControl3D(0x2400) after room drawing.
  for (int pass = 0; pass < 3; ++pass) {
    if ((draw_flags & 4) && !(runtime.scenario.flags[10][0] & 0x20)) {
      if (pass == 0)
        set_light(false);
      if (pass == 1)
        set_light(true);
    }
    for (const auto &entity : runtime.scenario.objects) {
      const int group = entity.id >= 1000 ? 1
                        : entity.type == 5 && entity.angular_speed != 0 ? 2 : 0;
      uint32_t gate = entity.id >= 1000 ? (entity.id < 1210 ? 2 : 1) : 4;
      if (group == pass && draw_flags & 4 && draw_flags & gate &&
          entity.flags & 1 && !(entity.model_flags & 1) &&
          (!(entity.state_flags & 0x1000000) || runtime.scenario.player_sleep_ticks) &&
          !((runtime.scenario.flags[11][0] & 0x4000) && (entity.model_flags & 0x40)) &&
          cut_visible(entity)) {
        posed(entity,
              pass == 0 ? AmbientTarget::Object : AmbientTarget::Character,
              object_clipping || (pass != 2 && (entity.model_flags & 8)),
              pass != 2);
        if (pass != 2)
          object_clipping = true; // bhDrawObject restores njControl3D(0x100).
      }
    }
  }
  if (!(runtime.scenario.flags[10][0] & 0x20))
    set_light(false); // bhDrawObjItm restores even with item draw flag clear.
  if (draw_flags & 8)
    for (const auto &entity : runtime.scenario.items)
      if (entity.flags & 1 && !(entity.state_flags & 0x1000000) &&
          !(entity.model_flags & 1) && cut_visible(entity)) {
        if (current_light && runtime.scenario.lighting) {
          if (entity.model_flags & 0x80) {
            current_light->has_direction = true;
            current_light->direction.vector =
                light_direction({57344, entity.yaw_correction, 0});
            current_light->direction.color.fill(.1f * entity.angular_speed);
          } else {
            current_light->has_direction =
                runtime.scenario.lighting->has_direction;
            current_light->direction = runtime.scenario.lighting->direction;
          }
        }
        posed(entity, AmbientTarget::Item, true, true);
      }
  set_light(true); // game.c unconditionally changes light before enemy/player.
  if (draw_flags & 2)
    for (size_t work = 0; work < runtime.scenario.enemies.size(); ++work) {
      const auto &entity = runtime.scenario.enemies[work];
      if (entity.flags & 1 && !(entity.flags & 0x80000000) &&
          !(entity.state_flags & 0x41000000) && !(entity.model_flags & 1) &&
          ((runtime.scenario.flags[5][0] & 0x20) ||
           !(entity.model_flags & 0x200))) {
        if (entity.flags & 0x80) {
          if (!entity.linked_parent ||
              *entity.linked_parent >= runtime.scenario.enemies.size())
            throw std::runtime_error("Missing runtime enemy drawing parent");
          if (runtime.scenario.enemies[*entity.linked_parent].state_flags &
              0x1000000)
            continue;
        }
        if (entity.id == 1 && entity.zombie_work)
          append(draw_zombie_model(runtime.scenario, work),
                 AmbientTarget::Character, false, true,
                 draw_mode(entity.draw_type));
        else
          posed(entity, AmbientTarget::Character, false);
      }
    }
  if (draw_flags & 1)
    append(runtime.player_scene, AmbientTarget::Character, false, true,
           draw_mode(runtime.player_work.draw_type));
  result.ending_fog_state = drawing_fog;
  if (runtime.scenario.lights) {
    // The supported light event53 changes enable bit1 after this frame's
    // drawing boundary. Preserve it while publishing drawing clocks/flags.
    auto preserve_event_enable = [](auto &draw, const auto &live) {
      if (draw.size() != live.size())
        throw std::runtime_error(
            "Light event changed current room work extent");
      for (size_t i = 0; i < draw.size(); ++i)
        draw[i].flags = (draw[i].flags & ~1u) | (live[i].flags & 1u);
    };
    preserve_event_enable(light_work.normal, runtime.scenario.lights->normal);
    preserve_event_enable(light_work.events, runtime.scenario.lights->events);
    result.ending_light_work = std::move(light_work);
  }
  result.ending_lighting = current_light;
  return result;
}
namespace {
bool bind_current_fog(const RoomRuntime &runtime, FogWork &fog,
                      FogParameters &room, uint32_t st, uint32_t &gm,
                      uint32_t &effects) {
  const bool eye = bool(gm & 0x40);
  const CameraDefinition *definition = nullptr;
  if (!eye) {
    if (!runtime.world.cut)
      return false;
    const size_t cut = *runtime.world.cut;
    if (cut >= runtime.cuts.size())
      throw std::runtime_error("Current fog cut outside room");
    definition = &runtime.cuts[cut].variants[0];
    // cut.c::bhControlActiveCamera recomputes these ranges from the camera
    // each frame. Authored values alone cannot bind that source behavior.
    if (definition->flags & 0xc0)
      throw std::runtime_error("Unsupported dynamic camera fog binding");
  }
  if (!runtime.fog_bound || runtime.fog_eye != eye ||
      (!eye && runtime.fog_cut != runtime.world.cut)) {
    if (eye)
      select_fog_eye(fog, room, st, gm);
    else {
      select_fog_cut(fog, definition->fog, bool(definition->flags & 8), st, gm,
                     effects);
    }
  }
  return true;
}
void publish_fog_binding(RoomRuntime &runtime, FogWork fog, FogParameters room,
                         uint32_t st, uint32_t gm, uint32_t effects) {
  runtime.scenario.fog = std::move(fog);
  runtime.room_fog = room;
  runtime.scenario.flags[10][0] = st;
  runtime.scenario.flags[11][0] = gm;
  runtime.scenario.effect_flags = effects;
  runtime.fog_cut = runtime.world.cut;
  runtime.fog_bound = true;
  runtime.fog_eye = bool(gm & 0x40);
}
void bind_entry_fog(RoomRuntime &runtime) {
  auto fog = runtime.scenario.fog;
  auto room = runtime.room_fog;
  auto st = runtime.scenario.flags[10][0], gm = runtime.scenario.flags[11][0];
  auto effects = runtime.scenario.effect_flags;
  if (bind_current_fog(runtime, fog, room, st, gm, effects))
    publish_fog_binding(runtime, std::move(fog), room, st, gm, effects);
}
} // namespace
void control_room_fog(RoomRuntime &runtime) {
  auto fog = runtime.scenario.fog;
  auto room = runtime.room_fog;
  auto st = runtime.scenario.flags[10][0], gm = runtime.scenario.flags[11][0];
  auto effects = runtime.scenario.effect_flags;
  if (!bind_current_fog(runtime, fog, room, st, gm, effects))
    return;
  control_fog(fog, st, gm);
  publish_fog_binding(runtime, std::move(fog), room, st, gm, effects);
}
void control_room_lights(RoomRuntime &runtime) {
  if (!runtime.scenario.lights) {
    if (runtime.scene.lighting)
      throw std::runtime_error("Missing native room light work");
    return;
  }
  auto work = *runtime.scenario.lights;
  Camera camera;
  CameraDefinition definition;
  if (runtime.world.cut) {
    if (*runtime.world.cut >= runtime.cuts.size())
      throw std::runtime_error("Current light cut outside room");
    definition = runtime.cuts[*runtime.world.cut].variants[0];
    camera = resolve_camera(runtime.cuts[*runtime.world.cut], 0);
    if (!runtime.light_bound || runtime.light_cut != runtime.world.cut)
      select_light_cut(work, definition);
  } else if (!work.normal.empty() || !work.events.empty())
    throw std::runtime_error(
        "Native room lights require current camera binding");
  auto lighting = control_lights(work, runtime.scenario, &runtime.player_work,
                                 camera, definition, runtime.camera_flags);
  runtime.scenario.lights = std::move(work);
  runtime.scenario.drawing_light_work = runtime.scenario.lights;
  runtime.scenario.lighting = lighting;
  runtime.scenario.drawing_lighting = lighting;
  runtime.light_cut = runtime.world.cut;
  runtime.light_bound = true;
}
void tick_room_runtime(RoomRuntime &runtime, const PlayerPackage &player,
                       const PlayerTranslationTables &translations,
                       uint16_t pad) {
  auto &scenario = runtime.scenario;
  if ((scenario.flags[6][0] & 1) && !scenario.collision)
    throw std::runtime_error("Missing current runtime collision work");
  uint16_t pressed = uint16_t(pad & ~runtime.previous_pad);
  runtime.previous_pad = pad;
  scenario.input_held = pad;
  scenario.input_pressed = pressed;
  auto &work = runtime.player_work;
  work.flags = scenario.player_work_flags;
  work.state_flags = scenario.player_state_flags;
  work.secondary_flags = scenario.player_secondary_flags;
  work.health = scenario.player_health;
  work.collision_offset.x = work.collision_offset.z = 0;
  tick_scenario_actors(scenario, &work);
  scenario.player_work_flags = work.flags;
  scenario.player_state_flags = work.state_flags;
  scenario.player_secondary_flags = work.secondary_flags;
  scenario.player_health = work.health;
  runtime.world.player.position = work.position;
  runtime.world.player.yaw = uint16_t(work.angles[1]);
  if (scenario.flags[6][0] & 1) {
    if (scenario.equipped_weapon != 0)
      throw std::runtime_error("Armed runtime requires its player controller");
    if (work.modes[0] != 1)
      throw std::runtime_error("Unimplemented runtime player control mode " +
                               std::to_string(work.modes[0]));
    if (!work.player_work || !work.model_work ||
        work.model_work->joints.empty())
      throw std::runtime_error("Missing retained runtime player work/cache");
    work.previous_position = work.position;
    work.previous_angles = work.angles;
    const auto &base = work.model_work->joints[0].matrix;
    const Vec3 normal{detail::ninja_finite(float(base[3])), work.position.y,
                      detail::ninja_finite(float(base[11]))};
    const auto &anchor = work.player_work->normal_anchor;
    const float x = detail::ninja_finite(normal.x - anchor.x),
                y = detail::ninja_finite(normal.y - anchor.y),
                z = detail::ninja_finite(normal.z - anchor.z);
    if (std::sqrt(detail::ninja_finite((x * x + y * y) + z * z)) > .3f)
      work.player_work->normal_anchor = normal;
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
                             *scenario.collision, runtime.cuts, pad, &contacts);
    for (size_t slot = 0; slot < scenario.enemies.size(); ++slot)
      scenario.enemies[slot].state_flags = contacts.enemies[slot].state_flags;
    scenario.player_state_flags = contacts.player.state_flags;
    runtime.player_pose = sample_player_locomotion_pose(
        player, runtime.world.player, runtime.player_pose,
        &scenario.motion_angle_scratch);
    runtime.player_scene =
        pose_actor(player.models.at(0), runtime.player_pose,
                   runtime.world.player.position, runtime.world.player.yaw);
    synchronize_runtime_player(runtime, player);
    if ((pressed & 0x200) && !(scenario.flags[10][0] & 0x4017) &&
        !(scenario.player_state_flags & 0x10080))
      examine_room_interaction(runtime);
  }
  scenario.player_frame = runtime.world.player.frame;
  tick_shadow_effects(scenario);
  tick_wall_damage_timers(scenario);
  tick_scenario_props(scenario);
  control_room_fog(runtime);
  control_room_lights(runtime);
  control_scenario(scenario);
}
RoomRuntime load_room_runtime(const Bytes &data, uint8_t stage, uint8_t room,
                              size_t entry, ScenarioState persistent,
                              const PlayerPackage &player,
                              const ScenarioEntity *previous_player,
                              const RoomSoundResolver &sounds) {
  RoomRuntime result;
  if (persistent.flags[10][0] & 0x800000)
    throw std::runtime_error(
        "Save restoration requires a separate room entry path");
  auto records = decode_room_records(data);
  result.scene = decode_room_scene(data);
  result.collision = decode_room_collision(data);
  result.cuts = decode_room_cuts(data);
  result.room_fog = decode_room_fog(data);
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
  persistent.ambient = result.scene.ambient;
  persistent.lights = decode_room_lights(data);
  initialize_room_lights(*persistent.lights);
  persistent.drawing_light_work.reset();
  // Light work clocks start in bhMainSequence after camera control. Entry
  // must not consume a dynamic light's first RNG/counter update early.
  persistent.lighting = NativeLighting{};
  persistent.drawing_lighting = persistent.lighting;
  persistent.dynamic_miscellaneous.clear();
  persistent.dynamic_walls.clear();
  persistent.dynamic_floors.clear();
  if (persistent.effect_pool) {
    // bhClearEffect zeros eff[], retaining sys->ef. Old room indices expire.
    auto cleared = std::make_shared<NativeEffectPool>();
    cleared->scratch = persistent.effect_pool->scratch;
    persistent.effect_pool = std::move(cleared);
  }
  persistent.pending_enemy_sounds.clear();
  persistent.pending_enemy_vibrations.clear();
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
  result.scenario = std::move(persistent);
  if (previous_player) {
    result.player_work.player_work = previous_player->player_work;
    result.player_work.geometry = previous_player->geometry;
  }
  synchronize_runtime_player(result, player);
  bind_room_effects(data, result.scenario);
  bind_entry_fog(result); // room.c::bhCheckCut precedes system.c::bhInitEvent.
  if (result.world.cut)
    select_light_cut(*result.scenario.lights,
                     result.cuts.at(*result.world.cut).variants[0]);
  result.light_cut = result.world.cut;
  result.light_bound = true;
  initialize_scenario(result.scenario, &result.player_work);
  control_room_fog(result);
  if (sounds) {
    result.sound_assets = sounds(stage, room, result.scenario.room_sound_case);
    result.scenario.room_sound_case =
        0; // GetRoomSoundCaseNo consumes source case.
    background_sound_command(result.scenario.background_audio, 1);
    RoomAudioPlayback validation(result.sound_assets);
    validation.apply(result.scenario.background_audio.commands);
  } else {
    const auto &audio = result.scenario.background_audio;
    if (audio.request[0] || audio.request[1] || !audio.commands.empty())
      throw std::runtime_error(
          "Native room entry requires its sound asset resolver");
  }
  return result;
}
} // namespace ashford
