#pragma once
#include "scenario.hpp"
namespace ashford::diagnostic {
struct ZombieCollisionProbeActors {
  ScenarioState state;
  std::vector<size_t> roots;
  Vec3 entry;
};
void run_zombie_collision_probes(
    const std::vector<ZombieCollisionProbeActors> &);
} // namespace ashford::diagnostic
