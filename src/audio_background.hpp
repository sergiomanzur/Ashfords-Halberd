#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace ashford {
enum class BackgroundCommandKind { play, fade_out };
struct BackgroundSoundCommand {
  BackgroundCommandKind kind = BackgroundCommandKind::play;
  uint8_t slot = 0;
  int32_t code = -1;
  int16_t timer = 0;
};
struct BackgroundAudioState {
  std::array<int32_t, 2> current{-1, -1};
  std::array<uint8_t, 2> request{};
  bool initializing = false;
  std::vector<BackgroundSoundCommand> commands;
};
void request_background_sound(BackgroundAudioState &state, uint8_t slot,
                              uint16_t code);
void request_background_stop(BackgroundAudioState &state, uint8_t slot);
// Ambient part of source commands1..4. Command4 is matching empty behavior.
// Source enemy/object/ADX/door owners remain separate, explicit services.
void background_sound_command(BackgroundAudioState &state, uint8_t command);
} // namespace ashford
