#include "runtime_view.hpp"
#include "render.hpp"
#include <fstream>
#include <stdexcept>
namespace ashford {
namespace {
Bytes archive_entry(const fs::path &path, size_t index, bool compressed) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("Could not open native runtime archive");
  auto read = [&](uint64_t offset, size_t size) {
    if (size > 64 * 1024 * 1024)
      throw std::runtime_error("Runtime archive extent exceeds limit");
    Bytes bytes(size);
    file.seekg(std::streamoff(offset));
    if (!file.read(reinterpret_cast<char *>(bytes.data()),
                   std::streamsize(size)))
      throw std::runtime_error("Could not read native runtime archive");
    return bytes;
  };
  auto header = read(0, 8);
  size_t count = uint32_t(header[4]) | uint32_t(header[5]) << 8 |
                 uint32_t(header[6]) << 16 | uint32_t(header[7]) << 24;
  if (count > 10000)
    throw std::runtime_error("Invalid native runtime archive count");
  auto entries = afs_index(read(0, 8 + count * 8), fs::file_size(path));
  if (index >= entries.size())
    throw std::runtime_error("Missing native runtime archive entry");
  auto entry = entries[index];
  auto bytes = read(entry.offset, entry.size);
  return compressed ? expand(bytes) : bytes;
}
} // namespace
RuntimeView::RuntimeView(SDL_Renderer *renderer, const fs::path &rooms,
                         const fs::path &system)
    : renderer_(renderer) {
  player_ = decode_player_package(archive_entry(system, 10, false));
  translations_ = decode_player_translation_tables(player_.auxiliary);
  runtime_ = load_room_runtime(archive_entry(rooms, 1, true), 0, 1, 0,
                               ScenarioState{}, player_);
  tick(0);
  texture_.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24,
                                   SDL_TEXTUREACCESS_STREAMING, 320, 240));
  if (!texture_)
    throw std::runtime_error(SDL_GetError());
}
void RuntimeView::tick(uint16_t pad) {
  tick_room_runtime(runtime_, player_, translations_, pad);
  ++ticks_;
}
void RuntimeView::draw(const SDL_Rect &destination) {
  if (!runtime_.world.cut)
    throw std::runtime_error("Native runtime has no active original camera");
  auto image = render_camera(
      compose_room_runtime(runtime_, runtime_.scenario.drawing_flags, 0),
      resolve_camera(runtime_.cuts.at(*runtime_.world.cut), 0), 320, 240);
  if (SDL_UpdateTexture(texture_.get(), nullptr, image.rgb.data(), 320 * 3) <
          0 ||
      SDL_RenderCopy(renderer_, texture_.get(), nullptr, &destination) < 0)
    throw std::runtime_error(SDL_GetError());
}
} // namespace ashford
