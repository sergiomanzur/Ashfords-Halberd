#include "audio_output.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
int main() {
  try {
    using namespace ashford;
    if (!SDL_getenv("SDL_AUDIODRIVER") ||
        std::string(SDL_getenv("SDL_AUDIODRIVER")) != "dummy")
      throw std::runtime_error("Audio verification requires dummy driver");
    SDL_SetMainReady();
    {
      SdlAudioOutput output;
      if (std::string(SDL_GetCurrentAudioDriver()) != "dummy" ||
          output.format().freq != 48000 || output.format().channels != 2 ||
          output.format().format != AUDIO_S16SYS)
        throw std::runtime_error("Incorrect native stereo PCM output format");
      output.queue(std::vector<StereoPcmFrame>(800, {1234, -5678}));
      if (output.queued_frames() != 800 || output.submitted_frames() != 800)
        throw std::runtime_error("Native PCM byte/frame accounting");
      output.pause(true);
      if (output.queued_frames() != 0)
        throw std::runtime_error("Pause must clear queued PCM");
      output.queue(std::vector<StereoPcmFrame>(24000));
      bool rejected = false;
      try {
        output.queue({{1, 2}});
      } catch (const std::runtime_error &) {
        rejected = true;
      }
      if (!rejected || output.queued_frames() != 24000)
        throw std::runtime_error(
            "Audio backlog must reject before queue mutation");
      output.pause(false);
      const auto deadline = SDL_GetTicks64() + 500;
      while (output.queued_frames() == 24000 && SDL_GetTicks64() < deadline)
        SDL_Delay(10);
      if (output.queued_frames() >= 24000)
        throw std::runtime_error("Dummy driver did not consume PCM");
      output.pause(true);
      if (output.queued_frames())
        throw std::runtime_error("Menu/focus pause left queued PCM");
    }
    if (SDL_WasInit(SDL_INIT_AUDIO))
      throw std::runtime_error("Audio subsystem ownership leaked");
    std::cout << "Headless native SDL audio output passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
