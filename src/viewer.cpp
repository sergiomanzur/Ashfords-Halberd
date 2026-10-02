#include "viewer.hpp"
#include <fstream>
#include <stdexcept>
namespace ashford {
namespace {
Bytes read(std::ifstream &f, uint64_t offset, size_t size) {
  if (size > 64 * 1024 * 1024)
    throw std::runtime_error("Room archive read limit exceeded");
  Bytes b(size);
  f.seekg(std::streamoff(offset));
  if (!f.read(reinterpret_cast<char *>(b.data()), std::streamsize(size)))
    throw std::runtime_error("Could not read room archive");
  return b;
}
} // namespace
RoomViewer::RoomViewer(SDL_Renderer *renderer, const fs::path &archive)
    : renderer_(renderer), archive_(archive) {
  std::ifstream f(archive_, std::ios::binary);
  if (!f)
    throw std::runtime_error("Could not open room archive");
  auto header = read(f, 0, 8);
  uint32_t count = uint32_t(header[4]) | uint32_t(header[5]) << 8 |
                   uint32_t(header[6]) << 16 | uint32_t(header[7]) << 24;
  if (count > 10000)
    throw std::runtime_error("Invalid room archive count");
  entries_ =
      afs_index(read(f, 0, 8 + size_t(count) * 8), fs::file_size(archive_));
}
void RoomViewer::select(Settings &settings) {
  if (settings.viewer_room < 0 ||
      size_t(settings.viewer_room) >= entries_.size())
    settings.viewer_room = 0;
  Scene pending_scene;
  std::vector<RoomCut> pending_cuts;
  const Scene *scene = &scene_;
  const std::vector<RoomCut> *cuts = &cuts_;
  if (loaded_ != settings.viewer_room) {
    std::ifstream f(archive_, std::ios::binary);
    auto entry = entries_.at(size_t(settings.viewer_room));
    auto room = expand(read(f, entry.offset, entry.size));
    pending_scene = decode_room_scene(room);
    pending_cuts = decode_room_cuts(room);
    scene = &pending_scene;
    cuts = &pending_cuts;
  }
  if (settings.viewer_cut < 0 || size_t(settings.viewer_cut) >= cuts->size())
    settings.viewer_cut = 0;
  if (settings.viewer_variant < 0 || settings.viewer_variant > 1)
    settings.viewer_variant = 0;
  RasterImage image;
  bool authored = false;
  if (!cuts->empty()) {
    Camera camera;
    try {
      camera = resolve_camera((*cuts)[size_t(settings.viewer_cut)],
                              size_t(settings.viewer_variant));
      authored = true;
    } catch (const std::runtime_error
                 &) { /* Unused variants may have no valid perspective. */
    }
    image = authored ? render_camera(*scene, camera, 640, 480)
                     : render_overview(*scene, 640, 480);
  } else
    image = render_overview(*scene, 640, 480);
  std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> next(
      SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24,
                        SDL_TEXTUREACCESS_STREAMING, 640, 480),
      SDL_DestroyTexture);
  if (!next ||
      SDL_UpdateTexture(next.get(), nullptr, image.rgb.data(), 640 * 3) < 0)
    throw std::runtime_error(SDL_GetError());
  if (loaded_ != settings.viewer_room) {
    scene_ = std::move(pending_scene);
    cuts_ = std::move(pending_cuts);
    loaded_ = settings.viewer_room;
  }
  authored_ = authored;
  texture_ = std::move(next);
}
void RoomViewer::draw(const SDL_Rect &destination) const {
  if (!texture_ ||
      SDL_RenderCopy(renderer_, texture_.get(), nullptr, &destination) < 0)
    throw std::runtime_error(SDL_GetError());
}
} // namespace ashford
