#pragma once
#include "audio_assets.hpp"
#include <array>
#include <memory>
namespace ashford {
struct RoomSoundAssets {
  std::array<std::shared_ptr<const SceSoundBank>, 16> programs{}, shots{};
  std::array<std::shared_ptr<const std::vector<std::optional<SceSequence>>>, 16>
      sequences{};
  std::array<int8_t, 16> environment{};
  bool has_environment = false;
};
RoomSoundAssets
decode_room_sound_pack(const Bytes &data,
                       size_t pcm_budget = sound_pcm_frame_limit,
                       size_t event_budget = sound_sequence_event_limit);
// Matching RequestMidiFadeFunction/ExecSoundFadeManager tick arithmetic;
// caller schedules manager ticks separately from sample rendering.
class MidiVolumeFade {
public:
  void request(bool fade_in, int16_t timer, int16_t limit = 0);
  void tick();
  int driver_volume() const;
  int32_t remaining() const { return count_; }
  bool active() const { return active_; }
  bool stopped() const { return stopped_; }

private:
  float volume_ = 0, last_ = 0, speed_ = 0;
  int32_t count_ = 0;
  bool active_ = false, stopped_ = false;
};
class RoomEffectDepth {
public:
  void request(int8_t depth);
  int16_t tick();
  int16_t level() const { return int16_t(current_); }

private:
  int32_t current_ = 0, target_ = 0, step_ = 0;
  uint8_t remaining_ = 0;
};
} // namespace ashford
