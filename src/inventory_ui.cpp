#include "item_use_area.hpp"
#include "inventory_ui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace ashford {

namespace {

float ecg_waveform(float t) {
  // One cardiac cycle t in [0, 1)
  if (t < 0.12f) {
    float p = (t - 0.03f) / 0.09f;
    if (p >= 0.0f && p <= 1.0f) {
      return 0.18f * std::sin(p * 3.14159265f);
    }
  } else if (t >= 0.16f && t < 0.20f) {
    float q = (t - 0.16f) / 0.04f;
    return -0.16f * std::sin(q * 3.14159265f);
  } else if (t >= 0.20f && t < 0.26f) {
    float r = (t - 0.20f) / 0.06f;
    return 1.0f * std::sin(r * 3.14159265f);
  } else if (t >= 0.26f && t < 0.31f) {
    float s = (t - 0.26f) / 0.05f;
    return -0.32f * std::sin(s * 3.14159265f);
  } else if (t >= 0.38f && t < 0.58f) {
    float tw = (t - 0.38f) / 0.20f;
    return 0.28f * std::sin(tw * 3.14159265f);
  }
  return 0.0f;
}

struct ItemSpriteCoord {
  int tex_id = 4;
  int x = 0, y = 0, width = 34, height = 24;
  int slots = 1;
};

ItemSpriteCoord get_item_sprite(uint16_t id) {
  const auto &item = ItemDatabase::get(id);
  return {item.sprite_texture, item.sprite_cell_x, item.sprite_cell_y,
          item.sprite_cell_width, item.sprite_cell_height, item.slot_count};
}

} // namespace

InventoryUI::InventoryUI() = default;

InventoryUI::~InventoryUI() {
  for (int i = 0; i < 5; ++i) {
    if (subtx_[i]) {
      SDL_DestroyTexture(subtx_[i]);
      subtx_[i] = nullptr;
    }
  }
}

void InventoryUI::ensure_textures(SDL_Renderer *renderer) {
  if (!renderer) return;
  if (textures_loaded_ && cached_renderer_ == renderer) return;
  if (cached_renderer_ && cached_renderer_ != renderer) {
    for (int i = 0; i < 5; ++i) {
      if (subtx_[i]) {
        SDL_DestroyTexture(subtx_[i]);
        subtx_[i] = nullptr;
      }
    }
    textures_loaded_ = false;
  }
  cached_renderer_ = renderer;

  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;
  if (!assets_dir_.empty()) {
    candidates.push_back(assets_dir_ / "ITEM1.AFS");
    candidates.push_back(assets_dir_ / "assets/ITEM1.AFS");
  }
  candidates.push_back("assets/ITEM1.AFS");
  candidates.push_back("local/cache-sharing-real-import/assets/ITEM1.AFS");
  if (const char *local_app = std::getenv("LOCALAPPDATA")) {
    candidates.push_back(fs::path(local_app) / "AshfordsHalberd/assets/ITEM1.AFS");
    candidates.push_back(fs::path(local_app) / "AshfordsHalberd/ITEM1.AFS");
  }
  candidates.push_back(fs::current_path() / "assets/ITEM1.AFS");
  candidates.push_back(fs::current_path() / "local/cache-sharing-real-import/assets/ITEM1.AFS");

  fs::path item1_path;
  for (const auto &c : candidates) {
    std::error_code ec;
    if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) {
      item1_path = c;
      break;
    }
  }
  if (item1_path.empty()) return;

  std::ifstream f(item1_path, std::ios::binary);
  if (!f) return;

  char afs_hdr[8];
  if (!f.read(afs_hdr, 8)) return;
  if (std::memcmp(afs_hdr, "AFS\0", 4) != 0) return;

  uint32_t count = uint32_t(uint8_t(afs_hdr[4])) |
                   (uint32_t(uint8_t(afs_hdr[5])) << 8) |
                   (uint32_t(uint8_t(afs_hdr[6])) << 16) |
                   (uint32_t(uint8_t(afs_hdr[7])) << 24);
  if (count <= 145) return;

  f.seekg(8 + 145 * 8);
  char entry_desc[8];
  if (!f.read(entry_desc, 8)) return;
  uint32_t entry_off = uint32_t(uint8_t(entry_desc[0])) |
                       (uint32_t(uint8_t(entry_desc[1])) << 8) |
                       (uint32_t(uint8_t(entry_desc[2])) << 16) |
                       (uint32_t(uint8_t(entry_desc[3])) << 24);
  uint32_t entry_sz = uint32_t(uint8_t(entry_desc[4])) |
                      (uint32_t(uint8_t(entry_desc[5])) << 8) |
                      (uint32_t(uint8_t(entry_desc[6])) << 16) |
                      (uint32_t(uint8_t(entry_desc[7])) << 24);
  if (entry_sz < 824620 + 200 || entry_sz > 16 * 1024 * 1024) return;

  std::vector<uint8_t> data(entry_sz);
  f.seekg(entry_off);
  if (!f.read(reinterpret_cast<char *>(data.data()), entry_sz)) return;

  static const size_t kOffsets[5] = {124, 34120, 297620, 561120, 824620};

  auto read_u16 = [&](size_t pos) -> uint16_t {
    if (pos + 2 > data.size()) return 0;
    return uint16_t(data[pos]) | (uint16_t(data[pos + 1]) << 8);
  };
  auto read_u32 = [&](size_t pos) -> uint32_t {
    if (pos + 4 > data.size()) return 0;
    return uint32_t(data[pos]) | (uint32_t(data[pos + 1]) << 8) |
           (uint32_t(data[pos + 2]) << 16) | (uint32_t(data[pos + 3]) << 24);
  };

  for (int t = 0; t < 5; ++t) {
    size_t off = kOffsets[t];
    size_t pic = off + 128;
    if (pic + 32 > data.size()) continue;

    uint32_t clut = read_u32(pic + 4);
    uint32_t img = read_u32(pic + 8);
    uint16_t hdr = read_u16(pic + 12);
    uint8_t bpp = data[pic + 19];
    uint16_t w = read_u16(pic + 20);
    uint16_t h = read_u16(pic + 22);

    if (w == 0 || h == 0 || w > 1024 || h > 1024) continue;
    size_t pix_off = pic + hdr;
    size_t clut_off = pix_off + img;
    if (clut_off + clut > data.size()) continue;

    std::vector<uint32_t> pixels(size_t(w) * h, 0);

    if (bpp == 5) {
      std::vector<uint32_t> raw_pal(256);
      for (size_t ci = 0; ci < 256; ++ci) {
        size_t cp = clut_off + ci * 4;
        uint8_t r = data[cp];
        uint8_t g = data[cp + 1];
        uint8_t b = data[cp + 2];
        uint8_t a = data[cp + 3];
        a = (uint8_t)std::min(255, a * 2);

        // Color key item sprites in sheets 3 and 4
        if (t == 3 || t == 4) {
          if (r <= 16 && g <= 6 && b >= 60 && b <= 100) {
            a = 0;
          }
        }
        // Alpha key vital monitor / ECG in sheet 2
        if (t == 2) {
          if (a <= 32 && r <= 10 && g <= 10 && b <= 10) {
            a = 0;
          }
        }
        raw_pal[ci] = (uint32_t(a) << 24) | (uint32_t(b) << 16) |
                      (uint32_t(g) << 8) | uint32_t(r);
      }

      std::vector<uint32_t> unswizzled(256);
      for (int block = 0; block < 8; ++block) {
        int base = block * 32;
        for (int i = 0; i < 8; ++i) {
          unswizzled[base + i] = raw_pal[base + i];
          unswizzled[base + i + 8] = raw_pal[base + i + 16];
          unswizzled[base + i + 16] = raw_pal[base + i + 8];
          unswizzled[base + i + 24] = raw_pal[base + i + 24];
        }
      }

      for (size_t pi = 0; pi < pixels.size(); ++pi) {
        if (pix_off + pi < data.size()) {
          uint8_t idx = data[pix_off + pi];
          pixels[pi] = unswizzled[idx];
        }
      }
    } else if (bpp == 4) {
      size_t num_colors = std::min<size_t>(16, clut / 4);
      std::vector<uint32_t> pal(16, 0);
      for (size_t ci = 0; ci < num_colors; ++ci) {
        size_t cp = clut_off + ci * 4;
        uint8_t r = data[cp];
        uint8_t g = data[cp + 1];
        uint8_t b = data[cp + 2];
        uint8_t a = data[cp + 3];
        a = (uint8_t)std::min(255, a * 2);
        if (a <= 32 || (r == 0 && g == 0 && b == 0)) {
          a = 0;
        }
        pal[ci] = (uint32_t(a) << 24) | (uint32_t(b) << 16) |
                  (uint32_t(g) << 8) | uint32_t(r);
      }

      for (size_t bi = 0; bi < (pixels.size() + 1) / 2; ++bi) {
        if (pix_off + bi < data.size()) {
          uint8_t bval = data[pix_off + bi];
          uint8_t idx0 = bval & 0x0f;
          uint8_t idx1 = (bval >> 4) & 0x0f;
          if (bi * 2 < pixels.size()) pixels[bi * 2] = pal[idx0];
          if (bi * 2 + 1 < pixels.size()) pixels[bi * 2 + 1] = pal[idx1];
        }
      }
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface) {
      subtx_[t] = SDL_CreateTextureFromSurface(renderer, surface);
      if (subtx_[t]) {
        SDL_SetTextureBlendMode(subtx_[t], SDL_BLENDMODE_BLEND);
      }
      SDL_FreeSurface(surface);
    }
  }

  if (subtx_[1] && subtx_[2] && subtx_[4]) {
    textures_loaded_ = true;
  }
}

void InventoryUI::open() {
  open_ = true;
  cursor_ = 0;
  combine_source_ = 0;
  command_cursor_ = 0;
  state_ = InventoryState::Browsing;
  command_options_.clear();
  notice_message_.clear();
  message_timer_ = 0;
}

void InventoryUI::close() {
  open_ = false;
  state_ = InventoryState::Browsing;
  command_options_.clear();
  notice_message_.clear();
  message_timer_ = 0;
}

void InventoryUI::toggle() {
  if (open_) {
    close();
  } else {
    open();
  }
}

void InventoryUI::navigate_grid(int dx, int dy, size_t capacity) {
  if (capacity == 0) return;
  if (cursor_ == InventoryEngine::kExtraSlot) cursor_ = 0;
  int cols = 2;
  int rows = (static_cast<int>(capacity) + cols - 1) / cols;
  int col = static_cast<int>(cursor_) % cols;
  int row = static_cast<int>(cursor_) / cols;

  if (dx != 0) {
    col = (col + dx + cols) % cols;
  }
  if (dy != 0) {
    row = (row + dy + rows) % rows;
  }

  size_t next = static_cast<size_t>(row * cols + col);
  if (next < capacity) {
    cursor_ = next;
  } else {
    cursor_ = capacity - 1;
  }
}

void InventoryUI::build_command_options(const ScenarioState &state) {
  command_options_.clear();
  command_cursor_ = 0;
  uint32_t val = InventoryEngine::get_slot(state, cursor_);
  uint16_t id = InventoryEngine::get_item_id(val);
  if (id == 0) return;

  if (ItemDatabase::weapon_index(uint16_t(val >> 16)) >= 0 && id != 0) {
    command_options_.push_back(InventoryEngine::is_equipped(state, cursor_) ? "UNEQUIP" : "EQUIP");
  }
  const auto *area = selected_inventory_use_area(state);
  const bool area_match = area && std::find(area->parameters.begin(), area->parameters.end(), id) != area->parameters.end();
  if (InventoryEngine::can_use_item(state, cursor_) && (ItemDatabase::weapon_index(uint16_t(val >> 16)) < 0 || area_match)) {
    command_options_.push_back("USE");
  }
  if (cursor_ != InventoryEngine::kExtraSlot && ItemDatabase::get(id).combination_offset >= 0)
    command_options_.push_back("COMBINE");
  command_options_.push_back("CHECK");
  command_options_.push_back("CANCEL");
}

void InventoryUI::execute_command(ScenarioState &state) {
  if (command_cursor_ >= command_options_.size()) return;
  const std::string &cmd = command_options_[command_cursor_];

  if (cmd == "EQUIP" || cmd == "UNEQUIP") {
    std::string msg;
    InventoryEngine::equip_weapon(state, cursor_, msg);
    notice_message_ = msg;
    message_timer_ = 120;
    state_ = InventoryState::Browsing;
  } else if (cmd == "USE") {
    std::string msg;
    const bool used = InventoryEngine::use_item(state, cursor_, msg);
    if (used && (state.flags[10][0] & 0x400)) close();
    notice_message_ = msg;
    message_timer_ = 120;
    state_ = InventoryState::Browsing;
  } else if (cmd == "COMBINE") {
    combine_source_ = cursor_;
    state_ = InventoryState::CombineSelect;
    notice_message_ = "Select item to combine with.";
    message_timer_ = 120;
  } else if (cmd == "CHECK") {
    state_ = InventoryState::ExamineView;
  } else if (cmd == "CANCEL") {
    state_ = InventoryState::Browsing;
  }
}

void InventoryUI::select_current(ScenarioState &state) {
  if (state_ == InventoryState::Browsing) {
    uint32_t val = InventoryEngine::get_slot(state, cursor_);
    uint16_t id = InventoryEngine::get_item_id(val);
    if (id == 0) {
      notice_message_ = "Slot is empty.";
      message_timer_ = 60;
      return;
    }
    build_command_options(state);
    if (!command_options_.empty()) {
      state_ = InventoryState::CommandMenu;
    }
  } else if (state_ == InventoryState::CommandMenu) {
    execute_command(state);
  } else if (state_ == InventoryState::CombineSelect) {
    if (cursor_ == combine_source_) {
      state_ = InventoryState::Browsing;
      notice_message_ = "Combine cancelled.";
      message_timer_ = 60;
      return;
    }
    std::string msg;
    if (InventoryEngine::can_combine(state, combine_source_, cursor_)) {
      InventoryEngine::combine_slots(state, combine_source_, cursor_, msg);
      notice_message_ = msg;
      message_timer_ = 120;
      state_ = InventoryState::Browsing;
    } else {
      InventoryEngine::swap_slots(state, combine_source_, cursor_);
      notice_message_ = "Moved items.";
      message_timer_ = 60;
      state_ = InventoryState::Browsing;
    }
  } else if (state_ == InventoryState::ExamineView) {
    state_ = InventoryState::Browsing;
  }
}

void InventoryUI::cancel_action() {
  if (state_ == InventoryState::ExamineView) {
    state_ = InventoryState::Browsing;
  } else if (state_ == InventoryState::CombineSelect) {
    state_ = InventoryState::Browsing;
    notice_message_ = "Combine cancelled.";
    message_timer_ = 60;
  } else if (state_ == InventoryState::CommandMenu) {
    state_ = InventoryState::Browsing;
  } else if (state_ == InventoryState::Browsing) {
    close();
  }
}

void InventoryUI::handle_key_down(SDL_Keycode key, ScenarioState &state) {
  if (key == SDLK_TAB || key == SDLK_i) {
    toggle();
    return;
  }
  if (!open_) return;

  size_t cap = InventoryEngine::get_capacity(state);

  if (key == SDLK_x && state_ == InventoryState::Browsing) {
    cursor_ = cursor_ == InventoryEngine::kExtraSlot ? 0 : InventoryEngine::kExtraSlot;
    return;
  }
  if (key == SDLK_ESCAPE || key == SDLK_BACKSPACE) {
    cancel_action();
    return;
  }

  if (state_ == InventoryState::Browsing) {
    if (key == SDLK_UP || key == SDLK_w) navigate_grid(0, -1, cap);
    else if (key == SDLK_DOWN || key == SDLK_s) navigate_grid(0, 1, cap);
    else if (key == SDLK_LEFT || key == SDLK_a) navigate_grid(-1, 0, cap);
    else if (key == SDLK_RIGHT || key == SDLK_d) navigate_grid(1, 0, cap);
    else if (key == SDLK_RETURN || key == SDLK_e || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
      select_current(state);
    }
  } else if (state_ == InventoryState::CommandMenu) {
    if (key == SDLK_UP || key == SDLK_w) {
      if (!command_options_.empty()) {
        command_cursor_ = (command_cursor_ + command_options_.size() - 1) % command_options_.size();
      }
    } else if (key == SDLK_DOWN || key == SDLK_s) {
      if (!command_options_.empty()) {
        command_cursor_ = (command_cursor_ + 1) % command_options_.size();
      }
    } else if (key == SDLK_RETURN || key == SDLK_e || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
      select_current(state);
    }
  } else if (state_ == InventoryState::CombineSelect) {
    if (key == SDLK_UP || key == SDLK_w) navigate_grid(0, -1, cap);
    else if (key == SDLK_DOWN || key == SDLK_s) navigate_grid(0, 1, cap);
    else if (key == SDLK_LEFT || key == SDLK_a) navigate_grid(-1, 0, cap);
    else if (key == SDLK_RIGHT || key == SDLK_d) navigate_grid(1, 0, cap);
    else if (key == SDLK_RETURN || key == SDLK_e || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
      select_current(state);
    }
  } else if (state_ == InventoryState::ExamineView) {
    if (key == SDLK_RETURN || key == SDLK_e || key == SDLK_SPACE || key == SDLK_KP_ENTER || key == SDLK_ESCAPE) {
      state_ = InventoryState::Browsing;
    }
  }
}

void InventoryUI::handle_pad_button(uint8_t button, ScenarioState &state) {
  if (button == SDL_CONTROLLER_BUTTON_Y) {
    toggle();
    return;
  }
  if (!open_) return;

  size_t cap = InventoryEngine::get_capacity(state);

  if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER && state_ == InventoryState::Browsing) {
    cursor_ = cursor_ == InventoryEngine::kExtraSlot ? 0 : InventoryEngine::kExtraSlot;
    return;
  }
  if (button == SDL_CONTROLLER_BUTTON_B) {
    cancel_action();
    return;
  }

  if (button == SDL_CONTROLLER_BUTTON_A) {
    select_current(state);
    return;
  }

  if (state_ == InventoryState::CommandMenu) {
    if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
      if (!command_options_.empty()) {
        command_cursor_ = (command_cursor_ + command_options_.size() - 1) % command_options_.size();
      }
    } else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
      if (!command_options_.empty()) {
        command_cursor_ = (command_cursor_ + 1) % command_options_.size();
      }
    }
  } else if (state_ == InventoryState::Browsing || state_ == InventoryState::CombineSelect) {
    if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) navigate_grid(0, -1, cap);
    else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) navigate_grid(0, 1, cap);
    else if (button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) navigate_grid(-1, 0, cap);
    else if (button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) navigate_grid(1, 0, cap);
  }
}

void InventoryUI::tick([[maybe_unused]] uint16_t pad, ScenarioState &state) {
  anim_frame_++;
  if (message_timer_ > 0) {
    if (--message_timer_ == 0) {
      notice_message_.clear();
    }
  }

  bool is_poisoned = (state.player_state_flags & 0x280000u) != 0;
  float speed = 0.015f;
  if (is_poisoned) {
    speed = 0.022f;
  } else if (state.player_health < 30) {
    speed = 0.038f;
  } else if (state.player_health < 120) {
    speed = 0.025f;
  }

  ecg_phase_ += speed;
  if (ecg_phase_ >= 1.0f) {
    ecg_phase_ -= 1.0f;
  }
}

void InventoryUI::render(SDL_Renderer *renderer, Text &text,
                         const ScenarioState &state, int width, int height) {
  if (!open_) return;
  tick(0, const_cast<ScenarioState &>(state));
  if (!renderer) return;

  ensure_textures(renderer);

  render_authentic(renderer, text, state, width, height);

}

void InventoryUI::render_authentic(SDL_Renderer *renderer, Text &text,
                                  const ScenarioState &state, int width, int height) {
  (void)width; (void)height;
  int old_w=0, old_h=0; SDL_RenderGetLogicalSize(renderer,&old_w,&old_h); SDL_RenderSetLogicalSize(renderer,640,480);
  const SDL_Color ivory{207,198,151,255}, edge{126,117,77,255}, blue{8,15,92,255}, white{225,225,216,255}, green{63,209,69,255};
  auto fill=[&](int x,int y,int w,int h,SDL_Color c){ SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a); SDL_Rect r{x,y,w,h}; SDL_RenderFillRect(renderer,&r); };
  auto frame=[&](int x,int y,int w,int h){ fill(x,y,w,h,{25,24,20,255}); for(int i=0;i<5;++i){ SDL_SetRenderDrawColor(renderer,i%2?edge.r:ivory.r,i%2?edge.g:ivory.g,i%2?edge.b:ivory.b,255); SDL_Rect r{x+i,y+i,w-2*i,h-2*i}; SDL_RenderDrawRect(renderer,&r); } };
  fill(0,0,640,480,{10,12,13,255}); SDL_SetRenderDrawColor(renderer,42,43,41,255);
  for(int k=-480;k<640;k+=38){ SDL_RenderDrawLine(renderer,k,0,k+480,480); SDL_RenderDrawLine(renderer,k,480,k+480,0); }
  frame(56,42,262,48); const char *tabs[]={"EXIT","FILE","MAP","ITEM"};
  for(int i=0;i<4;++i){ int x=64+i*62; fill(x,49,57,33,{31,29,24,255}); frame(x,49,57,33); text.draw(tabs[i],x+5,55,16,i==3?SDL_Color{221,103,55,255}:ivory,true,true); }
  frame(333,42,148,64); fill(340,49,134,43,blue); frame(500,42,87,64); fill(507,49,73,43,blue);
  if(const auto eq=InventoryEngine::equipped_slot(state)){ auto v=InventoryEngine::get_slot(state,*eq); auto id=InventoryEngine::get_item_id(v); auto sp=get_item_sprite(id); SDL_Texture*tx=subtx_[sp.tex_id]; if(tx){SDL_Rect src{sp.x,sp.y,sp.width,sp.height},dst{350,52,90,33};SDL_RenderCopy(renderer,tx,&src,&dst);} text.draw(std::to_string(InventoryEngine::get_quantity(v)),344,78,12,white); }
  text.draw("EQUIPMENT",345,91,11,ivory,true,true); text.draw("STANDARD",510,91,11,ivory,true,true);
  frame(56,108,358,244); fill(67,119,336,222,{3,5,9,255}); fill(67,119,89,21,ivory); text.draw("STATUS",73,122,13,{28,35,44,255},true,true);
  fill(74,146,95,91,{17,11,31,255}); if(subtx_[2]){SDL_Rect src{state.player_id==1?88:0,0,88,88},dst{77,149,89,85};SDL_RenderCopy(renderer,subtx_[2],&src,&dst);}
  text.draw(state.player_id==1?"CHRIS":"CLAIRE",79,240,14,white,true,true);
  fill(178,143,125,73,{12,24,49,255}); frame(178,143,125,73); text.draw("DATA",184,146,10,{97,174,187,255},true,true);
  text.draw(state.player_id==1?"CHRIS REDFIELD":"CLAIRE REDFIELD",184,159,11,white,true,true); text.draw("HEIGHT     5.6 ft",184,174,10,white); text.draw("WEIGHT    115 lb",184,186,10,white); text.draw("BLOOD TYPE  O",184,198,10,white);
  fill(309,143,84,73,{4,15,43,255}); if(subtx_[2]){SDL_Rect src{0,88,140,60},dst{313,148,75,64};SDL_RenderCopy(renderer,subtx_[2],&src,&dst);}
  fill(178,221,215,26,{5,15,26,255}); frame(178,221,215,26); text.draw("CONDITION",183,222,10,{88,168,179,255},true,true);
  const bool poisoned=(state.player_state_flags&0x280000u)!=0; SDL_Color c=poisoned?SDL_Color{208,55,219,255}:state.player_health<30?SDL_Color{220,43,36,255}:state.player_health<120?SDL_Color{224,182,38,255}:green;
  fill(183,235,205,52,{2,8,15,255}); SDL_SetRenderDrawColor(renderer,27,50,58,255); for(int x=193;x<383;x+=20)SDL_RenderDrawLine(renderer,x,237,x,285); for(int y=245;y<285;y+=12)SDL_RenderDrawLine(renderer,184,y,386,y);
  SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,255); int px=184,py=261; for(int x=184;x<385;x+=2){float t=std::fmod(float(x-184)/52.0f-ecg_phase_,1.0f);if(t<0)t+=1;int y=261-static_cast<int>(ecg_waveform(t)*15);SDL_RenderDrawLine(renderer,px,py,x,y);px=x;py=y;}
  text.draw(InventoryEngine::get_condition_string(state.player_health,poisoned),338,269,16,c,true,true);
  fill(75,280,91,57,{49,5,8,255}); frame(75,280,91,57); text.draw("ID CARD",86,284,9,{235,170,159,255},true,true); text.draw("ASHFORD COUNTY",80,299,7,{215,151,146,255}); text.draw("SPECIAL TACTICS",80,309,7,{215,151,146,255});
  frame(430,108,157,262); fill(440,118,137,235,blue); size_t capacity=InventoryEngine::get_capacity(state);
  for(size_t i=0;i<capacity;++i){int col=int(i)%2,row=int(i)/2,x=444+col*68,y=122+row*45; auto v=InventoryEngine::get_slot(state,i);auto id=InventoryEngine::get_item_id(v);bool selected=cursor_==i,source=state_==InventoryState::CombineSelect&&combine_source_==i;
    if(selected||source){SDL_SetRenderDrawColor(renderer,source?90:220,source?211:41,source?223:38,255);SDL_Rect r{x,y,64,42};SDL_RenderDrawRect(renderer,&r);}
    if(id){auto sp=get_item_sprite(id);SDL_Texture*tx=subtx_[sp.tex_id];if(tx){SDL_Rect src{sp.x,sp.y,sp.width,sp.height},dst{x+2,y+2,sp.slots==2?59:43,30};SDL_RenderCopy(renderer,tx,&src,&dst);}if(ItemDatabase::is_ammo(id)||ItemDatabase::is_weapon(id))text.draw(std::to_string(InventoryEngine::get_quantity(v)),x+2,y+27,11,white);if(InventoryEngine::is_equipped(state,i))text.draw("E",x+52,y+28,9,{232,211,130,255});}}
  fill(442,347,133,21,ivory);text.draw("ITEM LIST",474,350,12,{36,33,25,255},true,true);
  frame(56,362,358,90);fill(66,372,338,70,{10,8,7,255});uint16_t id=InventoryEngine::get_item_id(InventoryEngine::get_slot(state,cursor_));if(id)text.draw(ItemDatabase::get_name(id),75,379,22,white,true,true);
  if(state_==InventoryState::CommandMenu&&!command_options_.empty()){int x=cursor_==InventoryEngine::kExtraSlot?260:445+int(cursor_%2)*67,y=cursor_==InventoryEngine::kExtraSlot?310:122+int(cursor_/2)*45;fill(x,y,95,int(command_options_.size())*17+8,{15,14,13,245});frame(x,y,95,int(command_options_.size())*17+8);for(size_t i=0;i<command_options_.size();++i)text.draw(command_options_[i],x+6,y+4+int(i)*17,10,i==command_cursor_?SDL_Color{235,166,62,255}:white,true,true);}
  SDL_RenderSetLogicalSize(renderer,old_w,old_h);
}
void InventoryUI::render_fallback(SDL_Renderer *renderer, Text &text,
                                  const ScenarioState &state, int width, int height) {
  render_authentic(renderer, text, state, width, height);
}
} // namespace ashford
