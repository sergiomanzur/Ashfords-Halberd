#include "audio_assets.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
namespace ashford {
// Distinct native adaptation of the PS-ADPCM format described by vgmstream.
// Rational predictor evaluation; notices/provenance:
// docs/native-audio-status.md.
PsAdpcmSample decode_ps_adpcm(const Bytes &data, size_t frame_limit) {
  frame_limit = std::min(frame_limit, sound_pcm_frame_limit);
  if (data.empty() || data.size() % 16 || data.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Invalid ADPCM block extent");
  constexpr std::array<std::array<int, 2>, 5> coefficients{
      {{{0, 0}}, {{60, 0}}, {{115, -52}}, {{98, -55}}, {{122, -60}}}};
  auto floor64 = [](int64_t value) {
    return value >= 0 ? value / 64 : -((-value + 63) / 64);
  };
  PsAdpcmSample result;
  int64_t previous = 0, older = 0;
  std::optional<size_t> loop_start;
  for (size_t at = 0; at < data.size(); at += 16) {
    const unsigned predictor = data[at] >> 4, shift = data[at] & 15;
    const unsigned flags = data[at + 1];
    if (predictor > 4 || shift > 12 || flags > 7 || flags == 5)
      throw std::runtime_error("Unsupported ADPCM block header");
    if (flags == 7) {
      result.decoded_bytes = at;
      return result; // Transport filler is not an audible frame.
    }
    if (result.pcm.size() > frame_limit || frame_limit - result.pcm.size() < 28)
      throw std::runtime_error("ADPCM PCM expansion budget exceeded");
    if (flags & 4)
      loop_start = result.pcm.size();
    for (size_t i = 0; i < 28; ++i) {
      int nibble = (data[at + 2 + i / 2] >> ((i & 1) * 4)) & 15;
      if (nibble >= 8)
        nibble -= 16;
      const int64_t residual = int64_t(nibble) * (int64_t(1) << (12 - shift));
      const int64_t value =
          residual + floor64(previous * coefficients[predictor][0] +
                             older * coefficients[predictor][1]);
      if (value < std::numeric_limits<int32_t>::min() ||
          value > std::numeric_limits<int32_t>::max())
        throw std::runtime_error("ADPCM predictor history overflow");
      result.pcm.push_back(
          int16_t(std::clamp(value, int64_t(-32768), int64_t(32767))));
      older = previous;
      previous = value;
    }
    if (flags & 1) {
      result.decoded_bytes = at + 16;
      if (flags & 2)
        result.loop = std::make_pair(loop_start.value_or(0), result.pcm.size());
      return result;
    }
  }
  throw std::runtime_error("ADPCM sample has no terminal block");
}
} // namespace ashford
