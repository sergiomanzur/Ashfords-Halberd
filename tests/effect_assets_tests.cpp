#include "effect_assets_fixture.hpp"
#include "scenario.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
namespace {
namespace fixture = effect_assets_fixture;
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void reject(F action, const char *message) {
  bool failed = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  require(failed, message);
}
ScenarioState initial() {
  ScenarioState state;
  state.effect_assets = decode_global_effect_assets(fixture::global());
  return state;
}
void global() {
  const auto assets = decode_global_effect_assets(fixture::global());
  require(assets->base_texture_count == 40 &&
              assets->textures.textures.size() == 40 &&
              assets->textures.emitted_pixels == 40 &&
              assets->textures.next_bank == 32,
          "Original base emission count/cursor");
  constexpr size_t first[20]{0,  9,  9,  11, 15, 19, 23, 25, 25, 26,
                             28, 31, 36, 36, 36, 36, 36, 36, 38, 39};
  constexpr size_t counts[20]{9, 0, 2, 4, 4, 4, 2, 0, 1, 2,
                              3, 5, 0, 0, 0, 0, 0, 2, 1, 1};
  for (size_t id = 0; id < 30; ++id) {
    const size_t offset = id < 20 ? first[id] : 0;
    require(assets->texture_first[id] == offset &&
                assets->family_texture_count[id] ==
                    (id < 20 ? counts[id] : 0) &&
                assets->palette_bank[id] == 255 &&
                assets->texture_families.test(id),
            "Original base table/zero-animation alias");
    const auto &binding = effect_texture_binding(*assets, int32_t(id));
    require(binding.global_index == 1000 + offset && !binding.shared_bank &&
                resolve_effect_asset(*assets, int32_t(id)).rgba[0] ==
                    10 + offset,
            "Original base texture lookup");
  }
  for (size_t id = 0; id < 20; ++id)
    require(bool(assets->models[id]) ==
                (id == 0 || id == 1 || id == 7 || id == 13),
            "Original global model IDs");
  require(effect_texture_binding(*assets, 1, 1).global_index == 1010 &&
              effect_texture_binding(*assets, 20, 39).global_index == 1039,
          "Source animation bounded by library, not family metadata");
  reject([&] { effect_texture_binding(*assets, -1); },
         "Negative family accepted");
  reject([&] { effect_texture_binding(*assets, 450); },
         "Oversized family accepted");
  reject([&] { effect_texture_binding(*assets, 92); },
         "Unloaded family accepted");
  reject([&] { effect_texture_binding(*assets, 0, -1); },
         "Negative frame accepted");
  reject([&] { effect_texture_binding(*assets, 19, 1); },
         "Out-of-library frame accepted");
}
void families() {
  auto state = initial();
  const auto previous = state.effect_assets;
  bind_room_effect_assets(fixture::room({408, 54, 92, 54, 179, 400}), state);
  const auto &assets = *state.effect_assets;
  require(state.effect_assets != previous &&
              assets.textures.textures.size() == 44 &&
              assets.textures.emitted_pixels == 44 &&
              assets.textures.next_bank == 36 &&
              previous->textures.textures.size() == 40,
          "Room source-order unique emissions and immutable base");
  size_t ordinal = 0;
  for (int32_t id : {408, 54, 92, 400}) {
    require(assets.texture_first[id] == 40 + ordinal &&
                assets.family_texture_count[id] == 1 &&
                assets.palette_bank[id] == 32 + ordinal &&
                effect_texture_binding(assets, id).global_index ==
                    3000 + ordinal &&
                resolve_effect_asset(assets, id).rgba[0] == 100 + ordinal * 10,
            "Inactive/unique family consumes actual next pack");
    ++ordinal;
  }
  require(!assets.texture_families.test(179) && state.room_texture_cursor == 1,
          "Controller ID179 does not load a room texture pack");
  // Geometry plus two cached actor packs precede effect textures. The binder
  // must use the published cursor, rather than count records backwards.
  auto room = fixture::room({92});
  room.resize(room.size() + 16);
  fixture::word(room, 1000, 4);
  fixture::word(room, 1016, 2400);
  auto cursor_state = initial();
  cursor_state.room_texture_cursor = 3;
  bind_room_effect_assets(room, cursor_state);
  require(resolve_effect_asset(*cursor_state.effect_assets, 92).rgba[0] == 100,
          "Actual actor texture cursor ignored");
}
void transition() {
  auto state = initial();
  bind_room_effect_assets(fixture::room({408, 54, 92}), state);
  auto old = state.effect_assets;
  std::weak_ptr<const NativeIndexedTexture> released =
      old->textures.textures[42].image;
  auto carry = std::make_shared<NativeEffectAssets>(*old);
  carry->textures.palette[2000] = {7, 8, 9, 10};
  carry->textures.initialized.set(2000);
  state.effect_assets = carry;
  bind_room_effect_assets(fixture::room({92}), state);
  const auto &assets = *state.effect_assets;
  require(assets.textures.textures.size() == 41 &&
              assets.textures.emitted_pixels == 41 &&
              assets.texture_first[92] == 40 && assets.palette_bank[92] == 32 &&
              assets.textures.next_bank == 33 &&
              !assets.texture_families.test(408) &&
              !assets.texture_families.test(54),
          "Transition releases previous room families and restarts cursor");
  require(
      assets.textures.palette[2000] == std::array<uint8_t, 4>{7, 8, 9, 10} &&
          assets.textures.initialized.test(2000) &&
          assets.textures.palette[33 * 16] == old->textures.palette[33 * 16] &&
          assets.textures.textures[0].direct ==
              old->textures.textures[0].direct,
      "Transition preserves CPU palette and base image identity");
  require(old->textures.textures.size() == 43 && old->texture_first[92] == 42 &&
              !released.expired(),
          "Incoming resource owner mutated during replacement");
  carry.reset();
  old.reset();
  require(released.expired(), "Released room image retained by replacement");
  // The old immutable snapshot deliberately owns its old images until it dies.
  // A no-effect room retains exactly the base and still preserves palette RAM.
  bind_room_effect_assets(fixture::room({179}), state);
  require(state.effect_assets->textures.textures.size() == 40 &&
              state.effect_assets->textures.next_bank == 32 &&
              state.effect_assets->textures.palette[2000][0] == 7,
          "No-effect transition drops room bindings, not palette RAM");
}
void validation() {
  reject([] { decode_global_effect_assets({}); },
         "Missing global header accepted");
  reject([] { decode_global_effect_assets(fixture::global(39)); },
         "Wrong base count accepted");
  for (int mutation = 0; mutation < 5; ++mutation) {
    auto data = fixture::global();
    if (mutation == 0)
      fixture::word(data, 0, 0xffffffff);
    if (mutation == 1)
      fixture::word(data, 4, 0);
    if (mutation == 2)
      fixture::half(data, 8, 8);
    if (mutation == 3)
      fixture::half(data, 10, 2);
    if (mutation == 4)
      fixture::word(data, 64, 2000);
    reject([&] { decode_global_effect_assets(data); },
           "Malformed global model accepted");
  }
  ScenarioState absent;
  reject([&] { bind_room_effect_assets(fixture::room({92}), absent); },
         "Missing actual global owner accepted");
  auto state = initial();
  bind_room_effect_assets(fixture::room({54}), state);
  const auto old = state.effect_assets;
  const auto old_colors = old->textures.palette;
  for (int mutation = 0; mutation < 6; ++mutation) {
    auto data = fixture::room({408, 92});
    if (mutation == 0)
      fixture::word(data, 32, 0);
    if (mutation == 1)
      fixture::word(data, 1000, 1);
    if (mutation == 2)
      fixture::word(data, 1008, 1004);
    if (mutation == 3)
      data.pop_back();
    if (mutation == 4)
      fixture::word(data, 2404, 0xffffffff);
    if (mutation == 5)
      fixture::word(data, 1000, 4097);
    reject([&] { bind_room_effect_assets(data, state); },
           "Malformed room resources accepted");
    require(state.effect_assets == old && old->textures.palette == old_colors &&
                old->textures.next_bank == 33 &&
                old->texture_families.test(54) &&
                !old->texture_families.test(92),
            "Failed resource replacement mutated current owner");
  }
  state.room_texture_cursor = 0;
  reject([&] { bind_room_effect_assets(fixture::room({92}), state); },
         "Invalid published texture cursor accepted");
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::string mode = argc == 2 ? argv[1] : "";
    if (mode == "global")
      global();
    else if (mode == "families")
      families();
    else if (mode == "transition")
      transition();
    else if (mode == "validation")
      validation();
    else
      throw std::runtime_error("Unknown resource binding test mode");
    std::cout << "Native effect resource binding " << mode << " verified\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
