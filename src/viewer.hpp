#pragma once
#include "core.hpp"
#include "render.hpp"
#include <SDL.h>
#include <memory>
namespace ashford {
class RoomViewer {
public:
  RoomViewer(SDL_Renderer *renderer, const fs::path &archive);
  void select(Settings &settings);
  void draw(const SDL_Rect &destination) const;
  size_t room_count() const { return entries_.size(); }
  size_t cut_count() const { return cuts_.size(); }
  bool authored() const { return authored_; }

private:
  SDL_Renderer *renderer_;
  fs::path archive_;
  std::vector<AfsEntry> entries_;
  Scene scene_;
  std::vector<RoomCut> cuts_;
  int loaded_ = -1;
  bool authored_ = false;
  std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture_{
      nullptr, SDL_DestroyTexture};
};
} // namespace ashford
