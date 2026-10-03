#include "audio_assets.hpp"
#include "audio_playback.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
using namespace ashford;
namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void rejects(F action, const char *message) {
  try {
    action();
  } catch (const std::runtime_error &) {
    return;
  }
  throw std::runtime_error(message);
}
SceEvent event(uint64_t tick, SceEventType type, uint8_t a = 0, uint8_t b = 0,
               uint8_t channel = 0) {
  return {tick, type, channel, a, b, 0};
}
SceSequence looping() {
  return {480,
          {event(0, SceEventType::control, 99, 0),
           event(0, SceEventType::control, 6, 0),
           event(1, SceEventType::note_on, 60, 127),
           event(2, SceEventType::note_off, 60),
           event(3, SceEventType::control, 99, 1),
           event(3, SceEventType::control, 6, 0),
           event(4, SceEventType::control, 38, 0),
           event(4, SceEventType::end)}};
}
void timing_tests() {
  SceSequence seq{3,
                  {{0, SceEventType::tempo, 0, 0, 0, 100000},
                   event(1, SceEventType::note_on, 60, 127),
                   {2, SceEventType::tempo, 0, 0, 0, 200000},
                   event(3, SceEventType::note_off, 60),
                   event(4, SceEventType::end)}};
  SceSequencer full(seq), split(seq);
  const auto all = full.advance(10000);
  check(all.size() == 5 && all[1].frame == 1600 && all[2].frame == 3200 &&
            all[3].frame == 6400 && all[4].frame == 9600,
        "tempo change must apply after its event");
  std::vector<ScheduledSceEvent> pieces;
  for (uint32_t count : {1u, 1599u, 1u, 1599u, 1u, 3199u, 1u, 3599u}) {
    auto part = split.advance(count);
    pieces.insert(pieces.end(), part.begin(), part.end());
  }
  check(pieces == all, "event timing must not depend on block partitioning");
  SceSequence fractional{7,
                         {{0, SceEventType::tempo, 0, 0, 0, 1},
                          event(10, SceEventType::note_on, 60, 127),
                          event(150, SceEventType::end)}};
  SceSequencer fraction(fractional);
  check(fraction.advance(1).size() == 1, "fractional event must not run early");
  auto one = fraction.advance(1);
  check(one.size() == 1 && one[0].frame == 1, "fractional deadline ceiling");
  auto two = fraction.advance(1);
  check(two.size() == 1 && two[0].frame == 2,
        "fraction retained between events");
  SceSequencer loop(looping());
  const auto repeated = loop.advance(1000);
  std::vector<uint64_t> starts;
  for (const auto &e : repeated)
    if (e.event.type == SceEventType::note_on)
      starts.push_back(e.frame);
  check(starts == std::vector<uint64_t>({50, 250, 450, 650, 850}),
        "loop jump must retain wall time and start delta");
  check(loop.loop_repetitions() == 4, "loop repeat accounting");
  auto bad = looping();
  for (auto &e : bad.events)
    e.tick = 0;
  rejects([&] { SceSequencer clock(bad); }, "zero-duration loop accepted");
  bad = looping();
  bad.events[6].data2 = 1;
  rejects([&] { SceSequencer clock(bad); }, "unsupported finite loop accepted");
  bad = looping();
  bad.events[5].data2 = 1;
  rejects([&] { SceSequencer clock(bad); }, "unmatched loop number accepted");
  bad = looping();
  bad.events[4].data2 = 0;
  rejects([&] { SceSequencer clock(bad); }, "nested loop accepted");
  bad = seq;
  bad.events[3].tick = 0;
  rejects([&] { SceSequencer clock(bad); }, "backwards tick accepted");
  bad = seq;
  bad.events.back().tick = UINT64_MAX;
  rejects(
      [&] {
        SceSequencer clock(bad);
        clock.advance(10000);
      },
      "tick arithmetic overflow accepted");
  rejects([&] { full.advance(48001); }, "unbounded frame block accepted");
  bad = looping();
  bad.ppqn = 32767;
  bad.events.insert(bad.events.begin(), {0, SceEventType::tempo, 0, 0, 0, 1});
  for (size_t i = 3; i < bad.events.size(); ++i)
    bad.events[i].tick = 1;
  rejects(
      [&] {
        SceSequencer clock(bad);
        clock.advance(2);
      },
      "sub-frame loop must respect event expansion cap");
}
std::shared_ptr<SceSoundBank> bank_fixture(uint8_t sends = 3) {
  auto bank = std::make_shared<SceSoundBank>();
  SceWave wave;
  wave.sample_rate = 48000;
  wave.attribute = 1;
  wave.sample.pcm.assign(64, 1000);
  wave.sample.loop = {{2, 64}};
  bank->waves.push_back(wave);
  SceSampleLayer layer;
  layer.parameters.assign(42, 0);
  layer.parameters[2] = layer.velocity_low = 1;
  layer.parameters[4] = layer.velocity_high = 127;
  layer.parameters[11] = layer.base_note = 60;
  layer.parameters[13] = 64;
  layer.parameters[16] = layer.volume = 127;
  layer.parameters[18] = 15;
  layer.parameters[20] = 0xc0;
  layer.parameters[21] = 0x1f;
  layer.parameters[41] = sends;
  layer.adsr1 = 15;
  layer.adsr2 = 0x1fc0;
  bank->layers.push_back(layer);
  SceSampleSet set;
  set.parameters = {0, 1, 127, 1};
  set.layers = {0};
  bank->sample_sets.push_back(set);
  SceSplit split;
  split.parameters.assign(20, 0);
  split.parameters[2] = split.note_low = 0;
  split.parameters[4] = split.note_high = 127;
  split.parameters[16] = split.volume = 127;
  split.parameters[17] = 64;
  SceProgram program;
  program.parameters.assign(36, 0);
  program.parameters[4] = 1;
  program.parameters[5] = 20;
  program.parameters[6] = program.volume = 127;
  program.parameters[7] = 64;
  program.splits.push_back(split);
  bank->programs.push_back(program);
  return bank;
}
void voice_tests() {
  SoundEnvelope e(0x000f, 0x0000);
  check(e.tick() == 14336 && e.tick() == 28672 && e.tick() == 32767,
        "linear attack and saturation");
  e.release();
  check(e.tick() == 16383 && e.tick() == 0 && !e.active(), "linear release");
  SoundEnvelope fast(0x800f, 0x0000);
  check(fast.tick() == 14336 && fast.tick() == 28672 && fast.tick() == 32256 &&
            fast.tick() == 32767,
        "fast exponential attack uses documented step reduction");
  SoundEnvelope decay(0x0000, 0x1fc0);
  for (int i = 0; i < 20; ++i)
    decay.tick();
  check(decay.level() == 2047, "exponential decay target and floor");
  for (int i = 0; i < 40000; ++i)
    check(decay.tick() == 2047, "all-ones sustain rate must retain level");
  check(gaussian_sample({1000, 1000, 1000, 1000}, 0) == 993 &&
            gaussian_sample({-1000, -1000, -1000, -1000}, 0) == -997 &&
            gaussian_sample({32767, 0, 0, 0}, 0) == 4806,
        "signed four-point interpolation literal vectors");
  auto bank = bank_fixture();
  SampleVoice loop(bank, 0, 4096, 15, 0x1fc0);
  bank.reset();
  int32_t last = 0;
  for (int i = 0; i < 200; ++i)
    last = loop.tick();
  check(loop.active() && last == 992,
        "exclusive sample loop and owned lifetime");
  bank = bank_fixture();
  bank->waves[0].sample.pcm = {0, 0, 3000, 6000, 9000, 12000};
  bank->waves[0].sample.loop = {{2, 6}};
  SampleVoice history(bank, 0, 4096, 15, 0x1fc0);
  for (int i = 0; i < 7; ++i)
    last = history.tick();
  const auto repeat = gaussian_sample({6000, 9000, 12000, 3000}, 0) - 1;
  check(last == repeat, "first sample loop must retain end history");
  for (int i = 0; i < 4; ++i)
    last = history.tick();
  check(last == repeat, "subsequent sample loop must retain wrapped history");
  bank = bank_fixture();
  bank->waves[0].attribute = 0;
  SampleVoice once(bank, 0, 8192, 15, 0x1fc0);
  for (int i = 0; i < 40; ++i)
    once.tick();
  check(!once.active() && once.tick() == 0, "one-shot must stop at sample end");
  bank->waves[0].attribute = 1;
  bank->waves[0].sample.loop = {{64, 64}};
  rejects([&] { SampleVoice bad(bank, 0, 4096, 15, 0x1fc0); },
          "empty sample loop accepted");
  bank = bank_fixture();
  rejects([&] { SampleVoice bad(bank, 0, 0, 15, 0x1fc0); },
          "zero pitch accepted");
  rejects([&] { SampleVoice bad(bank, 0, 16385, 15, 0x1fc0); },
          "over-limit pitch accepted");
}
SceSequence sustained() {
  return {480,
          {event(0, SceEventType::note_on, 60, 127),
           event(2, SceEventType::control, 10, 0),
           event(4, SceEventType::control, 7, 0),
           event(5, SceEventType::note_off, 60), event(6, SceEventType::end)}};
}
void mixer_tests() {
  auto bank = bank_fixture();
  SceMixer full(bank, sustained()), split(bank, sustained());
  const auto pcm = full.render(1000);
  auto pieces = split.render(17);
  auto rest = split.render(983);
  pieces.insert(pieces.end(), rest.begin(), rest.end());
  check(pieces == pcm, "PCM must not depend on block partitioning");
  check(pcm[80].dry_left > 0 && pcm[80].dry_right > 0 &&
            pcm[150].dry_left > 0 && pcm[150].dry_right == 0,
        "pan controller must update existing voice");
  check(pcm[200].dry_left == 0 && pcm[200].dry_right == 0,
        "volume controller must update existing voice");
  check(full.active_voices() == 0, "note-off must release sustained voice");
  bank = bank_fixture(12);
  SceMixer wet(bank, sustained());
  const auto buses = wet.render(90);
  check(buses[80].dry_left == 0 && buses[80].dry_right == 0 &&
            buses[80].effect_left > 0 && buses[80].effect_right > 0,
        "effect-only instrument must retain distinct wet bus");
  bank = bank_fixture();
  bank->sample_sets[0]->layers.assign(49, 0);
  bank->sample_sets[0]->parameters[3] = 49;
  rejects(
      [&] {
        SceMixer over(bank, sustained());
        over.render(1);
      },
      "voice expansion limit ignored");
  bank = bank_fixture();
  bank->layers[0]->parameters.resize(41);
  rejects(
      [&] {
        SceMixer bad(bank, sustained());
        bad.render(1);
      },
      "truncated instrument parameters accepted");
  bank = bank_fixture();
  bank->layers[0]->parameters[8] = 1;
  rejects(
      [&] {
        SceMixer bad(bank, sustained());
        bad.render(1);
      },
      "unsupported velocity-follow amplitude silently ignored");
  bank = bank_fixture();
  bank->programs[0].reset();
  rejects(
      [&] {
        SceMixer bad(bank, sustained());
        bad.render(1);
      },
      "missing sparse program accepted");
  bank = bank_fixture();
  auto pitch = sustained();
  pitch.events[0].type = SceEventType::pitch;
  rejects(
      [&] {
        SceMixer bad(bank, pitch);
        bad.render(1);
      },
      "unsupported pitch controller silently ignored");
  auto unknown = sustained();
  unknown.events[1].data1 = 1;
  rejects(
      [&] {
        SceMixer bad(bank, unknown);
        bad.render(101);
      },
      "unsupported modulation controller silently ignored");
  rejects([&] { full.render(48001); }, "mixer block limit ignored");
  bank = bank_fixture();
  bank->waves[0].attribute = 0;
  const auto note = [](uint8_t key) {
    return SceSequence{480,
                       {event(0, SceEventType::note_on, key, 127),
                        event(20, SceEventType::end)}};
  };
  SceMixer normal(bank, note(60)), octave(bank, note(72));
  normal.render(40);
  octave.render(40);
  check(normal.active_voices() == 1 && octave.active_voices() == 0,
        "octave must double source sample advance");
  bank->waves[0].sample_rate = 24000;
  SceMixer half_rate(bank, note(72));
  half_rate.render(40);
  check(half_rate.active_voices() == 1,
        "original sample rate must affect pitch");
  bank = bank_fixture();
  for (size_t i = 0; i < bank->waves[0].sample.pcm.size(); ++i)
    bank->waves[0].sample.pcm[i] = int16_t(i * 500);
  SceMixer semitone(bank, note(61));
  const auto semitone_pcm = semitone.render(40);
  bank->programs[0]->parameters[9] = 100;
  SceMixer cents(bank, note(60));
  check(cents.render(40) == semitone_pcm, "100 cents must equal one semitone");
  bank = bank_fixture();
  SceMixer repeat(bank, looping());
  const auto repeat_pcm = repeat.render(1000);
  check(repeat_pcm[80].dry_left > 0 && repeat_pcm[280].dry_left > 0 &&
            repeat_pcm[880].dry_left > 0 && repeat.loop_repetitions() == 4,
        "sequence looping must retrigger real sample voices");
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "all" || mode == "timing")
      timing_tests();
    if (mode == "all" || mode == "voice")
      voice_tests();
    if (mode == "all" || mode == "mixer")
      mixer_tests();
    std::cout << "Native sequence playback checks passed: " << mode << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
