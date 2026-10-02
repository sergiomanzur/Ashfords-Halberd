#include "player.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ashford {
PlayerTranslationTables decode_player_translation_tables(const Bytes &data) {
  PlayerTranslationTables result;
  size_t cursor = 0, values = 0;
  auto read = [&](size_t p) {
    if (p > data.size() || data.size() - p < 4)
      throw std::runtime_error("Truncated player auxiliary record");
    return uint32_t(data[p]) | uint32_t(data[p + 1]) << 8 |
           uint32_t(data[p + 2]) << 16 | uint32_t(data[p + 3]) << 24;
  };
  for (;;) {
    uint32_t size = read(cursor);
    cursor += 4;
    if (size == 0xffffffff) {
      if (data.size() - cursor > 63)
        throw std::runtime_error("Excess player auxiliary padding");
      for (size_t i = cursor; i < data.size(); ++i)
        if (data[i])
          throw std::runtime_error("Nonzero player auxiliary padding");
      return result;
    }
    if (size % 4 || size > data.size() - cursor || result.size() >= 512 ||
        size / 4 > 2000000 - values)
      throw std::runtime_error("Invalid player auxiliary record size");
    std::vector<float> record;
    record.reserve(size / 4);
    for (size_t i = 0; i < size; i += 4) {
      uint32_t bits = read(cursor + i);
      float value;
      std::memcpy(&value, &bits, 4);
      if (!std::isfinite(value))
        throw std::runtime_error("Nonfinite player auxiliary value");
      record.push_back(value);
    }
    values += record.size();
    result.push_back(std::move(record));
    cursor += size;
  }
}
float player_translation_speed(const PlayerTranslationTables &tables,
                               size_t table, size_t frame) {
  if (table > 10 || table >= tables.size() || frame >= tables[table].size() ||
      tables[table].size() - frame < 2)
    throw std::runtime_error("Player translation frame outside table");
  return tables[table][frame + 1];
}
Vec3 player_add_speed(Vec3 position, uint16_t yaw, float speed,
                      uint16_t relative_angle) {
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z) || !std::isfinite(speed))
    throw std::runtime_error("Nonfinite player movement");
  static const auto table = [] {
    std::array<float, 16384> values{};
    for (size_t i = 0; i < values.size(); ++i)
      values[i] = std::sin(0.0000958738f * float(i));
    return values;
  }();
  uint16_t angle = uint16_t(yaw + relative_angle);
  size_t index = angle & 16383;
  float sine = table[(angle & 0x4000) ? 16383 - index : index];
  if (angle & 0x8000)
    sine = -sine;
  float cosine = table[(angle & 0x4000) ? index : 16383 - index];
  if (bool(angle & 0x4000) != bool(angle & 0x8000))
    cosine = -cosine;
  position.x -= speed * sine;
  position.z -= speed * cosine;
  if (!std::isfinite(position.x) || !std::isfinite(position.z))
    throw std::runtime_error("Player movement overflow");
  return position;
}
} // namespace ashford
