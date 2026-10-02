#include "actor.hpp"
// Layout adapted from MIT recvx-decomp dread.c::bhReadPlayerData.
#include <cstring>
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
