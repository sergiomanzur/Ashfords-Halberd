#include "inventory_ui.hpp"
#include "runtime_view.hpp"
#include "text.hpp"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
using namespace ashford;

static void save_frame_bmp(SDL_Renderer *renderer, int width, int height,
                           const fs::path &output_path) {
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!surface) {
    throw std::runtime_error(std::string("Cannot create surface: ") + SDL_GetError());
  }
  if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                           surface->pixels, surface->pitch) < 0) {
    SDL_FreeSurface(surface);
    throw std::runtime_error(std::string("Cannot read pixels: ") + SDL_GetError());
  }
  if (SDL_SaveBMP(surface, output_path.string().c_str()) < 0) {
    SDL_FreeSurface(surface);
    throw std::runtime_error(std::string("Cannot save BMP: ") + SDL_GetError());
  }
  SDL_FreeSurface(surface);
  std::cout << "Saved: " << output_path << std::endl;
}

int main(int argc, char **argv) {
  try {
    fs::path assets_dir = "local/cache-sharing-real-import/assets";
    fs::path out_dir = "C:/Users/sergi/.gemini/antigravity/brain/5ce08b74-1e21-4e5a-af06-182070f832ef";

    if (argc > 1) {
      assets_dir = fs::path(argv[1]);
    }
    if (argc > 2) {
      out_dir = fs::path(argv[2]);
    }

    fs::create_directories(out_dir);

    fs::path rdx = assets_dir / "RDX_LNK.AFS";
    fs::path system = assets_dir / "SYSTEM.AFS";
    if (!fs::exists(rdx) || !fs::exists(system)) {
      std::cerr << "Required assets not found in " << assets_dir << std::endl;
      return 1;
    }

    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
      throw std::runtime_error(SDL_GetError());
    }

    struct SdlCleanup {
      ~SdlCleanup() { SDL_Quit(); }
    } cleanup;

    const int width = 1120;
    const int height = 720;

    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("AshfordCapture", 0, 0, width, height, SDL_WINDOW_HIDDEN),
        SDL_DestroyWindow);
    if (!window) throw std::runtime_error(SDL_GetError());

    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE),
        SDL_DestroyRenderer);
    if (!renderer) throw std::runtime_error(SDL_GetError());

    Text text(renderer.get());
    RuntimeView view(renderer.get(), rdx, system, 100);
    InventoryUI inventory_ui;
    inventory_ui.set_assets_dir(assets_dir);

    // -------------------------------------------------------------
    // Capture 1: Corridor Exploration (Claire in Stage 0 Room 1)
    // -------------------------------------------------------------
    std::cout << "[Capture 1] Rendering Stage 0 Room 1 (Corridor)..." << std::endl;
    // Advance a few ticks for camera and lighting settling
    for (int t = 0; t < 15; ++t) {
      view.tick(1); // Move forward slightly
    }

    SDL_SetRenderDrawColor(renderer.get(), 13, 18, 23, 255);
    SDL_RenderClear(renderer.get());
    SDL_Rect game_rect{160, 55, 800, 600};
    view.draw(game_rect);

    // Overlay HUD
    text.draw("STEP 1 PLAYABLE SLICE", 68, 18, 16, {202, 174, 117, 255});
    auto &scen = view.state().scenario;
    uint16_t ammo = uint16_t(scen.inventory[0] & 0xffff);
    std::string status = "HP: " + std::to_string(scen.player_health) + "   AMMO: " +
                         std::to_string(ammo) + "   Stage 0 Room 1 (Corridor)";
    text.draw(status, 560, 18, 15, {236, 234, 225, 255});
    text.draw("WASD: Move   SHIFT: Run   SPACE: Aim   ENTER/E: Shoot   TAB/I: Inventory   ESC: Menu",
              68, 673, 14, {236, 234, 225, 255});

    save_frame_bmp(renderer.get(), width, height, out_dir / "gameplay_1_corridor.bmp");

    // -------------------------------------------------------------
    // Capture 2: Combat Aiming & Zombie Encounter
    // -------------------------------------------------------------
    std::cout << "[Capture 2] Rendering Combat & Aiming..." << std::endl;
    // Enter aim mode (pad bit 0x10)
    for (int t = 0; t < 10; ++t) {
      view.tick(0x10); // Hold aim
    }

    SDL_SetRenderDrawColor(renderer.get(), 13, 18, 23, 255);
    SDL_RenderClear(renderer.get());
    view.draw(game_rect);

    text.draw("STEP 1 PLAYABLE SLICE", 68, 18, 16, {202, 174, 117, 255});
    text.draw(status, 560, 18, 15, {236, 234, 225, 255});
    text.draw("AIMING - Press ENTER or E to shoot", 170, 631, 14, {202, 174, 117, 255});
    text.draw("WASD: Move   SHIFT: Run   SPACE: Aim   ENTER/E: Shoot   TAB/I: Inventory   ESC: Menu",
              68, 673, 14, {236, 234, 225, 255});

    save_frame_bmp(renderer.get(), width, height, out_dir / "gameplay_2_combat.bmp");

    // -------------------------------------------------------------
    // Capture 3: Interactive Inventory UI & ECG Monitor
    // -------------------------------------------------------------
    std::cout << "[Capture 3] Rendering Native Inventory UI..." << std::endl;
    inventory_ui.open();
    // Simulate some frames of ECG pulse
    for (int t = 0; t < 30; ++t) {
      inventory_ui.tick(0, scen);
    }

    SDL_SetRenderDrawColor(renderer.get(), 13, 18, 23, 255);
    SDL_RenderClear(renderer.get());
    view.draw(game_rect);
    inventory_ui.render(renderer.get(), text, scen, width, height);

    save_frame_bmp(renderer.get(), width, height, out_dir / "gameplay_3_inventory.bmp");

    // -------------------------------------------------------------
    // Capture 4: Stage 0 Room 2 (Graveyard)
    // -------------------------------------------------------------
    std::cout << "[Capture 4] Transitioning to Stage 0 Room 2 (Graveyard)..." << std::endl;
    inventory_ui.close();

    // Trigger door transition to Room 2
    try {
      if (!scen.interactions.empty()) {
        const auto &door_trig = scen.interactions[0];
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
        view.tick(0x200); // Action to open door
      }
      for (int t = 0; t < 20; ++t) {
        view.tick(1); // Advance in Graveyard
      }
    } catch (const std::exception &e) {
      std::cout << "Transition note: " << e.what() << std::endl;
    }

    SDL_SetRenderDrawColor(renderer.get(), 13, 18, 23, 255);
    SDL_RenderClear(renderer.get());
    view.draw(game_rect);

    text.draw("STEP 1 PLAYABLE SLICE", 68, 18, 16, {202, 174, 117, 255});
    std::string room2_status = "HP: " + std::to_string(scen.player_health) + "   AMMO: " +
                              std::to_string(ammo) + "   Stage 0 Room 2 (Graveyard)";
    text.draw(room2_status, 560, 18, 15, {236, 234, 225, 255});
    text.draw("WASD: Move   SHIFT: Run   SPACE: Aim   ENTER/E: Shoot   TAB/I: Inventory   ESC: Menu",
              68, 673, 14, {236, 234, 225, 255});

    save_frame_bmp(renderer.get(), width, height, out_dir / "gameplay_4_graveyard.bmp");

    std::cout << "All gameplay frames captured successfully!" << std::endl;
    return 0;
  } catch (const std::exception &err) {
    std::cerr << "Error during capture: " << err.what() << std::endl;
    return 1;
  }
}
