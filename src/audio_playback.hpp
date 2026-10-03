#pragma once
#include "audio_assets.hpp"
#include <array>
#include <memory>
namespace ashford {
constexpr uint32_t sound_output_rate = 48000;
constexpr uint32_t sound_render_frame_limit = 48000;
constexpr size_t sound_voice_limit = 48;
constexpr size_t sound_scheduled_event_limit = 65536;
struct ScheduledSceEvent {
  uint64_t frame = 0;
  size_t index = 0;
  SceEvent event;
  bool operator==(const ScheduledSceEvent &other) const;
};
class SceSequencer {
public:
  explicit SceSequencer(SceSequence sequence);
  std::vector<ScheduledSceEvent> advance(uint32_t frames);
  uint64_t frame() const { return frame_; }
  uint64_t loop_repetitions() const { return repetitions_; }

private:
  struct Loop {
    size_t target = 0;
    uint64_t tick = 0;
  };
  void schedule();
  SceSequence sequence_;
  std::vector<std::optional<Loop>> loops_;
  size_t cursor_ = 0;
  uint64_t frame_ = 0, due_ = 0, fraction_ = 0, last_tick_ = 0,
           repetitions_ = 0;
  uint64_t denominator_ = 0;
  uint32_t tempo_ = 500000;
  bool pending_ = true;
};
// Independent native envelope candidate; PS2 hardware parity remains unproven.
class SoundEnvelope {
public:
  SoundEnvelope(uint16_t adsr1, uint16_t adsr2);
  int32_t tick();
  void release();
  int32_t level() const { return level_; }
  bool active() const { return phase_ != 4; }

private:
  uint16_t adsr1_, adsr2_;
  uint8_t phase_ = 0;
  int32_t level_ = 0;
  uint32_t counter_ = 0;
};
int32_t gaussian_sample(const std::array<int16_t, 4> &history,
                        uint16_t fraction);
class SampleVoice {
public:
  SampleVoice(std::shared_ptr<const SceSoundBank> bank, size_t wave,
              uint16_t pitch, uint16_t adsr1, uint16_t adsr2);
  int32_t tick();
  void release() { envelope_.release(); }
  bool active() const { return !ended_ && envelope_.active(); }

private:
  int16_t sample(int64_t index) const;
  std::shared_ptr<const SceSoundBank> bank_;
  size_t wave_;
  uint16_t pitch_;
  uint64_t position_ = 0;
  SoundEnvelope envelope_;
  bool ended_ = false;
};
struct SoundBusFrame {
  int32_t dry_left = 0, dry_right = 0, effect_left = 0, effect_right = 0;
  bool operator==(const SoundBusFrame &other) const;
};
// Banks must remain immutable while shared by a mixer/voice. No device or IOP
// state is owned here. Effect sends require a subsequent native effects stage.
class SceMixer {
public:
  SceMixer(std::shared_ptr<const SceSoundBank> bank, SceSequence sequence);
  ~SceMixer();
  SceMixer(SceMixer &&) noexcept;
  SceMixer &operator=(SceMixer &&) noexcept;
  std::vector<SoundBusFrame> render(uint32_t frames);
  size_t active_voices() const;
  size_t peak_voices() const;
  uint64_t loop_repetitions() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace ashford
