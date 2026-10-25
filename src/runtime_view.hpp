#pragma once
#include "audio_output.hpp"
#include "audio_runtime.hpp"
#include "core.hpp"
#include "runtime.hpp"
#include <SDL.h>
#include <memory>
namespace ashford {
class RuntimeView {
public:
  RuntimeView(SDL_Renderer *renderer, const fs::path &rooms,
              const fs::path &system, int effects_volume = 100);
  void tick(uint16_t pad);
  void draw(const SDL_Rect &destination);
  const RoomRuntime &state() const { return runtime_; }
  RoomRuntime &state() { return runtime_; }
  size_t ticks() const { return ticks_; }
  void set_audio_running(bool running);
  void set_effects_volume(int volume);
  uint64_t audio_frames() const { return audio_->rendered_frames(); }
  uint64_t audio_submitted_frames() const { return output_.submitted_frames(); }
  size_t audio_queue_frames() const { return output_.queued_frames(); }

  bool in_aim_mode() const { return aim_mode_; }
  bool last_shot_hit() const { return last_shot_hit_; }
  int32_t last_damage_dealt() const { return last_damage_dealt_; }

  void save(const fs::path &path);
  void load(const fs::path &path);

private:
  SDL_Renderer *renderer_;
  fs::path rooms_path_;
  fs::path system_path_;
  fs::path sound_root_;
  PlayerPackage player_;
  PlayerTranslationTables translations_;
  RoomRuntime runtime_;
  AmbientMaterial ambient_material_{255, 255, 255};
  LightMaterial light_material_;
  size_t ticks_ = 0;
  uint16_t previous_pad_ = 0;
  bool aim_mode_ = false;
  bool last_shot_hit_ = false;
  int32_t last_damage_dealt_ = 0;
  std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture_{
      nullptr, SDL_DestroyTexture};
  std::unique_ptr<RoomAudioPlayback> audio_;
  SdlAudioOutput output_;
  int effects_volume_ = 100;
  bool audio_running_ = false;
};
} // namespace ashford
