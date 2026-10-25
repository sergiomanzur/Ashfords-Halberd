#pragma once
#include "collision.hpp"
namespace ashford {
struct ScenarioState;
struct RoomRuntime;
// Inventory type-0 portion of bhCheckFloorP; other floor controllers are separate.
void update_inventory_use_area(RoomRuntime &runtime);
const CollisionRecord *selected_inventory_use_area(const ScenarioState &state);
}
