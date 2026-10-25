#include "item_use_area.hpp"
#include "runtime.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
void check(bool ok) { if (!ok) throw std::runtime_error("Inventory use area regression"); }
int main() {
  try {
    RoomRuntime r;
    r.scenario.collision.emplace();
    CollisionRecord a;
    a.flags=1; a.type=0; a.position={-1,-1,-1}; a.size={2,2,2}; a.parameters[0]=65;
    r.scenario.collision->floors.push_back(a);
    r.scenario.flags[10][0]=0x400;
    update_inventory_use_area(r);
    check(selected_inventory_use_area(r.scenario)->parameters[0]==65);
    check((r.scenario.flags[10][0]&0x400)!=0);
    a.parameters[0]=82;
    r.scenario.dynamic_floors.push_back(a);
    update_inventory_use_area(r);
    check(r.scenario.floor_index==1 && selected_inventory_use_area(r.scenario)->parameters[0]==82);
    r.world.player.position.x=20;
    update_inventory_use_area(r);
    check(!selected_inventory_use_area(r.scenario));
    r.scenario.dynamic_floors.clear();
    a.attribute=1; a.floor=2; a.position={-1,100,-2}; a.size={2,1,2};
    r.scenario.collision->floors[0]=a;
    r.world.front_anchor={0,0,2}; r.world.front_offset=3; r.world.floor=2;
    update_inventory_use_area(r);
    check(selected_inventory_use_area(r.scenario)!=nullptr);
    r.scenario.collision->floors[0].attribute |= 0x1000;
    update_inventory_use_area(r);
    check(!selected_inventory_use_area(r.scenario));
    r.scenario.collision->floors[0].attribute=1;
    r.player_work.flags=4;
    update_inventory_use_area(r);
    check(!selected_inventory_use_area(r.scenario));
    std::cout << "Inventory use area checks passed\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
