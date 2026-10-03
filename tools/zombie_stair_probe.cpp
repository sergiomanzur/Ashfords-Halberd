#include "zombie_stair_probe.hpp"
#include "enemy_navigation.hpp"
#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace ashford::diagnostic {
void collect_stair_probe_sources(std::vector<StairProbeSource> &sources,
                                 const RoomCollision &room, size_t room_id) {
  for (size_t i = 0; i < room.miscellaneous.size(); ++i) {
    const auto &r = room.miscellaneous[i];
    if (!(r.flags & 1) || r.type != 1 || (r.attribute & 0x400000) ||
        r.parameters[1] >= 4 || r.parameters[2] < 4 || r.parameters[2] % 2 ||
        r.floor < -2 || r.floor > 28)
      continue;
    bool have_direction = false;
    for (const auto &source : sources)
      if ((source.collision.miscellaneous[source.index].parameters[0] == 0) ==
          (r.parameters[0] == 0))
        have_direction = true;
    if (have_direction)
      continue;
    ScenarioState validation;
    validation.enemies.resize(128);
    validation.collision = room;
    reserve_actor_stairs(validation, 0, i);
    release_actor_stairs(validation, 0);
    sources.push_back({room, i, room_id});
  }
}
void run_zombie_stair_probes(const std::vector<StairProbeSource> &sources,
                             const std::vector<StairProbeActors> &actors) {
  if (sources.size() != 2 || actors.empty())
    throw std::runtime_error("Missing original uphill/downhill stair probes");
  size_t roots = 0, paths = 0, ticks = 0, samples = 0;
  const auto effects = native_zombie_motion_effects();
  for (const auto &context : actors)
    for (size_t root : context.roots) {
      ++roots;
      for (const auto &source : sources) {
        auto s = context.state;
        s.collision = source.collision;
        s.dynamic_miscellaneous.clear();
        s.dynamic_floors.clear();
        s.dynamic_walls.clear();
        auto &e = s.enemies[root];
        const auto &r = source.collision.miscellaneous[source.index];
        const size_t body = *e.zombie_work->body;
        e.position = {r.position.x + r.size.x * .5f,
                      source.collision.ground[size_t(int(r.floor) + 2)],
                      r.position.z + r.size.z * .5f};
        e.floor = r.floor;
        e.angles[1] =
            std::array<int32_t, 4>{0, 49152, 32768, 16384}[r.parameters[1]];
        e.modes = {1, 0, 8, 0};
        e.zombie_work->stair_target = source.index;
        reserve_actor_stairs(s, root, source.index);
        calculate_zombie_model(s, root);
        calculate_zombie_model(s, body);
        ScenarioEntity player;
        player.position = e.position;
        const float target_height =
            e.position.y +
            (r.parameters[0] == 0 ? 2.f : -2.f) * r.parameters[2];
        const auto expected_floor =
            player_floor_number(source.collision, target_height);
        const float expected_y =
            source.collision.ground[size_t(int(expected_floor) + 2)];
        const auto raw_model = e.model;
        const auto initial_attributes = [&] {
          auto copy = s;
          release_actor_stairs(copy, root);
          return copy.collision->miscellaneous;
        }();
        bool finished = false;
        for (size_t tick = 0; tick < 20000; ++tick) {
          const auto incoming_motion = e.motion_number;
          const auto incoming_blend = e.blend_count;
          tick_zombie_stairs(s, root, player);
          ++ticks;
          // ChgMtn retains blend_count when the incoming motion is already 2.
          // With zero blend, MV08 falls through directly to the entry clip.
          const bool immediate_entry = incoming_motion == 2 && !incoming_blend;
          const uint32_t startup_motion =
              immediate_entry ? (r.parameters[0] == 0 ? 58u : 59u) : 2u;
          if (!tick && (e.motion_number != startup_motion ||
                        s.enemies[body].motion_number != startup_motion + 200 ||
                        e.modes[3] != (immediate_entry ? 2 : 1) ||
                        e.floor_target_height != target_height))
            throw std::runtime_error(
                "Retail MV08 original startup mismatch: incoming=" +
                std::to_string(incoming_motion) +
                " blend=" + std::to_string(incoming_blend) +
                " actual=" + std::to_string(e.motion_number) +
                " body=" + std::to_string(s.enemies[body].motion_number) +
                " state=" + std::to_string(e.modes[3]));
          if (e.modes == std::array<uint8_t, 4>{1, 1, 1, 0}) {
            const int expected_step =
                r.parameters[0] == 0
                    ? (r.parameters[2] < 128 ? int(r.parameters[2])
                                             : int(r.parameters[2]) - 256)
                    : 0;
            if (e.floor != expected_floor || e.position.y != expected_y ||
                e.zombie_work->stair_step != expected_step ||
                e.motion_number !=
                    zombie_walk_motion(*e.zombie_walk_choice, false) ||
                s.enemies[body].motion_number !=
                    zombie_walk_motion(*e.zombie_walk_choice, true) ||
                e.frame != 3276800 || e.joint_pose[0].position->y != 0 ||
                e.model != raw_model ||
                e.zombie_work->stair_target != source.index ||
                e.stair_index != source.index)
              throw std::runtime_error(
                  "Retail MV08 original floor/byte/pose/walking handoff "
                  "mismatch");
            for (size_t i = 0; i < initial_attributes.size(); ++i)
              if (s.collision->miscellaneous[i].attribute !=
                  initial_attributes[i].attribute)
                throw std::runtime_error(
                    "Retail MV08 original stair reservation not released");
            finished = true;
            ++paths;
            break;
          }
          tick_zombie_motion(s, root, effects);
          calculate_zombie_model(s, root);
          tick_zombie_motion(s, body, effects);
          calculate_zombie_model(s, body);
          samples += 2;
          for (const auto &joint : e.model_work->joints)
            for (double value : joint.matrix)
              if (!std::isfinite(value))
                throw std::runtime_error(
                    "Retail MV08 original cache became nonfinite");
        }
        if (!finished)
          throw std::runtime_error(
              "Retail MV08 did not finish original stairs: room=" +
              std::to_string(source.room) + " state=" +
              std::to_string(e.modes[3]) + " frame=" + std::to_string(e.frame));
      }
    }
  if (roots != 10 || paths != 20 || ticks <= paths ||
      samples != (ticks - paths) * 2)
    throw std::runtime_error(
        "Retail MV08 original stair path coverage mismatch");
  std::cout << "Advanced " << ticks << " native MV08 states with " << samples
            << " original paired motion/cache samples; " << paths
            << " uphill/downhill floor/walking handoffs across " << roots
            << " roots using original stairs from rooms " << sources[0].room
            << "/" << sources[1].room << "\n";
}
} // namespace ashford::diagnostic
