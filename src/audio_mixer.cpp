#include "audio_playback.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ashford {
namespace {
void length(const Bytes &p, size_t size) {
  if (p.size() != size)
    throw std::runtime_error("Invalid native instrument parameter length");
}
void unsupported(bool condition, const char *what) {
  if (condition)
    throw std::runtime_error(what);
}
int signed_byte(uint8_t value) {
  return value < 128 ? value : int(value) - 256;
}
int pan_offset(uint8_t pan) {
  return pan <= 127 ? int(pan) - 64 : 64 - (int(pan) - 127);
}
template <class T>
const T &indexed(const std::vector<std::optional<T>> &table, size_t index) {
  if (index >= table.size() || !table[index])
    throw std::runtime_error("Missing indexed native instrument");
  return *table[index];
}
uint16_t pitch(const SceWave &wave, uint8_t note, const SceProgram &p,
               const SceSplit &s, const SceSampleLayer &layer) {
  const auto key = int(note) - layer.base_note + signed_byte(p.parameters[8]) +
                   signed_byte(s.parameters[18]);
  const auto cents = signed_byte(p.parameters[9]) +
                     signed_byte(s.parameters[19]) +
                     signed_byte(layer.parameters[12]);
  const double value = double(wave.sample_rate) / sound_output_rate * 4096 *
                       std::exp2(double(key) / 12 + double(cents) / 1200);
  if (!std::isfinite(value) || value < 0.5)
    throw std::runtime_error("Unsupported zero/sub-quantum native pitch");
  return uint16_t(std::min(16384.0, std::floor(value + 0.5)));
}
} // namespace
bool SoundBusFrame::operator==(const SoundBusFrame &b) const {
  return dry_left == b.dry_left && dry_right == b.dry_right &&
         effect_left == b.effect_left && effect_right == b.effect_right;
}
struct SceMixer::State {
  struct Channel {
    uint8_t program = 0, volume = 127, expression = 127, pan = 64;
  };
  struct Voice {
    SampleVoice sample;
    uint8_t channel = 0, note = 0, sends = 0;
    int pan = 0;
    uint32_t gain = 0;
  };
  std::shared_ptr<const SceSoundBank> bank;
  SceSequencer clock;
  std::array<Channel, 16> channels;
  std::vector<Voice> voices;
  size_t peak = 0;
  State(std::shared_ptr<const SceSoundBank> value, SceSequence sequence)
      : bank(std::move(value)), clock(std::move(sequence)) {
    if (!bank)
      throw std::runtime_error("Native mixer requires an immutable sound bank");
    voices.reserve(sound_voice_limit);
  }
  void note_on(const SceEvent &e) {
    const auto &program = indexed(bank->programs, channels[e.channel].program);
    length(program.parameters, 36);
    unsupported(program.parameters[10] || program.parameters[12],
                "Unsupported native program key-follow/attributes");
    if (program.volume > 127)
      throw std::runtime_error("Invalid native program volume");
    std::vector<Voice> additions;
    additions.reserve(sound_voice_limit - voices.size());
    for (const auto &split : program.splits) {
      length(split.parameters, 20);
      if (split.note_low > split.note_high)
        throw std::runtime_error("Unsupported inverted native split range");
      if (e.data1 < split.note_low || e.data1 > split.note_high)
        continue;
      unsupported((split.parameters[3] & 0x80) || split.parameters[10] ||
                      split.parameters[12] || split.parameters[14],
                  "Unsupported native split crossfade/key-follow");
      const auto &set = indexed(bank->sample_sets, split.sample_set);
      length(set.parameters, 4);
      unsupported(set.parameters[0], "Unsupported native velocity curve");
      if (set.parameters[1] > set.parameters[2] || set.parameters[2] > 127 ||
          split.volume > 127)
        throw std::runtime_error("Invalid native sample-set velocity range");
      if (e.data2 < set.parameters[1] || e.data2 > set.parameters[2])
        continue;
      for (size_t layer_index : set.layers) {
        const auto &layer = indexed(bank->layers, layer_index);
        length(layer.parameters, 42);
        if (layer.velocity_low > layer.velocity_high ||
            layer.velocity_high > 127 || layer.volume > 127 ||
            layer.base_note > 127 || layer.wave >= bank->waves.size())
          throw std::runtime_error("Invalid native sample-layer parameters");
        if (e.data2 < layer.velocity_low || e.data2 > layer.velocity_high)
          continue;
        unsupported((layer.parameters[3] & 0x80) || layer.parameters[5] ||
                        layer.parameters[8] || layer.parameters[14] ||
                        layer.parameters[22] || layer.parameters[24] ||
                        layer.parameters[26] || layer.parameters[28] ||
                        layer.parameters[30] || layer.parameters[40],
                    "Unsupported native layer crossfade/follow/group/LFO");
        unsupported(layer.parameters[41] & 0xe0,
                    "Unsupported native sample send/core attributes");
        if (voices.size() + additions.size() >= sound_voice_limit)
          throw std::runtime_error("Native sound voice budget exceeded");
        const auto &wave = bank->waves[layer.wave];
        const auto sample_pitch = pitch(wave, e.data1, program, split, layer);
        Voice voice{SampleVoice(bank, layer.wave, sample_pitch, layer.adsr1,
                                layer.adsr2),
                    e.channel,
                    e.data1,
                    layer.parameters[41],
                    pan_offset(program.parameters[7]) +
                        pan_offset(split.parameters[17]) +
                        pan_offset(layer.parameters[13]),
                    uint32_t(program.volume) * split.volume * layer.volume *
                        e.data2};
        additions.push_back(std::move(voice));
      }
    }
    for (auto &voice : additions)
      voices.push_back(std::move(voice));
    peak = std::max(peak, voices.size());
  }
  void event(const SceEvent &e) {
    auto &channel = channels[e.channel];
    switch (e.type) {
    case SceEventType::note_on:
      if (e.data2) {
        note_on(e);
        break;
      }
      [[fallthrough]];
    case SceEventType::note_off:
      for (auto &voice : voices)
        if (voice.channel == e.channel && voice.note == e.data1)
          voice.sample.release();
      break;
    case SceEventType::program:
      channel.program = e.data1;
      break;
    case SceEventType::control:
      switch (e.data1) {
      case 7:
        channel.volume = e.data2;
        break;
      case 10:
        channel.pan = e.data2;
        break;
      case 11:
        channel.expression = e.data2;
        break;
      case 6:
      case 38:
      case 99:
        break; // Validated and executed by the native clock.
      default:
        throw std::runtime_error("Unsupported native sound controller");
      }
      break;
    case SceEventType::end:
      for (auto &voice : voices)
        voice.sample.release();
      break;
    case SceEventType::tempo:
      break; // Applied to subsequent deadlines by the clock.
    case SceEventType::pitch:
      throw std::runtime_error("Unsupported native sound pitch controller");
    default:
      throw std::runtime_error("Unknown native sound event");
    }
  }
  SoundBusFrame tick() {
    SoundBusFrame bus;
    constexpr int64_t gain_denominator = 4195872914689LL; // 127^6.
    for (auto &voice : voices) {
      const auto &channel = channels[voice.channel];
      const int64_t product = int64_t(voice.sample.tick()) * voice.gain *
                              channel.volume * channel.expression;
      const int64_t gain = product / gain_denominator;
      const int pan =
          std::clamp(64 + voice.pan + pan_offset(channel.pan), 0, 127);
      const int32_t left = int32_t(gain * (127 - pan) / 127);
      const int32_t right = int32_t(gain * pan / 127);
      if (voice.sends & 1)
        bus.dry_left += left;
      if (voice.sends & 2)
        bus.dry_right += right;
      if (voice.sends & 4)
        bus.effect_left += left;
      if (voice.sends & 8)
        bus.effect_right += right;
    }
    voices.erase(
        std::remove_if(voices.begin(), voices.end(),
                       [](const Voice &v) { return !v.sample.active(); }),
        voices.end());
    return bus;
  }
};
SceMixer::SceMixer(std::shared_ptr<const SceSoundBank> bank,
                   SceSequence sequence)
    : state_(std::make_unique<State>(std::move(bank), std::move(sequence))) {}
SceMixer::~SceMixer() = default;
SceMixer::SceMixer(SceMixer &&) noexcept = default;
SceMixer &SceMixer::operator=(SceMixer &&) noexcept = default;
std::vector<SoundBusFrame> SceMixer::render(uint32_t frames) {
  const auto start = state_->clock.frame();
  const auto events = state_->clock.advance(frames);
  std::vector<SoundBusFrame> pcm(frames);
  size_t event = 0;
  for (uint32_t frame = 0; frame < frames; ++frame) {
    while (event < events.size() && events[event].frame == start + frame)
      state_->event(events[event++].event);
    pcm[frame] = state_->tick();
  }
  return pcm;
}
size_t SceMixer::active_voices() const { return state_->voices.size(); }
size_t SceMixer::peak_voices() const { return state_->peak; }
uint64_t SceMixer::loop_repetitions() const {
  return state_->clock.loop_repetitions();
}
} // namespace ashford
