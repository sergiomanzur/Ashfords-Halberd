#pragma once
#include "audio_reverb.hpp"
#include <SDL.h>
namespace ashford {
class SdlAudioOutput {
public:
  SdlAudioOutput();
  ~SdlAudioOutput();
  SdlAudioOutput(const SdlAudioOutput &) = delete;
  SdlAudioOutput &operator=(const SdlAudioOutput &) = delete;
  void queue(const std::vector<StereoPcmFrame> &frames);
  void pause(bool paused);
  size_t queued_frames() const;
  uint64_t submitted_frames() const { return submitted_frames_; }
  const SDL_AudioSpec &format() const { return format_; }

private:
  SDL_AudioDeviceID device_ = 0;
  SDL_AudioSpec format_{};
  uint64_t submitted_frames_ = 0;
};
} // namespace ashford
