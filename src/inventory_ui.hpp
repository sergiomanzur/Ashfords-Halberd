#pragma once
#include "inventory_engine.hpp"
#include "text.hpp"
#include <SDL.h>
#include <filesystem>
#include <string>
#include <vector>

namespace ashford {

enum class InventoryState {
  Browsing,
  CommandMenu,
  CombineSelect,
  ExamineView
};

class InventoryUI {
public:
  InventoryUI();

  ~InventoryUI();

  void open();
  void close();
  void toggle();
  bool is_open() const { return open_; }

  void set_assets_dir(const std::filesystem::path &dir) { assets_dir_ = dir; }
  bool has_textures() const { return textures_loaded_; }

  void handle_key_down(SDL_Keycode key, ScenarioState &state);
  void handle_pad_button(uint8_t button, ScenarioState &state);
  void tick(uint16_t pad, ScenarioState &state);

  void render(SDL_Renderer *renderer, Text &text, const ScenarioState &state,
              int width = 1120, int height = 720);

  size_t selected_slot() const { return cursor_; }
  InventoryState current_state() const { return state_; }
  const std::string &message() const { return notice_message_; }

private:
  void navigate_grid(int dx, int dy, size_t capacity);
  void select_current(ScenarioState &state);
  void cancel_action();
  void build_command_options(const ScenarioState &state);
  void execute_command(ScenarioState &state);

  void ensure_textures(SDL_Renderer *renderer);
  void render_authentic(SDL_Renderer *renderer, Text &text,
                        const ScenarioState &state, int width, int height);
  void render_fallback(SDL_Renderer *renderer, Text &text,
                       const ScenarioState &state, int width, int height);

  bool open_ = false;
  size_t cursor_ = 0;
  size_t combine_source_ = 0;
  size_t command_cursor_ = 0;
  InventoryState state_ = InventoryState::Browsing;
  std::vector<std::string> command_options_;
  std::string notice_message_;
  int message_timer_ = 0;
  float ecg_phase_ = 0.0f;
  uint32_t anim_frame_ = 0;

  std::filesystem::path assets_dir_;
  bool textures_loaded_ = false;
  SDL_Renderer *cached_renderer_ = nullptr;
  SDL_Texture *subtx_[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
};

} // namespace ashford
