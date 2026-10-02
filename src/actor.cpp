#include "actor.hpp"
#include "room.hpp"
// Layout adapted from MIT recvx-decomp dread.c::bhReadPlayerData.
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
namespace ashford {
namespace {
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Player extent exceeds input");
  }
  uint32_t u32(size_t p) const {
    range(p, 4);
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
           uint32_t(b[p + 3]) << 24;
  }
  Bytes copy(size_t p, size_t n) const {
    range(p, n);
    Bytes result(n);
    if (n)
      std::memcpy(result.data(), b.data() + p, n);
    return result;
  }
};
template <class F> void records(const Bytes &b, size_t limit, F handle) {
  Reader r{b};
  size_t p = 0, slots = 0;
  while (true) {
    auto size = r.u32(p);
    p += 4;
    if (size == 0xffffffff) {
      if (p != b.size())
        throw std::runtime_error("Trailing player records");
      return;
    }
    if (++slots > limit)
      throw std::runtime_error("Too many player records");
    handle(r.copy(p, size));
    p += size;
  }
}
} // namespace
ActorModelBank decode_actor_model_bank(const Bytes &data, size_t begin,
                                       size_t end) {
  if (begin > end || end > data.size() || end - begin > 64 * 1024 * 1024)
    throw std::runtime_error("Invalid actor model bank extent");
  Reader r{data};
  ActorModelBank bank;
  Bytes skin;
  size_t p = begin;
  for (size_t records = 0; records < 4096; ++records) {
    if (end - p < 4)
      throw std::runtime_error("Missing actor model bank terminator");
    auto size = r.u32(p);
    p += 4;
    if (size == 0xffffffff) {
      if (!skin.empty())
        throw std::runtime_error("Unbound actor skin record");
      return bank;
    }
    if (size > end - p)
      throw std::runtime_error("Actor model bank record outside extent");
    if (!size)
      continue;
    auto record = r.copy(p, size);
    p += size;
    Reader part{record};
    auto signature = part.u32(0);
    if (signature == 0x4b53414d) {
      bank.mask = std::move(record);
    } else if (signature == 0x4e494b53) {
      skin = std::move(record);
    } else {
      part.range(0, 16);
      if (record[0] != 'M' || record[1] != 'D' || record[2] != 'L' ||
          bank.models.size() >= 16)
        throw std::runtime_error("Invalid actor model bank model");
      size_t header = uint16_t(record[4]) | uint16_t(record[5]) << 8;
      if (header < 16)
        throw std::runtime_error("Invalid actor model bank header");
      part.range(0, header);
      bank.models.push_back({std::move(record), std::move(skin), {}});
      skin.clear();
    }
  }
  throw std::runtime_error("Actor model bank record limit exceeded");
}
static RoomActorModels decode_room_actor_models_impl(const Bytes &data,
                                                     bool include_props) {
  if (data.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Room enemy input exceeds limit");
  auto records = decode_room_records(data);
  if (records.enemies.size() > 128)
    throw std::runtime_error("Too many room enemy models");
  Reader r{data};
  size_t table = r.u32(20);
  RoomActorModels result;
  result.enemies.resize(records.enemies.size());
  auto &banks = result.enemies;
  if (include_props) {
    result.objects.resize(records.objects.size());
    result.items.resize(records.items.size());
  }
  size_t active = 0;
  for (const auto &enemy : records.enemies)
    if (enemy.flags & 1)
      ++active;
  if (!active && (!include_props ||
                  (records.objects.size() <= 4 && records.items.empty())))
    return result;
  if (!table)
    throw std::runtime_error("Missing room model pointer table");
  size_t pointer_count =
      1 + active +
      (records.objects.size() > 4 ? records.objects.size() - 4 : 0) +
      records.items.size();
  r.range(table, pointer_count * 4);
  std::vector<size_t> pointers;
  for (size_t i = 0; i < pointer_count; ++i)
    pointers.push_back(r.u32(table + i * 4));
  auto boundary = [&](size_t begin, const std::vector<size_t> &others) {
    if (begin >= data.size())
      throw std::runtime_error("Room actor section outside input");
    size_t end = data.size();
    for (auto p : others)
      if (p > begin && p < end)
        end = p;
    for (size_t h :
         {size_t(16), size_t(20), size_t(24), size_t(28), size_t(32)}) {
      size_t p = r.u32(h);
      if (p > begin && p < end)
        end = p;
    }
    return end;
  };
  size_t slot = 1;
  size_t remaining = 64 * 1024 * 1024;
  auto charge = [&](size_t size) {
    if (size > remaining)
      throw std::runtime_error("Room enemy asset budget exceeded");
    remaining -= size;
  };
  for (size_t i = 0; i < banks.size(); ++i)
    if (records.enemies[i].flags & 1) {
      size_t begin = pointers[slot++];
      if (begin < table + pointer_count * 4)
        throw std::runtime_error("Room enemy model overlaps pointer table");
      banks[i] =
          decode_actor_model_bank(data, begin, boundary(begin, pointers));
      charge(banks[i].mask.size());
      for (const auto &model : banks[i].models) {
        charge(model.model.size());
        charge(model.skin.size());
      }
    }
  if (include_props) {
    auto model_record = [&](size_t begin, bool skin_prefix) {
      size_t end = boundary(begin, pointers);
      if (begin < table + pointer_count * 4 || end - begin < 16)
        throw std::runtime_error("Room prop model outside extent");
      ActorModel model;
      if (skin_prefix && !(data[begin] == 'M' && data[begin + 1] == 'D' &&
                           data[begin + 2] == 'L')) {
        size_t size = r.u32(begin);
        if (size > end - begin - 8)
          throw std::runtime_error("Room object skin outside extent");
        model.skin = r.copy(begin + 4, size + 4);
        if (model.skin.size() < 4 || model.skin[0] != 'S' ||
            model.skin[1] != 'K' || model.skin[2] != 'I' ||
            model.skin[3] != 'N')
          throw std::runtime_error("Invalid room object skin signature");
        charge(model.skin.size());
        begin += size + 8;
      }
      if (end - begin < 16)
        throw std::runtime_error("Truncated room prop model");
      model.model = r.copy(begin, end - begin);
      if (model.model[0] != 'M' || model.model[1] != 'D' ||
          model.model[2] != 'L')
        throw std::runtime_error("Invalid room prop model signature");
      size_t header = uint16_t(model.model[4]) | uint16_t(model.model[5]) << 8;
      if (header < 16 || header > model.model.size())
        throw std::runtime_error("Invalid room prop model header");
      charge(model.model.size());
      return model;
    };
    for (size_t i = 4; i < result.objects.size(); ++i)
      result.objects[i] = model_record(pointers.at(slot++), true);
    for (auto &item : result.items)
      item = model_record(pointers.at(slot++), false);
  }
  size_t texture_table = r.u32(32);
  if (!texture_table)
    throw std::runtime_error("Missing room actor texture table");
  size_t texture_count = r.u32(texture_table);
  if (!texture_count || texture_count > 4096)
    throw std::runtime_error("Invalid room actor texture count");
  r.range(texture_table + 4, texture_count * 4);
  std::vector<size_t> textures;
  for (size_t i = 0; i < texture_count; ++i)
    textures.push_back(r.u32(texture_table + 4 + i * 4));
  auto texture = [&](size_t offset) {
    size_t end = boundary(offset, textures), p = offset;
    if (offset < texture_table + 4 + texture_count * 4)
      throw std::runtime_error("Actor texture overlaps pointer table");
    for (size_t n = 0; n < 4096; ++n) {
      if (end - p < 4)
        throw std::runtime_error("Missing actor texture terminator");
      if (r.u32(p) == 0xffffffff)
        return r.copy(offset, p + 4 - offset);
      if (end - p < 32)
        throw std::runtime_error("Truncated actor texture record");
      size_t size = r.u32(p + 4);
      if (size > end - p - 32)
        throw std::runtime_error("Actor texture record outside extent");
      p += 32 + size;
    }
    throw std::runtime_error("Actor texture record limit exceeded");
  };
  std::map<size_t, Bytes> cache;
  size_t texture_slot = 1; // Room geometry consumes the first entry.
  for (size_t i = 0; i < banks.size(); ++i) {
    int version = -1;
    const auto &enemy = records.enemies[i];
    for (auto &model : banks[i].models) {
      bool own = Reader{model.model}.u32(8) != 0xffffffff;
      if (own)
        ++version;
      if (version < 0)
        continue;
      // PS2 uses a linear byte address into et_lp[100][16][16]. Retail
      // model_version=32 aliases a later ID's slots within that allocation.
      size_t key = size_t(enemy.id) * 256 + size_t(enemy.model_version) * 16 +
                   size_t(version);
      if (key >= 100 * 16 * 16 || version >= 16)
        throw std::runtime_error(
            "Enemy texture cache index outside source limits: ID " +
            std::to_string(enemy.id) + ", model version " +
            std::to_string(enemy.model_version) + ", texture version " +
            std::to_string(version));
      auto found = cache.find(key);
      if (own && found == cache.end()) {
        if (texture_slot >= textures.size())
          throw std::runtime_error("Missing enemy texture pack");
        auto pack = texture(textures[texture_slot++]);
        charge(pack.size());
        found = cache.emplace(key, std::move(pack)).first;
      }
      if (found != cache.end()) {
        charge(found->second.size());
        model.textures = found->second;
      }
    }
  }
  if (include_props) {
    auto assign = [&](std::vector<ActorModel> &models,
                      const std::vector<RoomEntity> &entities, size_t first,
                      size_t cache_limit) {
      std::map<uint16_t, Bytes> cache;
      for (size_t i = first; i < models.size(); ++i) {
        auto &model = models[i];
        if (Reader{model.model}.u32(8) == 0xffffffff)
          continue;
        auto id = entities[i].id;
        if (id >= cache_limit)
          throw std::runtime_error("Room prop texture ID outside source cache");
        auto found = cache.find(id);
        if (found == cache.end()) {
          if (texture_slot >= textures.size())
            throw std::runtime_error("Missing room prop texture pack");
          auto pack = texture(textures[texture_slot++]);
          charge(pack.size());
          found = cache.emplace(id, std::move(pack)).first;
        }
        charge(found->second.size());
        model.textures = found->second;
      }
    };
    assign(result.objects, records.objects, 4, 1300);
    assign(result.items, records.items, 0, 200);
  }
  return result;
}
RoomActorModels decode_room_actor_models(const Bytes &data) {
  return decode_room_actor_models_impl(data, true);
}
std::vector<ActorModelBank> decode_room_enemy_models(const Bytes &data) {
  return decode_room_actor_models_impl(data, false).enemies;
}
std::vector<SkinNode> decode_skin(const Bytes &data, size_t count) {
  // npSkinConvPreparation/npSkinConvSub and npCalcSkin use a ten-step
  // blend between a joint's unrotated and rotated transforms.
  Reader r{data};
  r.range(0, 4);
  if (std::string(data.begin(), data.begin() + 4) != "SKIN" || !count ||
      count > 4096)
    throw std::runtime_error("Invalid skin header");
  std::vector<SkinNode> nodes(count);
  std::array<bool, 16> groups{};
  size_t p = 4, total = 0;
  for (auto &node : nodes) {
    node.flags = r.u32(p);
    p += 4;
    if (node.flags & ~3u)
      throw std::runtime_error("Unsupported skin node flags");
    if (!node.flags)
      continue;
    node.group = r.u32(p);
    p += 4;
    if (node.group >= groups.size())
      throw std::runtime_error("Skin group outside limit");
    if (node.flags & 2) {
      if (groups[node.group])
        throw std::runtime_error("Duplicate skin group");
      groups[node.group] = true;
      auto n = r.u32(p);
      p += 4;
      total += n;
      if (n > 65535 || total > 1000000)
        throw std::runtime_error("Skin vertex limit exceeded");
      r.range(p, size_t(n) * 4);
      node.vertices.reserve(n);
      for (size_t i = 0; i < n; ++i) {
        auto packed = r.u32(p);
        p += 4;
        auto joint = packed >> 24, level = (packed >> 16) & 255;
        if (joint >= count || level > 9)
          throw std::runtime_error("Invalid skin vertex binding");
        node.vertices.push_back(
            {uint8_t(joint), float(level + 1) * 0.1f, uint16_t(packed)});
      }
    }
  }
  for (const auto &node : nodes)
    if (node.flags && !groups[node.group])
      throw std::runtime_error("Missing skin group");
  if (r.u32(p) != 0xffffffff || p + 4 != data.size())
    throw std::runtime_error("Invalid skin terminator");
  return nodes;
}
PlayerPackage decode_player_package(const Bytes &data, size_t key_limit) {
  if (data.size() > 64 * 1024 * 1024)
    throw std::runtime_error("Player package exceeds limit");
  Reader r{data};
  size_t p = 0;
  auto block = [&] {
    auto n = r.u32(p);
    p += 4;
    auto result = r.copy(p, n);
    p += n;
    return result;
  };
  auto models = block(), motions = block();
  PlayerPackage package;
  package.auxiliary = block();
  Bytes pending_skin;
  records(models, 64, [&](Bytes record) {
    if (record.empty())
      return;
    Reader model{record};
    model.range(0, 4);
    if (std::string(record.begin(), record.begin() + 4) == "SKIN") {
      if (!pending_skin.empty())
        throw std::runtime_error("Unbound player skin record");
      pending_skin = std::move(record);
      return;
    }
    model.range(0, 16);
    if (record[0] != 'M' || record[1] != 'D' || record[2] != 'L')
      throw std::runtime_error("Invalid player model signature");
    auto header = uint16_t(record[4]) | uint16_t(record[5]) << 8;
    if (header < 16)
      throw std::runtime_error("Invalid player model header");
    model.range(0, header);
    if (package.models.size() >= 16)
      throw std::runtime_error("Too many player models");
    package.models.push_back({std::move(record), std::move(pending_skin), {}});
    pending_skin.clear();
  });
  if (!pending_skin.empty())
    throw std::runtime_error("Unbound player skin record");
  records(motions, 512, [&](Bytes record) {
    if (record.empty())
      package.motions.push_back(std::nullopt);
    else {
      auto motion = decode_motion(record, key_limit);
      for (const auto &track : motion.tracks)
        key_limit -= track.positions.size() + track.angles.size();
      package.motions.push_back(std::move(motion));
    }
  });
  for (auto &model : package.models) {
    if (Reader{model.model}.u32(8) == 0xffffffff)
      continue;
    auto n = r.u32(p);
    p += 4;
    if (n & 0x80000000) {
      p = (p + 31) & ~size_t(31);
      n &= 0x7fffffff;
    }
    model.textures = r.copy(p, n);
    p += n;
  }
  // Retail packages end with an alignment word after their final texture.
  if (data.size() - p > 31)
    throw std::runtime_error("Trailing player package data");
  return package;
}
} // namespace ashford
