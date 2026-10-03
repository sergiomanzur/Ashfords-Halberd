#include "audio_runtime.hpp"
#include <stdexcept>
namespace ashford {
RoomAudioPlayback::RoomAudioPlayback(
    std::shared_ptr<const RoomSoundAssets> assets)
    : assets_(std::move(assets)) {
  if (!assets_ || !assets_->has_environment)
    throw std::runtime_error(
        "Native room audio requires bound sound environment");
  depth_.request(assets_->environment[9]);
}
void RoomAudioPlayback::apply(
    const std::vector<BackgroundSoundCommand> &commands) {
  if (failed_)
    throw std::runtime_error(
        "Failed native room playback requires reconstruction");
  if (commands.size() > 32)
    throw std::runtime_error("Native room command batch exceeds budget");
  struct Prepared {
    BackgroundSoundCommand command;
    std::unique_ptr<SceMixer> mixer;
  };
  std::vector<Prepared> prepared;
  prepared.reserve(commands.size());
  for (const auto &command : commands) {
    if (command.slot >= 2 || command.timer < 0)
      throw std::runtime_error("Invalid native room sound slot/timer");
    Prepared next{command, nullptr};
    if (command.kind == BackgroundCommandKind::play) {
      const auto bank = (uint32_t(command.code) >> 8) & 15;
      const auto sequence = uint32_t(command.code) & 255;
      if (bank != 3 || !assets_->programs[bank] || !assets_->sequences[bank] ||
          sequence == 255 || sequence >= assets_->sequences[bank]->size() ||
          !assets_->sequences[bank]->at(sequence))
        throw std::runtime_error(
            "Missing native room background bank/sequence");
      next.mixer = std::make_unique<SceMixer>(
          assets_->programs[bank], *assets_->sequences[bank]->at(sequence));
    } else if (command.kind != BackgroundCommandKind::fade_out) {
      throw std::runtime_error("Invalid native room sound action");
    }
    prepared.push_back(std::move(next));
  }
  // All commands/constructors validated before touching the live owners.
  for (auto &next : prepared) {
    auto &slot = slots_[next.command.slot];
    if (next.command.kind == BackgroundCommandKind::play) {
      slot.mixer = std::move(next.mixer);
      slot.fade = MidiVolumeFade{};
      if (next.command.timer)
        slot.fade.request(true, next.command.timer);
    } else if (!next.command.timer)
      slot.mixer.reset();
    else if (slot.mixer)
      slot.fade.request(false, next.command.timer);
  }
}
std::vector<StereoPcmFrame> RoomAudioPlayback::tick(int effects_percent) {
  if (failed_)
    throw std::runtime_error(
        "Failed native room playback requires reconstruction");
  if (effects_percent < 0 || effects_percent > 100)
    throw std::runtime_error("Invalid native effects volume");
  try {
    std::vector<SoundBusFrame> combined(800);
    size_t peak_bound = 0;
    for (auto &slot : slots_) {
      slot.fade.tick();
      if (slot.fade.stopped())
        slot.mixer.reset();
      if (!slot.mixer)
        continue;
      const auto buses = slot.mixer->render(800);
      peak_bound += slot.mixer->peak_voices();
      const int volume = slot.fade.driver_volume();
      auto scale = [&](int32_t value) {
        return int32_t(int64_t(value) * volume / 127);
      };
      for (size_t i = 0; i < 800; ++i) {
        combined[i].dry_left += scale(buses[i].dry_left);
        combined[i].dry_right += scale(buses[i].dry_right);
        combined[i].effect_left += scale(buses[i].effect_left);
        combined[i].effect_right += scale(buses[i].effect_right);
      }
    }
    // Conservative shared48-voice bound; selected original slots peak4+7.
    // Original voice stealing/global scheduling still require their owner.
    if (peak_bound > sound_voice_limit)
      throw std::runtime_error("Native ambient shared voice budget exceeded");
    auto output = hall_.render(combined, depth_.tick());
    for (auto &frame : output) {
      frame.left = int16_t(int32_t(frame.left) * effects_percent / 100);
      frame.right = int16_t(int32_t(frame.right) * effects_percent / 100);
    }
    rendered_frames_ += 800;
    return output;
  } catch (...) {
    stop();
    failed_ = true;
    throw;
  }
}
size_t RoomAudioPlayback::active_slots() const {
  return size_t(bool(slots_[0].mixer)) + size_t(bool(slots_[1].mixer));
}
void RoomAudioPlayback::stop() {
  for (auto &slot : slots_) {
    slot.mixer.reset();
    slot.fade = MidiVolumeFade{};
  }
  hall_.reset();
}
} // namespace ashford
