#include "audio_reverb.hpp"
#include <algorithm>
#include <stdexcept>
namespace ashford {
namespace {
// Mathematical register/FIR data from pinned primary SPU documentation.
// PS2 Hall mode5/address mapping confirmed against open ps2sdk
// ac92a9f657d2e531dd8f060250b07f2a5ac6dea5. No SDK/emulator code is used.
constexpr std::array<int16_t, 39> fir{
    -1,   0, 2,     0, -10,   0,     35,    0, -103,  0, 266,  0, -616, 0,
    1332, 0, -2960, 0, 10246, 16384, 10246, 0, -2960, 0, 1332, 0, -616, 0,
    266,  0, -103,  0, 35,    0,     -10,   0, 2,     0, -1};
constexpr std::array<uint16_t, 32> hall{
    0x01a5, 0x0139, 0x6000, 0x5000, 0x4c00, 0xb800, 0xbc00, 0xc000,
    0x6000, 0x5c00, 0x15ba, 0x11bb, 0x14c2, 0x10bd, 0x11bc, 0x0dc1,
    0x11c0, 0x0dc3, 0x0dc0, 0x09c1, 0x0bc4, 0x07c1, 0x0a00, 0x06cd,
    0x09c2, 0x05c1, 0x05c0, 0x041a, 0x0274, 0x013a, 0x8000, 0x8000};
int coefficient(size_t index) {
  return hall[index] >= 0x8000 ? int(hall[index]) - 65536 : int(hall[index]);
}
int offset(size_t index) { return int(hall[index]) * 4; }
int32_t floor_q15(int64_t value) {
  return int32_t(value >= 0 ? value / 32768 : -((-value + 32767) / 32768));
}
int32_t multiply(int32_t value, int32_t gain) {
  return floor_q15(int64_t(value) * gain);
}
int16_t clip(int64_t value) {
  return int16_t(std::clamp<int64_t>(value, -32768, 32767));
}
} // namespace
int16_t SpuReverbFir::tick(int16_t sample) {
  history_[cursor_] = sample;
  int64_t sum = 0;
  for (size_t i = 0; i < fir.size(); ++i)
    sum += int64_t(history_[(cursor_ + fir.size() - i) % fir.size()]) * fir[i];
  cursor_ = (cursor_ + 1) % fir.size();
  return clip(floor_q15(sum * (interpolation_ ? 2 : 1)));
}
void SpuReverbFir::reset() {
  history_.fill(0);
  cursor_ = 0;
}
int16_t HallReverb::read(int relative) const {
  const int length = int(buffer_.size());
  int at = (int(cursor_) + relative) % length;
  if (at < 0)
    at += length;
  return buffer_[size_t(at)];
}
void HallReverb::write(int relative, int32_t value) {
  const int length = int(buffer_.size());
  int at = (int(cursor_) + relative) % length;
  if (at < 0)
    at += length;
  buffer_[size_t(at)] = clip(value);
}
StereoPcmFrame HallReverb::half_rate(int16_t left, int16_t right) {
  const std::array<int32_t, 2> input{multiply(left, coefficient(30)),
                                     multiply(right, coefficient(31))};
  for (size_t channel = 0; channel < 2; ++channel) {
    for (bool different : {false, true}) {
      const int destination = offset((different ? 18 : 10) + channel);
      // The documented '-2' is bytes: one PCM16 word, not a register unit.
      const int32_t previous = read(destination - 1);
      const int source = offset(different ? 24 + (1 - channel) : 16 + channel);
      const int32_t reflection = multiply(read(source), coefficient(7));
      const int32_t value =
          multiply(input[channel] + reflection - previous, coefficient(2)) +
          previous;
      write(destination, value);
    }
  }
  StereoPcmFrame output;
  for (size_t channel = 0; channel < 2; ++channel) {
    int32_t sum = 0;
    size_t gain = 3;
    for (size_t tap : {size_t(12), size_t(14), size_t(20), size_t(22)})
      sum += multiply(read(offset(tap + channel)), coefficient(gain++));
    int32_t value = clip(sum);
    for (size_t stage = 0; stage < 2; ++stage) {
      const int destination = offset(26 + stage * 2 + channel);
      const int32_t delayed = read(destination - offset(stage));
      const int32_t allpass =
          clip(value - multiply(delayed, coefficient(8 + stage)));
      write(destination, allpass);
      value = clip(multiply(allpass, coefficient(8 + stage)) + delayed);
    }
    (channel == 0 ? output.left : output.right) = int16_t(value);
  }
  cursor_ = (cursor_ + 1) % buffer_.size();
  return output;
}
std::vector<StereoPcmFrame>
HallReverb::render(const std::vector<SoundBusFrame> &input, int16_t depth) {
  if (input.size() > sound_render_frame_limit)
    throw std::runtime_error("Native Hall block exceeds frame limit");
  std::vector<StereoPcmFrame> output;
  output.reserve(input.size());
  for (const auto &frame : input) {
    const auto left = down_left_.tick(clip(frame.effect_left));
    const auto right = down_right_.tick(clip(frame.effect_right));
    StereoPcmFrame wet;
    if (half_tick_)
      wet = half_rate(left, right);
    half_tick_ = !half_tick_;
    const int32_t wet_left = multiply(up_left_.tick(wet.left), depth);
    const int32_t wet_right = multiply(up_right_.tick(wet.right), depth);
    output.push_back({clip(int64_t(frame.dry_left) + wet_left),
                      clip(int64_t(frame.dry_right) + wet_right)});
  }
  return output;
}
void HallReverb::reset() {
  buffer_.fill(0);
  cursor_ = 0;
  half_tick_ = true;
  down_left_.reset();
  down_right_.reset();
  up_left_.reset();
  up_right_.reset();
}
} // namespace ashford
