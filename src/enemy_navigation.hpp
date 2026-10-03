#pragma once
#include "scenario.hpp"
namespace ashford {
std::optional<size_t>
enemy_stair_trigger(const RoomCollision &room,
                    const std::vector<CollisionRecord> &dynamic, Vec3 position,
                    int32_t yaw, int32_t floor, float offset);
void reserve_actor_stairs(ScenarioState &state, size_t actor, size_t index);
void release_actor_stairs(ScenarioState &state, size_t actor);
} // namespace ashford
