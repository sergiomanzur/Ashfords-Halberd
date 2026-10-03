#include "audio_reverb.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
void hall_tests() {
  using namespace ashford;
  // Primary SPU hardware FIR impulse, independent literal expected PCM.
  const std::vector<int16_t> expected{
      -1,   0,   1,   0,     -5,  0,    17,   0,    -52, 0,     133, 0,   -308,
      0,    666, 0,   -1480, 0,   5123, 8192, 5123, 0,   -1480, 0,   666, 0,
      -308, 0,   133, 0,     -52, 0,    17,   0,    -5,  0,     1,   0,   -1};
  SpuReverbFir down;
  for (size_t i = 0; i < expected.size(); ++i)
    check(down.tick(i == 0 ? 16384 : 0) == expected[i],
          "Independent39-tap FIR impulse");
  check(down.tick(0) == 0, "FIR finite impulse extent");
  down.reset();
  check(down.tick(16384) == -1, "FIR reset removes history");
  SpuReverbFir up(true);
  check(up.tick(16384) == -1, "Interpolation restores zero-insertion gain");
  for (int i = 1; i < 19; ++i)
    up.tick(0);
  check(up.tick(0) == 16384, "Interpolation center coefficient gain");
  HallReverb hall;
  auto dry = hall.render({{40000, -40000, 0, 0},
                          {std::numeric_limits<int32_t>::max(),
                           std::numeric_limits<int32_t>::min(), 0, 0},
                          {-1234, 4567, 0, 0}},
                         0);
  check(dry == std::vector<StereoPcmFrame>{{32767, -32768},
                                           {32767, -32768},
                                           {-1234, 4567}},
        "Dry output literal vectors clip safely without signed overflow");
  check(hall.render(std::vector<SoundBusFrame>(200), 4096) ==
            std::vector<StereoPcmFrame>(200),
        "Zero Hall state remains silent");
  std::vector<SoundBusFrame> input(48000);
  input[0].effect_left = 30000;
  hall.reset();
  const auto whole = hall.render(input, 4096);
  check(whole.front() == StereoPcmFrame{},
        "Hall impulse has an authored delay");
  check(std::any_of(whole.begin() + 1000, whole.end(),
                    [](auto f) { return f.left != 0; }),
        "Hall produces a delayed nonzero wet tail");
  check(std::any_of(whole.begin() + 1000, whole.end(),
                    [](auto f) { return f.right != 0; }),
        "Hall diffusion crosses into opposite channel");
  HallReverb partitioned;
  std::vector<StereoPcmFrame> divided;
  size_t at = 0;
  for (size_t n :
       {size_t(1), size_t(7), size_t(803), size_t(21999), size_t(25190)}) {
    auto block = partitioned.render(
        std::vector<SoundBusFrame>(input.begin() + at, input.begin() + at + n),
        4096);
    divided.insert(divided.end(), block.begin(), block.end());
    at += n;
  }
  check(divided == whole,
        "Hall PCM is exact across odd block boundaries and wraparound");
  hall.reset();
  check(hall.render(input, 4096) == whole,
        "Hall reset restores fresh deterministic PCM");
  HallReverb muted, reference;
  std::vector<SoundBusFrame> prefix(input.begin(), input.begin() + 12001);
  check(muted.render(prefix, 0) == std::vector<StereoPcmFrame>(prefix.size()),
        "Authored zero depth emits no effect bus");
  reference.render(prefix, 4096);
  std::vector<SoundBusFrame> tail(35999);
  const auto preserved = muted.render(tail, 4096);
  check(preserved == reference.render(tail, 4096) &&
            std::any_of(preserved.begin(), preserved.end(),
                        [](auto f) { return f.left || f.right; }),
        "Zero depth keeps feedback and FIR history for a later room depth");
  std::vector<SoundBusFrame> stress(48000,
                                    {std::numeric_limits<int32_t>::max(),
                                     std::numeric_limits<int32_t>::min(),
                                     std::numeric_limits<int32_t>::min(),
                                     std::numeric_limits<int32_t>::max()});
  hall.render(stress, std::numeric_limits<int16_t>::min());
  bool rejected = false;
  try {
    hall.render(std::vector<SoundBusFrame>(48001), 4096);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "Hall rejects oversized block before mutating DSP state");
  check(hall.render({}, 0).empty(), "Empty Hall block retains state");
}
} // namespace
int main() {
  try {
    hall_tests();
    std::cout << "Native Hall audio checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
