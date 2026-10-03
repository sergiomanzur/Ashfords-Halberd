#include "audio_runtime.hpp"
#include "scenario.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void rejects(F f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  check(caught, message);
}
void request_tests() {
  using namespace ashford;
  ScenarioState state;
  background_sound_command(state.background_audio, 3);
  run_scenario(Bytes{148, 0, 0, 0, 148, 1, 1, 0, 0}, state);
  check(state.background_audio.initializing &&
            state.background_audio.commands.empty(),
        "Original148 schedules requests without immediate playback");
  check(state.background_audio.current[0] == 0 &&
            state.background_audio.current[1] == 1 &&
            state.background_audio.request[0] == 2 &&
            state.background_audio.request[1] == 2,
        "Independent original slot requests");
  auto clone = state;
  run_scenario(Bytes{146, 0, 0}, clone);
  check(clone.background_audio.current[0] == -1 &&
            clone.background_audio.request[0] == 1 &&
            state.background_audio.current[0] == 0,
        "Scenario value-copy isolates live requests");
  background_sound_command(state.background_audio, 4);
  check(state.background_audio.initializing,
        "Matching command4 does not clear init flag");
  background_sound_command(state.background_audio, 1);
  check(!state.background_audio.initializing &&
            state.background_audio.commands.size() == 2 &&
            state.background_audio.current[0] == 0x300 &&
            state.background_audio.current[1] == 0x301 &&
            state.background_audio.commands[0].timer == 120 &&
            !state.background_audio.request[0],
        "Command1 starts bank3 with source120 fade and clears requests");
  state.background_audio.commands.clear();
  run_scenario(Bytes{148, 0, 0, 0, 0}, state);
  check(state.background_audio.request[0] == 2,
        "Raw0 differs from forced bank3 code300");
  state.background_audio.request[0] = 0;
  run_scenario(Bytes{148, 0, 0, 0, 0}, state);
  check(state.background_audio.request[0] == 0,
        "Repeated raw code suppresses a duplicate request");
  run_scenario(Bytes{146, 1, 0}, state);
  background_sound_command(state.background_audio, 2);
  check(state.background_audio.commands.size() == 1 &&
            state.background_audio.commands[0].kind ==
                BackgroundCommandKind::fade_out &&
            state.background_audio.commands[0].slot == 1 &&
            state.background_audio.commands[0].timer == 400,
        "Source stop waits for fade-out command");
  rejects([&] { run_scenario(Bytes{148, 2, 0, 0, 0}, state); },
          "Bad ambient slot");
  rejects([&] { run_scenario(Bytes{148, 0, 0}, state); }, "Truncated148");
  rejects([&] { run_scenario(Bytes{146}, state); }, "Truncated146");
  rejects([&] { background_sound_command(state.background_audio, 9); },
          "Unsupported sound command");
  BackgroundAudioState bounded;
  for (int i = 0; i < 32; ++i) {
    request_background_stop(bounded, 0);
    background_sound_command(bounded, 2);
  }
  request_background_stop(bounded, 1);
  rejects([&] { background_sound_command(bounded, 2); },
          "Background command budget");
  check(bounded.commands.size() == 32 && bounded.request[1] == 1,
        "Rejected command batch retains original requests");
}
std::shared_ptr<const ashford::RoomSoundAssets> assets_fixture() {
  using namespace ashford;
  auto bank = std::make_shared<SceSoundBank>();
  SceWave wave;
  wave.sample_rate = 48000;
  wave.attribute = 1;
  wave.sample.pcm = {12000, -11000, 8000, -7000};
  wave.sample.loop = {{0, 4}};
  bank->waves.push_back(wave);
  SceSampleLayer layer;
  layer.wave = 0;
  layer.base_note = 60;
  layer.volume = 127;
  layer.velocity_high = 127;
  layer.adsr1 = 0;
  layer.adsr2 = 0x1fc0;
  layer.parameters.assign(42, 0);
  layer.parameters[13] = 64;
  layer.parameters[16] = 127;
  // Native direct/effect send masks used by the existing mixer.
  layer.parameters[2] = 1;
  layer.parameters[41] = 3;
  bank->layers.push_back(layer);
  SceSampleSet set;
  set.parameters = {0, 1, 127, 1};
  set.layers = {0};
  bank->sample_sets.push_back(set);
  SceSplit split;
  split.sample_set = 0;
  split.note_high = 127;
  split.volume = 127;
  split.parameters.assign(20, 0);
  split.parameters[17] = 64;
  SceProgram program;
  program.volume = 127;
  program.parameters.assign(36, 0);
  program.parameters[7] = 64;
  program.splits = {split};
  bank->programs.push_back(program);
  SceSequence sequence;
  sequence.ppqn = 480;
  sequence.events = {{0, SceEventType::note_on, 0, 60, 127},
                     {100000, SceEventType::note_off, 0, 60},
                     {100001, SceEventType::end}};
  auto assets = std::make_shared<RoomSoundAssets>();
  assets->programs[3] = bank;
  assets->sequences[3] =
      std::make_shared<const std::vector<std::optional<SceSequence>>>(
          std::vector<std::optional<SceSequence>>{sequence, sequence});
  assets->has_environment = true;
  return assets;
}
void playback_tests() {
  using namespace ashford;
  const auto assets = assets_fixture();
  RoomAudioPlayback player(assets), single(assets), muted(assets),
      reference(assets);
  const BackgroundSoundCommand a{BackgroundCommandKind::play, 0, 0x300, 120};
  const BackgroundSoundCommand b{BackgroundCommandKind::play, 1, 0x301, 120};
  player.apply({a, b});
  single.apply({a});
  muted.apply({a});
  reference.apply({a});
  check(player.active_slots() == 2 && single.active_slots() == 1,
        "Native independent ambient slots");
  bool nonzero = false;
  for (int tick = 0; tick < 24; ++tick) {
    const auto both = player.tick(100), one = single.tick(100);
    check(both.size() == 800 && one.size() == 800,
          "48kHz source manager tick800 frames");
    for (size_t i = 0; i < one.size(); ++i) {
      nonzero |= both[i].left || both[i].right;
      check(both[i].left == one[i].left * 2 &&
                both[i].right == one[i].right * 2,
            "Two slots combine independently before final clipping");
    }
    check(muted.tick(0) == std::vector<StereoPcmFrame>(800),
          "Zero effects output gain");
    reference.tick(100);
  }
  check(nonzero && muted.tick(100) == reference.tick(100),
        "Mute preserves sequence/envelope clock");
  rejects(
      [&] { player.apply({a, {BackgroundCommandKind::play, 1, 0x307, 120}}); },
      "Invalid sequence rejects complete batch");
  check(player.active_slots() == 2,
        "Invalid batch cannot replace existing slots");
  player.apply({{BackgroundCommandKind::fade_out, 0, -1, 120}});
  for (int tick = 0; tick < 21; ++tick)
    player.tick(100);
  check(player.active_slots() == 2,
        "Source fade stop waits through zero counter");
  player.tick(100);
  check(player.active_slots() == 1, "Fade completion stops only its slot");
  rejects([&] { player.tick(101); }, "Invalid effects gain");
  rejects([&] { player.apply({{BackgroundCommandKind::play, 2, 0x300, 0}}); },
          "Invalid mixer slot");
  rejects([&] { player.apply({{BackgroundCommandKind::play, 0, 0x300, -1}}); },
          "Invalid playback timer");
  player.stop();
  check(player.active_slots() == 0, "Terminal owner stop clears slots");
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::string mode = argc == 2 ? argv[1] : "all";
    if (mode == "all" || mode == "requests")
      request_tests();
    if (mode == "all" || mode == "playback")
      playback_tests();
    std::cout << "Native room audio runtime passed: " << mode << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
