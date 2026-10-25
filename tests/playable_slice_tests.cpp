#include "runtime_view.hpp"
#include "inventory_engine.hpp"
#include <SDL.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace ashford;

namespace {
void require(bool cond, const char *msg) {
  if (!cond)
    throw std::runtime_error(msg);
}
} // namespace

int main(int argc, char **argv) {
  try {
    fs::path assets_dir;
    if (argc > 1) {
      assets_dir = fs::path(argv[1]);
    } else {
      assets_dir = "local/cache-sharing-real-import/assets";
    }

    fs::path rdx = assets_dir / "RDX_LNK.AFS";
    fs::path system = assets_dir / "SYSTEM.AFS";
    if (!fs::exists(rdx) || !fs::exists(system)) {
      std::cout << "Assets not found at " << assets_dir
                << ", skipping playable slice tests.\n";
      return 77;
    }

    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
      throw std::runtime_error(SDL_GetError());

    struct SdlCleanup {
      ~SdlCleanup() { SDL_Quit(); }
    } cleanup;

    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("PlayableSliceTest", 0, 0, 320, 240,
                         SDL_WINDOW_HIDDEN),
        SDL_DestroyWindow);
    require(window != nullptr, "Created test window");

    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE),
        SDL_DestroyRenderer);
    require(renderer != nullptr, "Created test renderer");

    std::cout << "[Step 1 Test] Initializing RuntimeView...\n";
    RuntimeView view(renderer.get(), rdx, system, 100);

    // 1. Initial State Verification
    const auto &initial_state = view.state();
    require(initial_state.scenario.stage == 0, "Stage is 0");
    require(initial_state.scenario.room == 1, "Initial room is 1 (Corridor)");
    require(initial_state.scenario.equipped_weapon == 4, "Source handgun weapon index equipped");
    require(initial_state.scenario.inventory[0] == 2,
            "Header selects first carried word");
    require(InventoryEngine::get_item_id(InventoryEngine::get_slot(initial_state.scenario, 0)) == 9,
            "First carried slot is source handgun item");
    require(InventoryEngine::get_quantity(InventoryEngine::get_slot(initial_state.scenario, 0)) == 15,
            "Handgun loaded with 15 rounds");
    require(InventoryEngine::get_item_id(InventoryEngine::get_slot(initial_state.scenario, 1)) == 12,
            "Slot 1 is ammo (handgun bullets)");
    require(InventoryEngine::get_quantity(InventoryEngine::get_slot(initial_state.scenario, 1)) == 30,
            "Ammo count is 30");
    require(initial_state.scenario.player_health == 160,
            "Player health is 160");
    require(initial_state.world.cut.has_value(), "Camera cut active");
    std::cout << "  Passed: Initial state verified.\n";

    // 2. Locomotion Verification
    auto start_pos = view.state().world.player.position;
    for (int t = 0; t < 10; ++t) {
      view.tick(1); // W (forward)
    }
    auto moved_pos = view.state().world.player.position;
    require(moved_pos.x != start_pos.x || moved_pos.z != start_pos.z,
            "Player moved with forward input");
    std::cout << "  Passed: Locomotion verified.\n";

    // 3. Combat Verification: Aiming & Shooting Zombie
    size_t active_enemy_idx = size_t(-1);
    for (size_t i = 0; i < view.state().scenario.enemies.size(); ++i) {
      const auto &e = view.state().scenario.enemies[i];
      if ((e.flags & 1) && e.id != 0) {
        active_enemy_idx = i;
        break;
      }
    }
    require(active_enemy_idx != size_t(-1), "Active enemy present in Room 1");
    auto &zombie = view.state().scenario.enemies[active_enemy_idx];
    std::cout << "  Found active enemy at index " << active_enemy_idx
              << " with id=" << zombie.id << " type=" << zombie.type << "\n";
    require(zombie.id == 1, "Original zombie encounter required; scripted actor 67 is not a zombie");
    require(zombie.health > 0, "Encounter must initialize original zombie health");
    std::cout << "  Zombie flags: 0x" << std::hex << zombie.flags
              << " state_flags: 0x" << zombie.state_flags << std::dec << "\n";
    std::cout << "  Zombie geom: rad=" << zombie.geometry.radius
              << " col_rad=" << zombie.geometry.collision_radius
              << " cap_rad=" << zombie.geometry.capsule_radius
              << " h=" << zombie.geometry.height
              << " cap_start=(" << zombie.geometry.capsule_start.x << ","
              << zombie.geometry.capsule_start.y << ","
              << zombie.geometry.capsule_start.z << ")\n";

    std::cout << "  Player pos: (" << view.state().world.player.position.x << ", "
              << view.state().world.player.position.y << ", "
              << view.state().world.player.position.z << ")\n";
    std::cout << "  Zombie pos: (" << zombie.position.x << ", "
              << zombie.position.y << ", " << zombie.position.z << ")\n";

    // Position zombie directly in front of the player along current facing
    zombie.position = player_add_speed(view.state().world.player.position,
                                       view.state().world.player.yaw, 5.0f);
    zombie.position.y = view.state().world.player.position.y;
    std::cout << "  Encounter setup: Player at ("
              << view.state().world.player.position.x << ", "
              << view.state().world.player.position.y << ", "
              << view.state().world.player.position.z << ") targeting Zombie at ("
              << zombie.position.x << ", " << zombie.position.y << ", "
              << zombie.position.z << ")\n";

    // Aim (pad 0x10)
    view.tick(0x10);
    require(view.in_aim_mode(), "Player is in aim mode");

    // Fire weapon (pad 0x10 | 0x100)
    uint16_t ammo_before =
        InventoryEngine::get_quantity(InventoryEngine::get_slot(view.state().scenario, 0));
    view.tick(0x10 | 0x100);
    std::cout << "  First shot: hit=" << view.last_shot_hit()
              << " damage=" << view.last_damage_dealt()
              << " zombie_hp=" << zombie.health << "\n";
    view.tick(0x10); // release shoot
    uint16_t ammo_after =
        InventoryEngine::get_quantity(InventoryEngine::get_slot(view.state().scenario, 0));
    require(ammo_after == ammo_before - 1, "Bullet consumed on firing");

    // Continue firing until zombie is defeated
    int shots = 0;
    while (zombie.health > 0 && shots < 20) {
      view.tick(0x10 | 0x100);
      view.tick(0x10);
      ++shots;
    }
    require(zombie.health <= 0, "Zombie was defeated");
    require(zombie.flags & 2, "Zombie defeat bit set");
    require(view.state().scenario.flags[3][0] & 0x80000000u,
            "Persistent defeat recorded in scenario.flags[3]");
    std::cout << "  Passed: Combat encounter & defeat persistence verified "
                 "after "
              << (shots + 1) << " shots.\n";

    // 4. Ground Item Pickup Verification
    require(view.state().scenario.interactions.size() >= 4,
            "Interactions present");
    size_t item_trigger_idx = 0;
    for (size_t i = 0; i < view.state().scenario.interactions.size(); ++i) {
      if (view.state().scenario.interactions[i].type == 4) {
        item_trigger_idx = i;
        break;
      }
    }
    const auto &item_trig = view.state().scenario.interactions[item_trigger_idx];
    std::cout << "  Item trigger " << item_trigger_idx << ": type=" << int(item_trig.type)
              << " pos=(" << item_trig.position.x << "," << item_trig.position.y << "," << item_trig.position.z
              << ") size=(" << item_trig.size.x << "," << item_trig.size.z << ") attr=0x"
              << std::hex << item_trig.attribute << std::dec << " floor=" << int(item_trig.floor) << "\n";

    // Probe 4 is player_add_speed(player.pos, yaw, 2.0f)
    // In CVX, -Z is forward. Probe 4 along yaw 0: z = pos.z - 2.0.
    // If center of trigger is (4.495, -33.005), placing player at (4.495, -31.005) puts probe at (4.495, -33.005).
    view.state().world.player.position = {
        7.5f, item_trig.position.y,
        item_trig.position.z + item_trig.size.z * 0.5f};
    view.state().world.front_anchor = view.state().world.player.position;
    view.state().world.floor = item_trig.floor;
    view.state().world.player.yaw = 16384;
    view.state().player_work.position = view.state().world.player.position;
    view.state().player_work.angles = {0, 16384, 0};

    uint32_t ammo_inv_before =
        InventoryEngine::count_item(view.state().scenario, 12);
    view.tick(0x200); // Press Action to trigger examine interaction
    view.tick(0);     // Release Action

    uint32_t ammo_inv_after =
        InventoryEngine::count_item(view.state().scenario, 12);
    require(ammo_inv_after == ammo_inv_before + 30,
            "Handgun bullets added to inventory");
    require(view.state().scenario.flags[7][3] & 0x18,
            "Persistent pickup recorded in scenario.flags[7][3]");
    std::cout << "  Passed: Ground item pickup & persistence verified. Total "
                 "inventory ammo: "
              << ammo_inv_after << "\n";

    // 5. Door Transition Verification
    const auto &door_trig = view.state().scenario.interactions[0];
    std::cout << "  Door trigger 0: type=" << int(door_trig.type)
              << " pos=(" << door_trig.position.x << "," << door_trig.position.y << "," << door_trig.position.z
              << ") size=(" << door_trig.size.x << "," << door_trig.size.z << ") attr=0x"
              << std::hex << door_trig.attribute << std::dec << " floor=" << int(door_trig.floor) << "\n";
    view.state().world.player.position = {
        -36.0f, door_trig.position.y,
        door_trig.position.z + door_trig.size.z * 0.5f};
    view.state().world.front_anchor = view.state().world.player.position;
    view.state().world.front_anchor.y += 12.5f;
    view.state().world.front_offset = 0.0f;
    view.state().world.floor = door_trig.floor;
    view.state().world.player.yaw = 16384;
    view.state().player_work.position = view.state().world.player.position;
    view.state().player_work.angles = {0, 16384, 0};

    view.tick(0x200); // Press Action to examine door
    require(view.state().scenario.room == 2,
            "Transitioned to Room 2 (Graveyard)");
    require(view.state().world.cut.has_value(), "Room 2 camera cut active");
    std::cout << "  Passed: Door transition Room 1 -> Room 2 verified.\n";

    // 6. Native Drawing Verification
    SDL_Rect dest{0, 0, 320, 240};
    view.draw(dest);
    std::cout << "  Passed: Native scene drawing verified.\n";

    // 7. Save / Load Verification
    fs::path temp_save = fs::path("local/playable-slice-tests/slice.dat");
    view.save(temp_save);
    require(fs::exists(temp_save), "Save file created on disk");

    // Modify state before reload
    view.state().scenario.player_health = 42;
    view.state().world.player.position = {99.0f, 99.0f, 99.0f};

    // Load state
    view.load(temp_save);
    require(view.state().scenario.room == 2, "Loaded room is 2 (Graveyard)");
    require(view.state().scenario.player_health == 160,
            "Loaded health restored to 160");
    require(uint32_t(InventoryEngine::count_item(view.state().scenario, 12)) == ammo_inv_after,
            "Loaded inventory ammo restored");
    require(view.state().scenario.flags[3][0] & 0x80000000u,
            "Loaded defeat persistence restored");
    require(view.state().scenario.flags[7][3] & 0x18,
            "Loaded pickup persistence restored");
    fs::remove(temp_save);
    std::cout << "  Passed: Save / Load round trip verified.\n";

    std::cout << "\nPrototype slice regression checks passed; independent original-game acceptance remains required.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Playable slice test failed: " << e.what() << "\n";
    return 1;
  }
}
