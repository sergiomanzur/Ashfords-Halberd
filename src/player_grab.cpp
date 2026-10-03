#include "enemy_motion_internal.hpp"
#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::PlyDG00/EnemyPushChk
// and zonzon.c::bhDGCdirCheck/bhEne_PlayerSePlay. Sampling follows separately.
namespace ashford {
namespace {
int32_t word(uint32_t v) {
  return v < 0x80000000u ? int32_t(v) : int32_t(int64_t(v) - 0x100000000ll);
}
int32_t add(int32_t a, int32_t b) { return word(uint32_t(a) + uint32_t(b)); }
int16_t short_word(int32_t v) {
  auto bits = uint16_t(v);
  return bits < 32768 ? int16_t(bits) : int16_t(int32_t(bits) - 65536);
}
uint32_t random(uint32_t &seed) {
  seed = seed * 1103515245u + 12345u;
  return seed & 0x7fffffffu;
}
float finite(float v) { return detail::ninja_finite(v); }
Vec3 vector(Vec3 v) {
  finite(v.x);
  finite(v.y);
  finite(v.z);
  return v;
}
const ScenarioEntity &root(const ScenarioState &s, size_t work) {
  if (s.enemies.size() != 128 || work >= s.enemies.size() ||
      !(s.enemies[work].flags & 1) || s.enemies[work].id != 1 ||
      !s.enemies[work].zombie_work)
    throw std::runtime_error("Invalid player grab enemy work");
  return s.enemies[work];
}
void no_alias(const ScenarioState &s, const ScenarioEntity &p) {
  for (const auto &e : s.enemies)
    if (&e == &p)
      throw std::runtime_error("Player aliases grab enemy pool");
}
bool direction(Vec3 v, int32_t yaw) {
  vector(v);
  const float squared = finite((v.x * v.x + v.y * v.y) + v.z * v.z);
  const float inverse = squared ? finite(1.f / std::sqrt(squared)) : 0.f;
  v = vector({v.x * inverse, v.y * inverse, v.z * inverse});
  const auto sc = ninja_sin_cos(uint16_t(yaw));
  return finite((-sc[0] * v.x + 0.f * v.y) + (-sc[1] * v.z)) >= 0.f;
}
struct PushWork {
  std::array<uint8_t, 4> modes;
  uint32_t zflags;
  Vec3 velocity;
};
struct PushBatch {
  std::array<std::optional<PushWork>, 128> changes;
  PushWork read(const ScenarioState &s, size_t i) const {
    if (changes[i])
      return *changes[i];
    const auto &e = s.enemies[i];
    return {e.modes, e.zombie_flags, e.damage_velocity};
  }
  void commit(ScenarioState &s) const {
    for (size_t i = 0; i < changes.size(); ++i)
      if (changes[i]) {
        s.enemies[i].modes = changes[i]->modes;
        s.enemies[i].zombie_flags = changes[i]->zflags;
        s.enemies[i].damage_velocity = changes[i]->velocity;
      }
  }
};
PushBatch push(const ScenarioState &s, size_t work, const ScenarioEntity &p,
               int mode, uint32_t &seed) {
  const auto &attacker = root(s, work);
  PushBatch out;
  for (size_t i = 0; i < s.enemies.size(); ++i) {
    const auto &e = s.enemies[i];
    auto candidate = out.read(s, i);
    if ((e.id != 1 && e.id != 26) || !(e.flags & 1) || (e.flags & 0x82) ||
        i == work || e.type == 2 || e.type == 3 || e.type == 6 || e.type == 7 ||
        e.type == 9 || e.type == 10 || candidate.modes[0] != 1 ||
        !e.zombie_work || (candidate.zflags & 0x440200))
      continue;
    // Source returns from the entire scan, before even measuring distance.
    if (mode == 1 && random(seed) % 3 != 0)
      return out;
    vector(p.position);
    vector(e.position);
    const auto delta =
        vector({e.position.x - p.position.x, e.position.y - p.position.y,
                e.position.z - p.position.z});
    const float squared =
        finite((delta.x * delta.x + delta.y * delta.y) + delta.z * delta.z);
    if (std::sqrt(squared) >= 10.f)
      continue;
    const Vec3 planar = {delta.x, 0, delta.z};
    const bool hit = !direction(planar, e.angles[1]);
    if (hit)
      candidate.zflags |= 0x2000;
    else
      candidate.zflags &= ~0x2000u;
    candidate.modes[0] = 3;
    candidate.modes[1] = 0;
    candidate.modes[3] = 0;
    if (attacker.zombie_work->body) {
      const size_t b = *attacker.zombie_work->body;
      if (b >= s.enemies.size() || b == work || !(s.enemies[b].flags & 1))
        throw std::runtime_error("Invalid grab push attacker body");
      auto body = out.read(s, b);
      body.modes[0] = 3;
      out.changes[b] = body;
    }
    const bool player_front = direction(planar, p.angles[1]);
    const bool back = attacker.zombie_flags & 0x4000;
    if (e.id == 26 || player_front != back)
      candidate.modes[2] = 2;
    else {
      const auto sc = ninja_sin_cos(uint16_t(e.angles[1]));
      candidate.velocity = {-sc[0], 0, -sc[1]};
      candidate.modes[2] = hit ? 4 : 3;
    }
    candidate.zflags |= 0x200;
    out.changes[i] = candidate;
  }
  return out;
}
constexpr Vec3 held_offsets[8] = {
    {0, 0, -6.326351f}, {0, 0, -6.326351f},         {0, 0, -5.f},
    {0, 0, -6.019392f}, {.167357f, 0, -5.7556629f}, {0, 0, -5.9214029f},
    {0, 0, -5.f},       {0, 0, -6.019392f}};
constexpr Vec3 lunge_offsets[8] = {
    {.469302f, 0, -5.826981f}, {.469231f, 0, -5.499186f}, {.2f, 0, -4.5f},
    {.4f, 0, -5.1999998f},     {.469302f, 0, -5.f},       {.469231f, 0, -4.9f},
    {.2f, 0, -4.5f},           {.4f, 0, -5.1999998f}};
constexpr uint32_t offsets[4] = {400, 425, 450, 425};
} // namespace
void push_zombie_grab_neighbors(ScenarioState &s, size_t work,
                                const ScenarioEntity &p, int mode) {
  no_alias(s, p);
  auto seed = s.random_seed;
  auto result = push(s, work, p, mode, seed);
  result.commit(s);
  s.random_seed = seed;
}
void tick_zombie_player_grab(ScenarioState &s, size_t work, ScenarioEntity &p,
                             const std::vector<int32_t> &playing) {
  const auto &e = root(s, work);
  if (!(e.zombie_flags & 0x20000) || (p.modes[0] != 4 && p.modes[0] != 6))
    return;
  no_alias(s, p);
  if (p.modes[2] == 1) {
    tick_zombie_player_standing_grab(s, work, p);
    return;
  }
  if (p.modes[2] != 0)
    throw std::runtime_error("Invalid player grab callback mode");
  auto m = detail::snapshot(p);
  auto bank = p.motions;
  auto modes = p.modes;
  auto zflags = e.zombie_flags;
  auto anchor = e.zombie_work->action_anchor;
  auto yaw = p.yaw_correction, timer = p.control0;
  auto yaw_offset = e.grab_yaw_offset;
  auto status = p.player_work ? p.player_work->status_flags : 0u;
  auto state_flags = p.state_flags, attack_flags = p.attack_flags,
       input = s.input_held;
  auto seed = s.random_seed;
  std::optional<int32_t> voice;
  std::optional<EnemySoundRequest> sound;
  std::optional<PushBatch> pushed;
  const bool back = zflags & 0x4000;
  auto player_status = [&] {
    if (!p.player_work)
      throw std::runtime_error("Missing player grab EXP1 status");
  };
  auto clip = [&](uint32_t relative) {
    if (s.player_id >= 4)
      throw std::runtime_error("Invalid player grab character");
    m.motion_number = offsets[s.player_id] + relative;
    if (!bank || m.motion_number >= bank->size() ||
        !bank->at(m.motion_number) || !bank->at(m.motion_number)->frames)
      throw std::runtime_error("Missing player grab motion clip");
  };
  auto last_frame = [&] {
    if (!bank || m.motion_number >= bank->size() ||
        !bank->at(m.motion_number) || !bank->at(m.motion_number)->frames ||
        m.frame < 0)
      throw std::runtime_error("Missing player grab release frame count");
    return uint32_t(m.frame / 65536) >= bank->at(m.motion_number)->frames - 1;
  };
  auto restore = [&] {
    if (!p.base_motions)
      throw std::runtime_error("Missing player's retained base motion bank");
    bank = p.base_motions;
  };
  auto call_voice = [&](int32_t code) {
    if (!(e.flags & 0x10000))
      voice = code;
  };
  auto start_clip = [&] {
    bank = e.motions;
    m.frame = 0;
    m.blend_count = 5;
    m.blend_rate = 42598;
    m.motion_step = 65536;
    m.motion_mode = 0;
  };
  auto align = [&] {
    yaw = int32_t(uint16_t(uint32_t(e.angles[1]) + (back ? 32768u : 0u) -
                           uint32_t(p.angles[1])));
    yaw_offset = back ? -32768 : 0;
    if (yaw > 32768)
      yaw -= 65536;
    yaw_offset = short_word(int32_t(yaw_offset) - yaw);
    yaw /= 5;
    timer = 0;
  };
  auto alignment_tick = [&] {
    if (timer < 5)
      yaw_offset = short_word(int32_t(yaw_offset) + yaw);
    else if (timer == 5)
      yaw_offset = back ? -32768 : 0;
    timer = add(timer, 1);
  };
  auto offset = [&](const Vec3 *table) {
    if (s.player_id >= 4)
      throw std::runtime_error("Invalid grab offset character");
    anchor = table[s.player_id * 2 + (back ? 0 : 1)];
    zflags |= 0x80000;
  };
  auto cleanup = [&](uint32_t clear) {
    input &= ~0xfu;
    m.flags &= ~0x10000u;
    state_flags &= ~0x10000u;
    attack_flags = 0;
    m.flags &= ~4u;
    zflags &= ~clear;
  };
  switch (modes[3]) {
  case 0:
    player_status();
    m.flags &= ~0x40000u;
    status = (status | 0x1e0) & ~4u;
    align();
    ++modes[3];
    [[fallthrough]];
  case 1: {
    alignment_tick();
    const int32_t frame = e.frame / 65536;
    if (frame == 1) {
      start_clip();
      clip(back ? 6 : 7);
      offset(held_offsets);
    } else if (frame == 12)
      call_voice(1026);
    break;
  }
  case 2:
    player_status();
    status |= 4;
    m.frame = 0;
    m.blend_count = 0;
    m.blend_rate = 32768;
    m.motion_step = 65536;
    clip(back ? 10 : 11);
    zflags &= ~0x80000u;
    ++modes[3];
    [[fallthrough]];
  case 3: {
    const int32_t frame = m.frame / 65536;
    if (frame == 14 || frame == 17)
      pushed = push(s, work, p, frame == 14 ? 1 : 0, seed);
    if (last_frame()) {
      restore();
      cleanup(0x24080);
      m.flags |= 8;
      modes = {1, 0, 0, 0};
    }
    break;
  }
  case 4:
    player_status();
    m.flags &= ~0x40000u;
    status = (status | 0x1e0) & ~4u;
    start_clip();
    clip(back ? 8 : 9);
    offset(lunge_offsets);
    call_voice(1026);
    align();
    ++modes[3];
    [[fallthrough]];
  case 5:
    alignment_tick();
    break;
  case 6:
    m.frame = 0;
    m.blend_count = 0;
    m.blend_rate = 32768;
    m.motion_step = 65536;
    m.flags &= ~0x10u;
    clip(back ? 12 : 13);
    call_voice(1025);
    ++modes[3];
    [[fallthrough]];
  case 7: {
    const int32_t frame = m.frame / 65536;
    if ((back && (frame == 22 || frame == 30)) ||
        (!back && (frame == 16 || frame == 22))) {
      if (auto code = choose_zombie_sound(e, seed, 8963, playing))
        sound = EnemySoundRequest{work, vector(e.position), *code};
    }
    if (last_frame()) {
      m.motion_step = 0;
      cleanup(0xa4080);
      m.flags |= 2;
    }
    break;
  }
  case 8:
    player_status();
    status |= 4;
    restore();
    cleanup(0xa4080);
    m.flags |= 0xc;
    modes = {2, 1, 1, 0};
    break;
  default:
    break;
  }
  if (sound)
    s.pending_enemy_sounds.reserve(s.pending_enemy_sounds.size() + 1);
  if (voice)
    s.pending_player_voices.reserve(s.pending_player_voices.size() + 1);
  if (pushed)
    pushed->commit(s);
  auto &enemy = s.enemies[work];
  enemy.zombie_flags = zflags;
  enemy.zombie_work->action_anchor = anchor;
  enemy.grab_yaw_offset = yaw_offset;
  detail::commit(p, m);
  p.motions = bank;
  p.modes = modes;
  p.yaw_correction = yaw;
  p.control0 = timer;
  if (p.player_work)
    p.player_work->status_flags = status;
  p.state_flags = state_flags;
  p.attack_flags = attack_flags;
  s.input_held = input;
  s.random_seed = seed;
  if (sound)
    s.pending_enemy_sounds.push_back(*sound);
  if (voice)
    s.pending_player_voices.push_back(*voice);
}
} // namespace ashford
