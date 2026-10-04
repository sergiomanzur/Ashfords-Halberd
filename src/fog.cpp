#include "fog.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp ps2_NaFog.c, cut.c and sync.c.
namespace ashford {
namespace {
uint32_t word(const Bytes &b, size_t p) {
  if (p > b.size() || b.size() - p < 4)
    throw std::runtime_error("Fog record outside room");
  return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
         uint32_t(b[p + 3]) << 24;
}
float real(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
void finite(const FogParameters &p) {
  if (!std::isfinite(p.near_z) || !std::isfinite(p.far_z))
    throw std::runtime_error("Nonfinite fog parameters");
}
int32_t decrement(int32_t count) {
  uint32_t bits = uint32_t(count) - 1;
  std::memcpy(&count, &bits, 4);
  return count;
}
} // namespace
FogParameters decode_room_fog(const Bytes &b) {
  room_info(b);
  const size_t rom = word(b, 16);
  FogParameters p{word(b, rom + 0x108), real(word(b, rom + 0x10c)),
                  real(word(b, rom + 0x110))};
  finite(p);
  return p;
}
FogTable generate_fog_table(float n, float f) {
  finite({0, n, f});
  if (n <= f || f > -1)
    throw std::runtime_error("Invalid consumed source fog range");
  FogTable result;
  if (n < -65535.f)
    result.density = 65536.f;
  else {
    int exponent = 0;
    const float fraction = std::frexp(-f, &exponent);
    const int count = exponent - 1;
    if (count < 0 || count > 30)
      throw std::runtime_error(
          "Fog density exponent outside source integer range");
    float k = fraction * 65536.f;
    const int tmp = (int(k) & 0xff00) | count;
    const int scale = 1 << count;
    k = (tmp & 0x7f00) / (65536.f / 2.f);
    const float m = (tmp >> 15) * scale;
    result.density = (k * scale) + m;
  }
  const float origin = f / 256.f > n ? -f : 1.f;
  int inside = 0;
  for (int count = 0; count < 128; ++count) {
    const float scale = 1 << (count / 16);
    const float x = float(count & 15) + 16.f;
    result.values[count] = origin / ((scale * x) / 16.f);
    if (result.values[count] >= -n)
      ++inside;
  }
  if (-f == origin) {
    if (inside > 1) {
      const float span = f + result.values[inside - 1];
      for (int count = 0; count < inside; ++count)
        result.values[count] = 1.f - ((f + result.values[count]) / span);
    } else if (inside == 1)
      result.values[0] = 1.f;
    for (; inside < 128; ++inside)
      result.values[inside] = 0;
  }
  for (auto &value : result.values)
    value = 255.f - (255.f * value);
  return result;
}
float fog_power(const NativeFog &fog, float inverse_z) {
  if (!fog.enabled)
    return 255.f;
  if (std::isnan(inverse_z) || !std::isfinite(fog.table.density) ||
      fog.table.density <= 0)
    throw std::runtime_error("Invalid consumed fog table or inverse-Z");
  auto value = [&](size_t i) {
    const float f = fog.table.values.at(i);
    if (!std::isfinite(f) || f < 0 || f > 255)
      throw std::runtime_error("Invalid consumed fog table value");
    return f;
  };
  if (inverse_z < 0)
    return value(127);
  float scaled = fog.table.density * inverse_z;
  scaled = std::clamp(scaled, 1.f, 255.9999f);
  uint32_t bits;
  std::memcpy(&bits, &scaled, 4);
  const int index = ((bits >> 19) & 15) | (((bits >> 23) - 127) * 16);
  if (index == 127)
    return value(127);
  const uint32_t base_bits =
      uint32_t((index / 16 + 127) << 23) | uint32_t((index & 15) << 19);
  constexpr float inv[] = {16, 8, 4, 2, 1, .5f, .25f, .125f};
  float blend = inv[index / 16];
  blend *= scaled - real(base_bits);
  return ((1.f - blend) * value(index)) + (blend * value(index + 1));
}
void select_fog_cut(FogWork &fog, const FogParameters &parameters, bool enabled,
                    uint32_t state_flags, uint32_t &game_flags,
                    uint32_t &effect_flags) {
  finite(parameters);
  if (fog.change_ticks < 0)
    throw std::runtime_error("Invalid fog change count");
  auto next = fog;
  next.camera = parameters;
  next.camera_enabled = enabled;
  next.countdown = 0;
  if ((state_flags & 2) && enabled) {
    next.countdown = std::max(0, next.change_ticks - 3);
    if (effect_flags & 0x10) {
      effect_flags &= ~0x10u;
      next.countdown = decrement(next.countdown);
    }
  }
  game_flags |= 0x10;
  fog = next;
}
void select_fog_eye(FogWork &fog, FogParameters &room, uint32_t state_flags,
                    uint32_t &game_flags) {
  finite(room);
  if (fog.change_ticks < 0)
    throw std::runtime_error("Invalid eye fog change count");
  auto parameters = room;
  if (parameters.near_z <= parameters.far_z)
    parameters = {0, 0, -100};
  auto next = fog;
  next.camera = parameters;
  next.camera_enabled = true;
  next.countdown = (state_flags & 2) ? std::max(0, next.change_ticks - 4) : 0;
  fog = next;
  room = parameters;
  game_flags |= 0x10;
}
void control_fog(FogWork &fog, uint32_t &state_flags, uint32_t &game_flags,
                 uint32_t camera_flags, std::optional<bool> keyframe_enabled) {
  auto next = fog;
  uint32_t st = state_flags, gm = game_flags;
  if (gm & 0x200000)
    next.active.enabled = false;
  else if (gm & 0x10) {
    bool update = true, enabled = next.camera_enabled;
    if (gm & 0x40) {
      enabled = true;
      if (next.countdown != 0) {
        next.countdown = decrement(next.countdown);
        update = false;
      } else
        gm &= ~0x10u;
    } else if (camera_flags & 2) {
      if (next.countdown != 0) {
        next.countdown = decrement(next.countdown);
        update = false;
      } else {
        if (!keyframe_enabled)
          throw std::runtime_error("Missing actual keyframe fog flag");
        enabled = *keyframe_enabled;
      }
    } else {
      if (next.countdown != 0)
        next.countdown = decrement(next.countdown);
      if (next.countdown > 0)
        update = false;
      else
        gm &= ~0x10u;
    }
    if (update) {
      next.active.enabled = enabled;
      if (enabled) {
        const auto &p = next.camera;
        next.active.table = generate_fog_table(p.near_z, p.far_z);
        next.active.color =
            ((st & 0x100000) ? next.override_color : p.color) & 0xffffff;
        st |= 2;
      } else
        st &= ~2u;
    }
  }
  fog = next;
  state_flags = st;
  game_flags = gm;
}
} // namespace ashford
