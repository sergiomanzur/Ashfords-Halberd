#include "effect_assets.hpp"
#include "room.hpp"
#include "scenario.hpp"
#include <stdexcept>
// Native resource ownership from effect.c::bhInitEffect, room.c::bhInitRoom
// and system.c::bhInitRoomChangeSystem. Original controller/draw services
// follow.
namespace ashford {
namespace {
constexpr std::array<uint8_t, 20> animations{9, 0, 2, 4, 4, 4, 2, 0, 1, 2,
                                             3, 5, 0, 0, 0, 0, 0, 2, 1, 1};
constexpr std::array<size_t, 4> model_ids{0, 1, 7, 13};
struct Reader {
  const Bytes &data;
  void range(size_t p, size_t n) const {
    if (p > data.size() || n > data.size() - p)
      throw std::runtime_error("Effect resource extent exceeds input");
  }
  uint16_t u16(size_t p) const {
    range(p, 2);
    return uint16_t(data[p]) | uint16_t(data[p + 1]) << 8;
  }
  uint32_t u32(size_t p) const {
    range(p, 4);
    return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16;
  }
};
uint8_t binding_bank(const NativeEffectTextureBinding &binding) {
  return binding.shared_bank ? binding.bank : uint8_t(255);
}
size_t texture_index(const NativeEffectAssets &assets, int32_t id,
                     int32_t animation) {
  if (id < 0 || id >= 450 || animation < 0 ||
      !assets.texture_families.test(size_t(id)))
    throw std::runtime_error("Unloaded native effect texture family");
  const auto first = assets.texture_first[size_t(id)];
  if (first > assets.textures.textures.size() ||
      size_t(animation) >= assets.textures.textures.size() - first)
    throw std::runtime_error("Effect animation exceeds texture library");
  return first + size_t(animation);
}
bool room_family(uint16_t id) { return (id >= 30 && id < 100) || id >= 400; }
void release_room_assets(NativeEffectAssets &assets) {
  if (!assets.base_texture_count ||
      assets.base_texture_count > assets.textures.textures.size())
    throw std::runtime_error("Invalid base effect resource ownership");
  assets.textures.textures.resize(assets.base_texture_count);
  assets.textures.emitted_pixels = 0;
  for (const auto &binding : assets.textures.textures) {
    const auto pixels =
        binding.image ? size_t(binding.image->width) * binding.image->height
                      : size_t(binding.direct->width) * binding.direct->height;
    assets.textures.emitted_pixels += pixels;
  }
  // Original room change resets ef_pbnk but does not clear palbuf.
  assets.textures.next_bank = 32;
  for (size_t id = 0; id < 450; ++id)
    if (room_family(uint16_t(id))) {
      assets.texture_first[id] = assets.family_texture_count[id] = 0;
      assets.texture_families.reset(id);
    }
}
} // namespace
std::shared_ptr<const NativeEffectAssets>
decode_global_effect_assets(const Bytes &data) {
  if (data.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Global effect input exceeds limit");
  Reader r{data};
  auto assets = std::make_shared<NativeEffectAssets>();
  size_t size = r.u32(0), cursor = 4;
  for (auto id : model_ids) {
    r.range(cursor, size);
    if (size < 16 || r.u32(cursor) % 0x1000000 != 0x4c444d)
      throw std::runtime_error("Invalid global effect MDL signature");
    const size_t header = r.u16(cursor + 4);
    if (header < 16 || header > size)
      throw std::runtime_error("Invalid global effect MDL header");
    ActorModel model;
    model.model.assign(data.begin() + cursor, data.begin() + cursor + size);
    const auto pose = actor_bind_pose(model);
    calculate_actor_model_work(model, pose); // Validate hierarchy/work extents.
    assets->models[id] = std::make_shared<const ActorModel>(std::move(model));
    cursor += size;
    size = r.u32(cursor);
    cursor += 4;
  }
  const size_t texture = (cursor + 31) & ~size_t(31);
  r.range(texture, 4);
  const auto loaded = load_effect_texture_pack(data, texture, assets->textures);
  size_t expected = 0;
  for (size_t i = 0; i < animations.size(); ++i) {
    assets->texture_first[i] = expected;
    assets->family_texture_count[i] = animations[i];
    expected += animations[i];
  }
  if (loaded.first || loaded.count != expected)
    throw std::runtime_error("Global effect texture count differs from source");
  assets->base_texture_count = loaded.count;
  for (size_t i = 0; i < 30; ++i) {
    assets->texture_families.set(i);
    assets->palette_bank[i] =
        binding_bank(assets->textures.textures.at(assets->texture_first[i]));
  }
  // Prove all base bindings can sample their real colors before publication.
  for (size_t i = 0; i < assets->base_texture_count; ++i)
    resolve_effect_texture(assets->textures, i);
  return assets;
}
void bind_room_effect_assets(const Bytes &data, ScenarioState &state) {
  const auto records = decode_room_effects(data);
  bool required = false;
  for (const auto &record : records)
    required |= room_family(record.id);
  if (!state.effect_assets) {
    if (required)
      throw std::runtime_error(
          "Room effects require original global resources");
    return;
  }
  auto assets = std::make_shared<NativeEffectAssets>(*state.effect_assets);
  release_room_assets(*assets);
  Reader r{data};
  const size_t table = r.u32(32);
  if (!table)
    throw std::runtime_error("Missing room effect texture pointer table");
  const size_t count = r.u32(table);
  if (!count || count > 4096 || !state.room_texture_cursor ||
      state.room_texture_cursor > count)
    throw std::runtime_error("Invalid room effect texture cursor/count");
  r.range(table + 4, count * 4);
  size_t cursor = state.room_texture_cursor;
  for (const auto &record : records) {
    const auto id = record.id;
    if (!room_family(id) || assets->texture_families.test(id))
      continue;
    if (cursor >= count)
      throw std::runtime_error("Missing authored effect texture pack");
    const size_t offset = r.u32(table + 4 + cursor++ * 4);
    if (offset < table + 4 + count * 4)
      throw std::runtime_error("Effect texture overlaps pointer table");
    const auto loaded =
        load_effect_texture_pack(data, offset, assets->textures);
    if (!loaded.count)
      throw std::runtime_error("Authored effect family emitted no textures");
    assets->texture_first[id] = loaded.first;
    assets->family_texture_count[id] = loaded.count;
    assets->palette_bank[id] =
        binding_bank(assets->textures.textures.at(loaded.first));
    assets->texture_families.set(id);
  }
  if (cursor != count)
    throw std::runtime_error("Unconsumed room effect texture packs");
  // Final palette state, after every family upload, is what drawing samples.
  for (size_t i = 0; i < assets->textures.textures.size(); ++i)
    resolve_effect_texture(assets->textures, i);
  state.effect_assets = std::move(assets);
}
const NativeEffectTextureBinding &
effect_texture_binding(const NativeEffectAssets &assets, int32_t id,
                       int32_t animation) {
  return assets.textures.textures[texture_index(assets, id, animation)];
}
Texture resolve_effect_asset(const NativeEffectAssets &assets, int32_t id,
                             int32_t animation) {
  return resolve_effect_texture(assets.textures,
                                texture_index(assets, id, animation));
}
} // namespace ashford
