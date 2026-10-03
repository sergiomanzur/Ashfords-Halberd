#pragma once
#include "audio_playback.hpp"
namespace ashford {
struct StereoPcmFrame {
  int16_t left = 0, right = 0;
  bool operator==(const StereoPcmFrame &other) const {
    return left == other.left && right == other.right;
  }
};
// Hardware39-tap coefficients; interpolation compensates for zero insertion.
class SpuReverbFir {
public:
  explicit SpuReverbFir(bool interpolation = false)
      : interpolation_(interpolation) {}
  int16_t tick(int16_t sample);
  void reset();

private:
  std::array<int16_t, 39> history_{};
  size_t cursor_ = 0;
  bool interpolation_;
};
// Independently expressed native Hall candidate, not verified PS2 parity.
// Depth scales only output; zero depth still advances feedback and filters.
class HallReverb {
public:
  std::vector<StereoPcmFrame> render(const std::vector<SoundBusFrame> &input,
                                     int16_t depth);
  void reset();

private:
  StereoPcmFrame half_rate(int16_t left, int16_t right);
  int16_t read(int offset) const;
  void write(int offset, int32_t value);
  std::array<int16_t, 22256> buffer_{};
  size_t cursor_ = 0;
  bool half_tick_ = true;
  SpuReverbFir down_left_, down_right_, up_left_{true}, up_right_{true};
};
} // namespace ashford
