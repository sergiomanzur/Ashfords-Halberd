#pragma once
#include "audio_background.hpp"
#include "audio_reverb.hpp"
#include "audio_room.hpp"
namespace ashford {
class RoomAudioPlayback {
public:
  explicit RoomAudioPlayback(std::shared_ptr<const RoomSoundAssets> assets);
  void apply(const std::vector<BackgroundSoundCommand> &commands);
  // Current native60Hz manager cadence: one tick produces800 stereo frames.
  std::vector<StereoPcmFrame> tick(int effects_percent);
  size_t active_slots() const;
  uint64_t rendered_frames() const { return rendered_frames_; }
  void stop();

private:
  struct Slot {
    std::unique_ptr<SceMixer> mixer;
    MidiVolumeFade fade;
  };
  std::shared_ptr<const RoomSoundAssets> assets_;
  std::array<Slot, 2> slots_;
  RoomEffectDepth depth_;
  HallReverb hall_;
  uint64_t rendered_frames_ = 0;
  bool failed_ = false;
};
} // namespace ashford
