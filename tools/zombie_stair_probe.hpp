#pragma once
#include "scenario.hpp"
namespace ashford::diagnostic {
struct StairProbeSource {
  RoomCollision collision;
  size_t index = 0, room = 0;
};
struct StairProbeActors {
  ScenarioState state;
  std::vector<size_t> roots;
};
void collect_stair_probe_sources(std::vector<StairProbeSource> &sources,
                                 const RoomCollision &room, size_t room_id);
void run_zombie_stair_probes(const std::vector<StairProbeSource> &sources,
                             const std::vector<StairProbeActors> &actors);
} // namespace ashford::diagnostic
