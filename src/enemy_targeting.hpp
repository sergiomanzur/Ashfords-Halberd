#pragma once
#include "scenario.hpp"
namespace ashford {
// pwksub.c::bhSearchPlayer and en01b.c::bhSearchPlayer2. The explicit probe
// replaces the source's temporary actor-position/yaw overwrite and restore.
// -1 means outside the view cone; valid signed yaw -1 is remapped to -2.
int32_t search_enemy_player(Vec3 probe, int32_t yaw, Vec3 player, int32_t cone);
// zonzon.c::NitenDir_ck, preserving float atan2 and truncation to signed yaw.
int32_t enemy_direction(Vec3 origin, Vec3 target);
// en01b.c::bhEne01_SideRotNeck. Inputs are cached joint-world origins; all
// rotations retain source 16-bit wrap, strict snap range and 910-word step.
int32_t turn_enemy_neck(Vec3 origin, Vec3 target, int32_t neck_yaw,
                        int32_t limit, int32_t actor_yaw);
// zonzon.c::bhEne_EnemyAtariCheck. Returned pointer belongs to the supplied
// room or dynamic vector and is valid only while that owner remains stable.
const CollisionRecord *
enemy_floor_trigger(const RoomCollision &room,
                    const std::vector<CollisionRecord> &dynamic, Vec3 position,
                    int32_t floor, uint8_t id, uint8_t action);
// en01b.c::bhEne01_RotNeck. Resolves the model-work owner and the linked
// parent yaw, reads cached matrices and changes only the selected pose yaw.
void rotate_zombie_neck(ScenarioState &state, size_t enemy, size_t neck,
                        const ActorModelWork &player, size_t player_neck);
} // namespace ashford
