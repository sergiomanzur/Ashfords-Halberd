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
                         const fs::path &system, int effects_volume)
    : renderer_(renderer) {
  player_ = decode_player_package(archive_entry(system, 10, false));
  translations_ = decode_player_translation_tables(player_.auxiliary);
  set_effects_volume(effects_volume);
  const auto sound_root = rooms.parent_path();
  runtime_ = load_room_runtime(
      archive_entry(rooms, 1, true), 0, 1, 0, ScenarioState{}, player_, nullptr,
      [&](uint8_t stage, uint8_t room, int32_t sound_case) {
        return read_room_sound_assets(sound_root / "MULTSPQ1.AFS",
                                      sound_root / "MULTSPQ1.IDX", stage, room,
                                      sound_case);
      });
  audio_ = std::make_unique<RoomAudioPlayback>(runtime_.sound_assets);
  audio_->apply(runtime_.scenario.background_audio.commands);
  runtime_.scenario.background_audio.commands.clear();
  tick(0);
  texture_.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24,
                                   SDL_TEXTUREACCESS_STREAMING, 320, 240));
  if (!texture_)
    throw std::runtime_error(SDL_GetError());
  set_audio_running(true);
}
void RuntimeView::tick(uint16_t pad) {
  try {
    tick_room_runtime(runtime_, player_, translations_, pad);
    audio_->apply(runtime_.scenario.background_audio.commands);
    runtime_.scenario.background_audio.commands.clear();
    const auto frames = audio_->tick(effects_volume_);
    if (audio_running_)
      output_.queue(frames);
    ++ticks_;
  } catch (...) {
    set_audio_running(false);
    audio_->stop();
    throw;
  }
}
void RuntimeView::set_audio_running(bool running) {
  output_.pause(!running);
  audio_running_ = running;
}
void RuntimeView::set_effects_volume(int volume) {
  if (volume < 0 || volume > 100)
    throw std::runtime_error("Invalid effects volume");
  effects_volume_ = volume;
}
void RuntimeView::draw(const SDL_Rect &destination) {
  if (!runtime_.world.cut)
    throw std::runtime_error("Native runtime has no active original camera");
  auto scene = compose_room_runtime(runtime_, runtime_.scenario.drawing_flags,
                                    runtime_.camera_flags);
  auto image = render_camera(
      scene, resolve_camera(runtime_.cuts.at(*runtime_.world.cut), 0), 320, 240,
      &ambient_material_, &runtime_.scenario.fog.active.enabled,
      &light_material_);
  if (scene.ending_light_work)
    runtime_.scenario.lights = std::move(scene.ending_light_work);
  if (scene.ending_lighting)
    runtime_.scenario.drawing_lighting = scene.ending_lighting;
  if (SDL_UpdateTexture(texture_.get(), nullptr, image.rgb.data(), 320 * 3) <
          0 ||
      SDL_RenderCopy(renderer_, texture_.get(), nullptr, &destination) < 0)
    throw std::runtime_error(SDL_GetError());
}
} // namespace ashford
