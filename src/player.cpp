#include "player.hpp"
#include "ninja_math.hpp"
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
  uint16_t angle = uint16_t(yaw + relative_angle);
  auto trig = ninja_sin_cos(angle);
  position.x -= speed * trig[0];
  position.z -= speed * trig[1];
  if (!std::isfinite(position.x) || !std::isfinite(position.z))
    throw std::runtime_error("Player movement overflow");
  return position;
}
} // namespace ashford
