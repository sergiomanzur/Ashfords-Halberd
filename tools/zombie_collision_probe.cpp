#include "zombie_collision_probe.hpp"
#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace ashford::diagnostic {
void run_zombie_collision_probes(
    const std::vector<ZombieCollisionProbeActors> &contexts) {
  size_t roots = 0, callbacks = 0, ordinary = 0, shapes = 0, changes = 0;
  for (const auto &context : contexts)
    for (const auto root : context.roots) {
      ++roots;
      for (int shape = -1; shape < 7; ++shape)
        for (bool linked : {false, true}) {
          auto s = context.state;
          auto &e = s.enemies.at(root);
          if (!e.zombie_work || !e.zombie_work->body)
            throw std::runtime_error("Original collision probe missing body");
          const auto body = *e.zombie_work->body;
          // ControlEnemies captures the preceding work position before
          // callbacks.
          e.previous_position = e.position;
          if (shape >= 0) {
            e.zombie_flags =
                (e.zombie_flags & ~15u) | 0x40000000u | uint32_t(shape);
            e.flags |= 0x10;
          }
          ScenarioEntity player;
          player.flags = 0x118;
          player.position = context.entry;
          player.floor = player_floor_number(*s.collision, context.entry.y);
          player.position.y =
              s.collision->ground[size_t(int(player.floor) + 2)];
          player.geometry.radius = 3.5f;
          player.geometry.height = 15.5f;
          player.geometry.collision_radius = 2.5f;
          player.geometry.collision_height = 15.5f;
          const auto model = e.model;
          const auto motions = e.motions;
          const auto old_position = e.position;
          const auto old_seed = s.random_seed;
          tick_zombie_collision(s, linked ? body : root, player);
          ++callbacks;
          if (shape < 0)
            ++ordinary;
          else
            ++shapes;
          if (e.position.x != old_position.x ||
              e.position.y != old_position.y || e.position.z != old_position.z)
            ++changes;
          if (!std::isfinite(e.position.x) || !std::isfinite(e.position.y) ||
              !std::isfinite(e.position.z) ||
              !std::isfinite(player.position.x) ||
              !std::isfinite(player.position.y) ||
              !std::isfinite(player.position.z) || e.model != model ||
              e.motions != motions || s.random_seed != old_seed ||
              e.joint_pose.size() !=
                  context.state.enemies[root].joint_pose.size() ||
              !e.model_work || !context.state.enemies[root].model_work ||
              e.model_work->joints.size() !=
                  context.state.enemies[root].model_work->joints.size())
            throw std::runtime_error(
                "Original collision changed pose/asset/RNG ownership or became "
                "nonfinite");
          for (size_t joint = 0; joint < e.joint_pose.size(); ++joint) {
            const auto &before = context.state.enemies[root].joint_pose[joint];
            const auto &after = e.joint_pose[joint];
            if (before.angles != after.angles ||
                bool(before.position) != bool(after.position) ||
                (before.position &&
                 (before.position->x != after.position->x ||
                  before.position->y != after.position->y ||
                  before.position->z != after.position->z)) ||
                e.model_work->joints[joint].matrix !=
                    context.state.enemies[root]
                        .model_work->joints[joint]
                        .matrix)
              throw std::runtime_error(
                  "Original collision mutated retained pose/cache");
          }
        }
    }
  if (roots != 10 || callbacks != 160 || ordinary != 20 || shapes != 140)
    throw std::runtime_error("Original collision probe coverage mismatch");
  std::cout << "Applied " << callbacks
            << " native zombie collision callbacks: " << ordinary
            << " original root/body paths and " << shapes
            << " posture0..6 dependency paths across " << roots
            << " original graveyard roots; " << changes
            << " displaced positions\n";
}
} // namespace ashford::diagnostic
