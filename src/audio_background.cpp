#include "audio_background.hpp"
#include <stdexcept>
#include <utility>
namespace ashford {
namespace {
void slot_range(uint8_t slot) {
  if (slot >= 2)
    throw std::runtime_error("Background sound slot outside native pair");
}
void append(BackgroundAudioState &state, BackgroundSoundCommand command) {
  if (state.commands.size() >= 32)
    throw std::runtime_error("Native background sound command budget exceeded");
  state.commands.push_back(command);
}
} // namespace
// Pinned MIT sdfunc.c::CallBackGroundSe2/StopBackGroundSe2 semantics.
void request_background_sound(BackgroundAudioState &state, uint8_t slot,
                              uint16_t code) {
  slot_range(slot);
  if (state.current[slot] != code) {
    state.request[slot] |= 2;
    state.current[slot] = code;
  }
}
void request_background_stop(BackgroundAudioState &state, uint8_t slot) {
  slot_range(slot);
  state.request[slot] = 1;
  state.current[slot] = -1;
}
void background_sound_command(BackgroundAudioState &state, uint8_t command) {
  if (command == 3) {
    state.initializing = true;
    return;
  }
  if (command == 4)
    return; // Matching Com_FinishInitScript does nothing.
  if (command != 1 && command != 2)
    throw std::runtime_error("Unsupported native background sound command");
  auto next = state;
  if (command == 1) {
    next.initializing = false;
    for (uint8_t slot = 0; slot < 2; ++slot) {
      if (next.request[slot] & 2) {
        const auto forced =
            int32_t((uint32_t(next.current[slot]) & 0xffff00ffu) | 0x300u);
        append(next, {BackgroundCommandKind::play, slot, forced, 120});
        next.current[slot] = forced;
      }
      next.request[slot] = 0;
    }
  } else {
    for (uint8_t slot = 0; slot < 2; ++slot)
      if (next.request[slot] & 1)
        append(next, {BackgroundCommandKind::fade_out, slot, -1, 400});
  }
  state = std::move(next);
}
} // namespace ashford
