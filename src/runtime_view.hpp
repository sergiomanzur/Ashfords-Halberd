#pragma once
#include "core.hpp"
#include "runtime.hpp"
#include <SDL.h>
#include <memory>
namespace ashford {
class RuntimeView {
public:
  RuntimeView(SDL_Renderer *renderer, const fs::path &rooms,
              const fs::path &system);
  void tick(uint16_t pad);
  void draw(const SDL_Rect &destination);
  const RoomRuntime &state() const { return runtime_; }
  size_t ticks() const { return ticks_; }

private:
  SDL_Renderer *renderer_;
  PlayerPackage player_;
  PlayerTranslationTables translations_;
  RoomRuntime runtime_;
  size_t ticks_ = 0;
  std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture_{
      nullptr, SDL_DestroyTexture};
};
} // namespace ashford
