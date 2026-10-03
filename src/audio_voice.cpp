#include "audio_gaussian.hpp"
#include "audio_playback.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace ashford {
namespace {
int32_t floor_div(int64_t value, int32_t denominator) {
  return int32_t(value >= 0 ? value / denominator
                            : -((-value + denominator - 1) / denominator));
}
} // namespace
SoundEnvelope::SoundEnvelope(uint16_t adsr1, uint16_t adsr2)
    : adsr1_(adsr1), adsr2_(adsr2) {
  if (adsr2 & 0x2000)
    throw std::runtime_error("Unsupported reserved ADSR bit");
}
void SoundEnvelope::release() {
  if (phase_ < 3) {
    phase_ = 3;
    counter_ = 0;
  }
}
int32_t SoundEnvelope::tick() {
  if (!active())
    return 0;
  const int32_t sustain = ((adsr1_ & 15) + 1) * 0x800;
  if (phase_ == 1 && level_ <= sustain) {
    phase_ = 2;
    counter_ = 0;
  }
  unsigned shift = 0, step_bits = 0;
  bool decrease = false, exponential = false, frozen = false;
  if (phase_ == 0) {
    shift = (adsr1_ >> 10) & 31;
    step_bits = (adsr1_ >> 8) & 3;
    exponential = adsr1_ & 0x8000;
    frozen = shift == 31 && step_bits == 3;
  } else if (phase_ == 1) {
    shift = (adsr1_ >> 4) & 15;
    decrease = exponential = true;
  } else if (phase_ == 2) {
    shift = (adsr2_ >> 8) & 31;
    step_bits = (adsr2_ >> 6) & 3;
    decrease = adsr2_ & 0x4000;
    exponential = adsr2_ & 0x8000;
    frozen = shift == 31 && step_bits == 3;
  } else {
    shift = adsr2_ & 31;
    decrease = true;
    exponential = adsr2_ & 32;
    frozen = shift == 31;
  }
  int32_t step = decrease ? -8 + int32_t(step_bits) : 7 - int32_t(step_bits);
  step *= int32_t(1u << (shift < 11 ? 11 - shift : 0));
  uint32_t increment = 0x8000u >> (shift > 11 ? shift - 11 : 0);
  if (exponential && !decrease && level_ > 0x6000) {
    if (shift < 10)
      step /= 4;
    else if (shift == 10) {
      step /= 2;
      increment /= 2;
    } else
      increment /= 4;
  } else if (exponential && decrease)
    step = floor_div(int64_t(step) * level_, 0x8000);
  if (!frozen)
    increment = std::max(increment, 1u);
  counter_ += increment;
  if (counter_ >= 0x8000) {
    counter_ = 0;
    level_ = std::clamp(level_ + step, 0, 32767);
  }
  if (phase_ == 0 && level_ == 32767) {
    phase_ = 1;
    counter_ = 0;
  } else if (phase_ == 1 && level_ <= sustain) {
    phase_ = 2;
    counter_ = 0;
  } else if (phase_ == 3 && level_ == 0)
    phase_ = 4;
  return level_;
}
int32_t gaussian_sample(const std::array<int16_t, 4> &h, uint16_t fraction) {
  if (fraction >= 4096)
    throw std::runtime_error("Invalid Gaussian interpolation fraction");
  const size_t i = fraction >> 4;
  return floor_div(int64_t(h[0]) * audio_detail::gaussian[255 - i], 32768) +
         floor_div(int64_t(h[1]) * audio_detail::gaussian[511 - i], 32768) +
         floor_div(int64_t(h[2]) * audio_detail::gaussian[256 + i], 32768) +
         floor_div(int64_t(h[3]) * audio_detail::gaussian[i], 32768);
}
SampleVoice::SampleVoice(std::shared_ptr<const SceSoundBank> bank, size_t wave,
                         uint16_t pitch, uint16_t adsr1, uint16_t adsr2)
    : bank_(std::move(bank)), wave_(wave), pitch_(pitch),
      envelope_(adsr1, adsr2) {
  if (!bank_ || wave >= bank_->waves.size() || !pitch || pitch > 16384)
    throw std::runtime_error("Invalid native sample voice");
  const auto &w = bank_->waves[wave];
  if (!w.sample_rate || w.attribute > 1 || w.sample.pcm.empty() ||
      w.sample.pcm.size() > sound_pcm_frame_limit ||
      (w.attribute == 1 && !w.sample.loop) ||
      (w.sample.loop && (w.sample.loop->first >= w.sample.loop->second ||
                         w.sample.loop->second > w.sample.pcm.size())))
    throw std::runtime_error("Invalid native sample wave/loop");
}
int16_t SampleVoice::sample(int64_t index) const {
  if (index < 0)
    return 0;
  const auto &w = bank_->waves[wave_];
  auto at = uint64_t(index);
  if (w.attribute == 1 && at >= w.sample.loop->second)
    at = w.sample.loop->first +
         (at - w.sample.loop->first) %
             (w.sample.loop->second - w.sample.loop->first);
  return at < w.sample.pcm.size() ? w.sample.pcm[size_t(at)] : 0;
}
int32_t SampleVoice::tick() {
  if (!active())
    return 0;
  const auto at = int64_t(position_ >> 12);
  const auto &wave = bank_->waves[wave_];
  if (!wave.attribute && uint64_t(at) >= wave.sample.pcm.size() + 3) {
    ended_ = true;
    return 0;
  }
  const std::array<int16_t, 4> history = {sample(at - 3), sample(at - 2),
                                          sample(at - 1), sample(at)};
  const int32_t value =
      floor_div(int64_t(gaussian_sample(history, uint16_t(position_ & 4095))) *
                    envelope_.tick(),
                32768);
  if (position_ > std::numeric_limits<uint64_t>::max() - pitch_)
    throw std::runtime_error("Native sample position overflow");
  position_ += pitch_;
  return value;
}
} // namespace ashford
