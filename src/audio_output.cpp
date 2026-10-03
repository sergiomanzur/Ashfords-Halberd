#include "audio_output.hpp"
#include <stdexcept>
namespace ashford {
SdlAudioOutput::SdlAudioOutput() {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    throw std::runtime_error(SDL_GetError());
  try {
    SDL_AudioSpec desired{};
    desired.freq = sound_output_rate;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 512;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &format_, 0);
    if (!device_)
      throw std::runtime_error(SDL_GetError());
    if (format_.freq != int(sound_output_rate) ||
        format_.format != AUDIO_S16SYS || format_.channels != 2)
      throw std::runtime_error("Native stereo PCM device format unavailable");
  } catch (...) {
    if (device_)
      SDL_CloseAudioDevice(device_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    throw;
  }
}
SdlAudioOutput::~SdlAudioOutput() {
  SDL_CloseAudioDevice(device_);
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
size_t SdlAudioOutput::queued_frames() const {
  return SDL_GetQueuedAudioSize(device_) / 4;
}
void SdlAudioOutput::queue(const std::vector<StereoPcmFrame> &frames) {
  constexpr size_t limit = sound_output_rate / 2;
  if (frames.size() > limit || queued_frames() > limit - frames.size())
    throw std::runtime_error("Native audio queue exceeds half-second backlog");
  std::vector<int16_t> pcm;
  pcm.reserve(frames.size() * 2);
  for (const auto &frame : frames) {
    pcm.push_back(frame.left);
    pcm.push_back(frame.right);
  }
  if (!pcm.empty() &&
      SDL_QueueAudio(device_, pcm.data(), Uint32(pcm.size() * 2)) < 0)
    throw std::runtime_error(SDL_GetError());
  submitted_frames_ += frames.size();
}
void SdlAudioOutput::pause(bool paused) {
  SDL_PauseAudioDevice(device_, paused ? 1 : 0);
  if (paused)
    SDL_ClearQueuedAudio(device_);
}
} // namespace ashford
