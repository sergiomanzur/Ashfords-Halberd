#include "enemy_motion_internal.hpp"
#include "enemy_targeting.hpp"
#include "ninja_matrix.hpp"
#include "player.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::NG00/NG01/NG02/ZulzulCheck/
// GakeotiCheck/FlyingCap and zonzon.c::LeverCheck/AngleCheck.
namespace ashford {
namespace {
int32_t add(int32_t a, int32_t b) {
  const uint32_t value = uint32_t(a) + uint32_t(b);
  return value < 0x80000000u ? int32_t(value)
                             : int32_t(int64_t(value) - 0x100000000ll);
}
int16_t short_word(int32_t value) {
  const uint16_t bits = uint16_t(value);
  return bits < 32768 ? int16_t(bits) : int16_t(int32_t(bits) - 65536);
}
float finite(float value) { return detail::ninja_finite(value); }
Vec3 vector(Vec3 p) {
  finite(p.x);
  finite(p.y);
  finite(p.z);
  return p;
}
const ScenarioEntity &root(const ScenarioState &s, size_t work) {
  if (s.enemies.size() != 128 || work >= s.enemies.size() ||
      !(s.enemies[work].flags & 1) || s.enemies[work].id != 1 ||
      !s.enemies[work].zombie_work)
    throw std::runtime_error("Invalid zombie grab work");
  return s.enemies[work];
}
const ScenarioEntity &cache_owner(const ScenarioState &s, size_t work) {
  const auto &e = s.enemies[work];
  if (!e.pose_owner)
    return e;
  const auto owner = *e.pose_owner;
  if (owner >= s.enemies.size() || !(s.enemies[owner].flags & 1) ||
      s.enemies[owner].pose_owner)
    throw std::runtime_error("Invalid grab cache owner");
  return s.enemies[owner];
}
struct Work {
  detail::MotionState motion;
  std::array<uint8_t, 4> modes;
  uint32_t zflags, secondary, zsecondary;
  int32_t yaw, turn, health, timer;
  Vec3 position, anchor;
  float speed;
  explicit Work(const ScenarioEntity &e)
      : motion(detail::snapshot(e)), modes(e.modes), zflags(e.zombie_flags),
        secondary(e.secondary_flags), zsecondary(e.zombie_secondary_flags),
        yaw(e.angles[1]), turn(e.yaw_correction), health(e.health),
        timer(e.control0), position(e.position),
        anchor(e.zombie_work->action_anchor), speed(e.speed) {}
  void commit(ScenarioEntity &e) const {
    detail::commit(e, motion);
    e.modes = modes;
    e.zombie_flags = zflags;
    e.secondary_flags = secondary;
    e.zombie_secondary_flags = zsecondary;
    e.angles[1] = yaw;
    e.yaw_correction = turn;
    e.health = health;
    e.control0 = timer;
    e.position = position;
    e.zombie_work->action_anchor = anchor;
    e.speed = speed;
  }
};
bool wall_recoil(const ScenarioState &s, Work &e, int32_t relative,
                 float distance) {
  if (!s.collision)
    throw std::runtime_error("Grab recoil requires original room collision");
  const auto ps =
      vector({e.position.x, finite(e.position.y + 5.f), e.position.z});
  const auto sc = ninja_sin_cos(uint16_t(uint32_t(e.yaw) + uint32_t(relative)));
  const Vec3 pd =
      vector({ps.x - distance * sc[0], ps.y, ps.z - distance * sc[1]});
  const auto result =
      query_collision_line(*s.collision, s.dynamic_walls, ps, pd);
  if (!result.wall)
    return false;
  const size_t index = *result.wall;
  const auto &hit = index < s.collision->walls.size()
                        ? s.collision->walls[index]
                        : s.dynamic_walls.at(index - s.collision->walls.size());
  if (hit.attribute & 8)
    return false;
  const int32_t angle = int32_t(
      finite(10430.381f * std::atan2(result.normal.x, result.normal.z)));
  e.turn = int32_t(
      uint16_t(uint32_t(uint16_t(uint32_t(angle) + uint32_t(relative))) -
               uint32_t(e.yaw)));
  if (e.turn > 32768)
    e.turn -= 65536;
  const auto opposite = ninja_sin_cos(uint16_t(uint32_t(e.yaw) + 32768u));
  e.anchor =
      vector({result.position.x - (ps.x - 1.5f * opposite[0]), e.position.y,
              result.position.z - (ps.z - 1.5f * opposite[1])});
  return true;
}
bool cliff(const ScenarioState &s, const ScenarioEntity &original,
           const Work &e) {
  if (!s.collision)
    throw std::runtime_error("Grab cliff query requires room collision");
  const auto *hit =
      enemy_floor_trigger(*s.collision, s.dynamic_floors, e.position,
                          original.floor, uint8_t(original.id), 1);
  if (!hit)
    return false;
  static constexpr int32_t angles[4] = {0, 49152, 32768, 16384};
  if (hit->parameters[2] >= 4)
    throw std::runtime_error("Cliff direction outside source table");
  const auto stair = ninja_sin_cos(uint16_t(angles[hit->parameters[2]]));
  Vec3 v = {-stair[0], 0, -stair[1]};
  const float squared = finite((v.x * v.x + v.y * v.y) + v.z * v.z);
  const float inverse = squared ? finite(1.f / std::sqrt(squared)) : 0.f;
  v = vector({v.x * inverse, v.y * inverse, v.z * inverse});
  const auto facing = ninja_sin_cos(uint16_t(uint32_t(e.yaw) + 32768u));
  const float inner =
      finite((-facing[0] * v.x + 0.f * v.y) + (-facing[1] * v.z));
  return inner > ninja_sin_cos(6144)[1];
}
struct CapFlight {
  size_t work;
  uint32_t flags, joint_flags;
  int16_t yaw, rotation, roll, point;
  float velocity;
  int8_t floor;
};
std::optional<CapFlight> cap_flight(const ScenarioState &s, size_t work,
                                    const Work &e) {
  const auto child = s.enemies[work].zombie_work->children[5];
  if (!child)
    return std::nullopt;
  if (*child >= s.enemies.size() || *child == work ||
      !(s.enemies[*child].flags & 1) || s.enemies[*child].id != 34 ||
      !s.enemies[*child].model_work ||
      s.enemies[*child].model_work->joints.empty())
    throw std::runtime_error("Missing consumed flying-cap work/cache");
  const auto &owner = cache_owner(s, work);
  if (owner.joint_pose.size() <= 10 || !owner.joint_pose[10].angles)
    throw std::runtime_error("Missing source cap attachment angles");
  const auto &cap = s.enemies[*child];
  return CapFlight{
      *child,
      cap.flags & ~0x80u,
      cap.model_work->joints[0].flags | 3u,
      short_word(add(32768, add(e.yaw, (*owner.joint_pose[10].angles)[1]))),
      0x800,
      0x1800,
      0,
      .6f,
      s.enemies[work].floor};
}
} // namespace
void tick_zombie_grab(ScenarioState &s, size_t work, ScenarioEntity &player,
                      const std::vector<int32_t> &playing) {
  const auto &original = root(s, work);
  for (const auto &other : s.enemies)
    if (&other == &player)
      throw std::runtime_error("Player aliases zombie grab pool");
  Work e(original);
  std::optional<size_t> body;
  std::optional<Work> linked;
  auto link = [&] {
    if (linked || !original.zombie_work->body)
      return;
    body = original.zombie_work->body;
    if (*body == work)
      throw std::runtime_error("Grab body aliases root");
    linked.emplace(root(s, *body));
  };
  auto change = [&](uint32_t main, uint32_t paired, int32_t frame,
                    int32_t blend) {
    detail::change(e.motion, main, frame, blend);
    e.zflags &= ~0x3000000u;
    link();
    if (linked) {
      detail::change(linked->motion, paired, frame, blend);
      linked->zflags &= ~0x3000000u;
    }
  };
  auto last_frame = [&] {
    if (!original.motions ||
        e.motion.motion_number >= original.motions->size() ||
        !original.motions->at(e.motion.motion_number) ||
        !original.motions->at(e.motion.motion_number)->frames)
      throw std::runtime_error("Missing grab authored motion frame count");
    return int64_t(e.motion.frame / 65536) ==
           int64_t(original.motions->at(e.motion.motion_number)->frames) - 1;
  };
  auto stop = [&](std::array<uint8_t, 4> modes) {
    e.motion.motion_step = 0;
    e.modes = modes;
    link();
    if (linked) {
      linked->motion.motion_step = 0;
      linked->modes = modes;
    }
  };
  auto correction = [&] {
    if (e.timer) {
      if (e.timer < 5)
        e.yaw = add(e.yaw, e.turn);
      e.position.x = finite(e.position.x + finite(e.anchor.x) / 5.f);
      e.position.z = finite(e.position.z + finite(e.anchor.z) / 5.f);
      e.timer = add(e.timer, -1);
    }
  };
  auto low_flags = [&](uint32_t flags) {
    e.zflags = (e.zflags & ~0xfu) | flags;
  };
  auto player_modes = player.modes;
  auto player_health = player.health;
  auto player_state = player.state_flags;
  auto player_timer = player.control1;
  auto seed = s.random_seed;
  std::optional<EnemySoundRequest> sound;
  std::optional<CapFlight> cap;
  auto play = [&] {
    if (auto code = choose_zombie_sound(original, seed, 8964, playing))
      sound = EnemySoundRequest{work, vector(e.position), *code};
  };
  switch (e.modes[3]) {
  case 0: {
    if (s.player_id == 1 || s.player_id == 3)
      change(120, 320, 0, 0);
    else
      change(8, 208, 0, 0);
    e.yaw = enemy_direction(e.position, player.position);
    e.motion.flags &= ~0x40u;
    low_flags(5);
    e.zflags &= ~0x4000000u;
    player_modes = {4, 0, 0, 0};
    e.secondary |= 1;
    Vec3 offset = {0, 0, 6.290813f};
    auto yaw = e.yaw;
    if (e.zflags & 0x4000) {
      offset.z = -6.442887f;
      yaw = int32_t(uint16_t(uint32_t(yaw) + 32768u));
    }
    auto matrix = detail::ninja_identity();
    detail::ninja_translate(matrix, player.position);
    detail::ninja_rotate(matrix, {0, yaw, 0});
    e.position = detail::ninja_point(matrix, offset);
    if (e.health < 0)
      e.health = 0;
    e.zflags |= 0x20000;
    ++e.modes[3];
  }
    [[fallthrough]];
  case 1:
    if (e.motion.frame / 65536 == 20) {
      e.timer = 60;
      ++e.modes[3];
    }
    break;
  case 2: {
    const int32_t frame = e.motion.frame / 65536;
    if (frame == 25 || frame == 60) {
      player_health =
          add(player_health,
              -(zombie_personality_at(original.zombie_work->personality)
                    .attack_bonus +
                10));
      if (player_health < 0)
        player_state |= 0x40000;
    }
    const int32_t lever = ((s.input_pressed & 0xf) ? 4 : 0) +
                          ((s.input_pressed & 0x2d00) ? 3 : 0);
    e.timer = add(e.timer, -lever - 1);
    if (e.timer <= 0 || (e.motion.flags & 0x2000000)) {
      if (player_health < 0) {
        change(34, 234, 0, 0);
        low_flags(5);
        e.modes[3] = 6;
        player_modes[0] = 6;
        player_modes[3] = 6;
      } else {
        change(7, 207, 0, 5);
        e.motion.flags |= 0x40000;
        ++e.modes[3];
        player_modes[3] = 2;
        player_timer = 0;
        low_flags(6);
        e.timer = 0;
        e.secondary &= ~1u;
      }
      e.motion.flags |= 0x20;
    }
    break;
  }
  case 3: {
    const int32_t frame = e.motion.frame / 65536;
    if (frame > 17 && frame < 31) {
      e.speed = finite(float(1.5 - double(e.timer) * .14));
      if (e.speed < 0)
        e.speed = .05f;
      e.position =
          player_add_speed(e.position, uint16_t(e.yaw), e.speed, 32768);
      e.timer = add(e.timer, 1);
    }
    if (frame == 20) {
      e.zflags |= 0x100;
      cap = cap_flight(s, work, e);
    }
    if (frame > 17 && wall_recoil(s, e, 32768, 4.5f)) {
      e.zflags &= ~0x100u;
      e.timer = 5;
      e.turn /= e.timer;
      e.motion.flags |= 0x40;
      if (cliff(s, original, e)) {
        change(124, 324, 0, 2);
        e.motion.flags &= ~0x20u;
        low_flags(0);
        e.modes[3] = 8;
        break;
      }
      change(50, 250, 0, 4);
      low_flags(3);
      e.modes[3] = 5;
      break;
    }
    if (last_frame()) {
      change(12, 212, 7 * 65536, 0);
      e.motion.flags |= 0x40040;
      low_flags(6);
      ++e.modes[3];
    }
    break;
  }
  case 4:
    if (e.motion.frame / 65536 == 19 && (original.state_flags & 0x100000))
      play();
    if (e.motion.frame / 65536 == 10)
      e.zflags &= ~0x100u;
    if (last_frame()) {
      e.secondary &= ~1u;
      low_flags(6);
      e.zflags |= 0x40000;
      e.zsecondary |= 0x40;
      stop({1, 0, 4, 0});
    }
    break;
  case 5:
    correction();
    if (last_frame()) {
      e.zflags &= ~0x200u;
      e.zflags |= 0x40000;
      stop({1, 0, 4, 0});
    }
    break;
  case 6:
    if (e.motion.frame / 65536 == 15 && (original.state_flags & 0x100000))
      play();
    if (e.motion.flags & 0x2000000) {
      const auto &owner = cache_owner(s, work);
      if (!owner.model_work || owner.model_work->joints.empty())
        throw std::runtime_error("Missing fatal grab cached root");
      const auto &matrix = owner.model_work->joints[0].matrix;
      e.position.x = finite(float(matrix[3]));
      e.position.z = finite(float(matrix[11]));
      e.zflags &= ~0x80000u;
      change(111, 311, 0, 0);
      low_flags(0);
      ++e.modes[3];
    }
    break;
  case 8:
    correction();
    if (e.motion.frame / 65536 >= 5 && e.motion.frame / 65536 < 32) {
      e.speed = .5f;
      e.position =
          player_add_speed(e.position, uint16_t(e.yaw), e.speed, 32768);
    }
    if (last_frame()) {
      e.health = -1;
      stop({4, 0, 3, 0});
    }
    break;
  case 9:
    if (last_frame()) {
      e.zflags |= 0x40000;
      e.zsecondary |= 0x40;
      stop({1, 0, 4, 0});
    }
    break;
  default:
    break;
  }
  if (sound)
    s.pending_enemy_sounds.reserve(s.pending_enemy_sounds.size() + 1);
  e.commit(s.enemies[work]);
  if (linked)
    linked->commit(s.enemies[*body]);
  if (cap) {
    auto &c = s.enemies[cap->work];
    c.flags = cap->flags;
    c.model_work->joints[0].flags = cap->joint_flags;
    c.turn_speed = cap->yaw;
    c.torso_yaw = cap->rotation;
    c.roll_speed = cap->roll;
    c.grab_yaw_offset = cap->point;
    c.damage_position.x = cap->velocity;
    c.floor = cap->floor;
    c.modes[0] = 3;
  }
  player.modes = player_modes;
  player.health = player_health;
  player.state_flags = player_state;
  player.control1 = player_timer;
  s.random_seed = seed;
  if (sound)
    s.pending_enemy_sounds.push_back(*sound);
}
void tick_zombie_failed_grab(ScenarioState &s, size_t work,
                             const std::vector<int32_t> &playing) {
  const auto &original = root(s, work);
  Work e(original);
  std::optional<size_t> body;
  std::optional<Work> linked;
  auto link = [&] {
    if (linked || !original.zombie_work->body)
      return;
    body = original.zombie_work->body;
    if (*body == work)
      throw std::runtime_error("Failed-grab body aliases root");
    linked.emplace(root(s, *body));
  };
  auto change = [&](uint32_t main, uint32_t paired, int32_t frame,
                    int32_t blend) {
    detail::change(e.motion, main, frame, blend);
    e.zflags &= ~0x3000000u;
    link();
    if (linked) {
      detail::change(linked->motion, paired, frame, blend);
      linked->zflags &= ~0x3000000u;
    }
  };
  auto last_frame = [&] {
    if (!original.motions ||
        e.motion.motion_number >= original.motions->size() ||
        !original.motions->at(e.motion.motion_number) ||
        !original.motions->at(e.motion.motion_number)->frames)
      throw std::runtime_error("Missing failed-grab authored frame count");
    return int64_t(e.motion.frame / 65536) ==
           int64_t(original.motions->at(e.motion.motion_number)->frames) - 1;
  };
  auto low_flags = [&](uint32_t flags) {
    e.zflags = (e.zflags & ~0xfu) | flags;
  };
  auto correction = [&] {
    if (e.timer) {
      if (e.timer < 5)
        e.yaw = add(e.yaw, e.turn);
      e.position.x = finite(e.position.x + finite(e.anchor.x) / 5.f);
      e.position.z = finite(e.position.z + finite(e.anchor.z) / 5.f);
      e.timer = add(e.timer, -1);
    }
  };
  auto stop = [&](std::array<uint8_t, 4> modes) {
    e.motion.motion_step = 0;
    e.modes = modes;
    link();
    if (linked) {
      linked->motion.motion_step = 0;
      linked->modes = modes;
    }
  };
  auto seed = s.random_seed;
  std::optional<EnemySoundRequest> sound;
  switch (e.modes[3]) {
  case 0:
    change(7, 207, 16 * 65536, 5);
    e.motion.flags |= 0x40000;
    low_flags(6);
    e.timer = 0;
    ++e.modes[3];
    [[fallthrough]];
  case 1: {
    const int32_t frame = e.motion.frame / 65536;
    if (frame > 17 && frame < 31) {
      e.speed = finite(float(1.5 - double(e.timer) * .14));
      if (e.speed < 0)
        e.speed = .05f;
      e.position =
          player_add_speed(e.position, uint16_t(e.yaw), e.speed, 32768);
      e.timer = add(e.timer, 1);
    }
    if (frame > 17 && wall_recoil(s, e, 32768, 4.5f)) {
      e.timer = 5;
      e.turn /= e.timer;
      if (cliff(s, original, e)) {
        change(124, 324, 0, 2);
        e.motion.flags &= ~0x20u;
        low_flags(0);
        e.modes[3] = 4;
        break;
      }
      change(50, 250, 0, 4);
      low_flags(3);
      e.modes[3] = 3;
      break;
    }
    if (last_frame()) {
      change(12, 212, 7 * 65536, 0);
      e.motion.flags |= 0x40000;
      low_flags(6);
      ++e.modes[3];
    }
    break;
  }
  case 2:
    if (e.motion.frame / 65536 == 20 && (original.state_flags & 0x100000)) {
      if (auto code = choose_zombie_sound(original, seed, 8964, playing))
        sound = EnemySoundRequest{work, vector(e.position), *code};
    }
    if (last_frame()) {
      low_flags(6);
      e.zflags |= 0x40000;
      e.zsecondary |= 0x40;
      stop({1, 0, 4, 0});
    }
    break;
  case 3:
    correction();
    if (last_frame()) {
      e.zflags &= ~0x200u;
      e.zflags |= 0x40000;
      stop({1, 0, 4, 0});
    }
    break;
  case 4:
    correction();
    if (e.motion.frame / 65536 >= 5 && e.motion.frame / 65536 < 32) {
      e.speed = .5f;
      e.position =
          player_add_speed(e.position, uint16_t(e.yaw), e.speed, 32768);
    }
    if (last_frame()) {
      e.health = -1;
      stop({4, 0, 3, 0});
    }
    break;
  default:
    break;
  }
  if (sound)
    s.pending_enemy_sounds.reserve(s.pending_enemy_sounds.size() + 1);
  e.commit(s.enemies[work]);
  if (linked)
    linked->commit(s.enemies[*body]);
  s.random_seed = seed;
  if (sound)
    s.pending_enemy_sounds.push_back(*sound);
}
void tick_zombie_standing_grab(ScenarioState &s, size_t work,
                               ScenarioEntity &player,
                               const std::vector<int32_t> &playing) {
  const auto &original = root(s, work);
  for (const auto &other : s.enemies)
    if (&other == &player)
      throw std::runtime_error("Player aliases standing-grab enemy pool");
  Work e(original);
  auto head = original.zombie_work->head_damage;
  auto velocity = original.damage_velocity;
  auto player_modes = player.modes;
  auto player_health = player.health;
  auto player_timer = player.control1;
  std::optional<size_t> body;
  std::optional<Work> linked;
  auto link = [&] {
    if (linked || !original.zombie_work->body)
      return;
    body = original.zombie_work->body;
    if (*body == work)
      throw std::runtime_error("Standing-grab body aliases root");
    linked.emplace(root(s, *body));
  };
  auto change = [&](uint32_t main, uint32_t paired, int32_t frame,
                    int32_t blend) {
    detail::change(e.motion, main, frame, blend);
    e.zflags &= ~0x3000000u;
    link();
    if (linked) {
      detail::change(linked->motion, paired, frame, blend);
      linked->zflags &= ~0x3000000u;
    }
  };
  auto last_frame = [&] {
    if (!original.motions ||
        e.motion.motion_number >= original.motions->size() ||
        !original.motions->at(e.motion.motion_number) ||
        !original.motions->at(e.motion.motion_number)->frames)
      throw std::runtime_error("Missing standing-grab authored frame count");
    return int64_t(e.motion.frame / 65536) ==
           int64_t(original.motions->at(e.motion.motion_number)->frames) - 1;
  };
  // The synchronous creators replace only pool/RNG. Restore them if a later
  // consumed frame count or paired work fails, without copying any model data.
  struct BirthRollback {
    ScenarioState &state;
    std::shared_ptr<const NativeEffectPool> pool;
    uint32_t seed;
    bool committed = false;
    ~BirthRollback() {
      if (!committed) {
        state.effect_pool = std::move(pool);
        state.random_seed = seed;
      }
    }
  } rollback{s, s.effect_pool, s.random_seed};
  auto seed = s.random_seed;
  std::optional<EnemySoundRequest> sound;
  auto cut = [&](int32_t code) {
    head = -1; // Source CutHead ignores its type and only sets EXP0_I(0x78).
    if (auto selected = choose_zombie_sound(original, seed, code, playing))
      sound = EnemySoundRequest{work, vector(e.position), *selected};
    e.health = -1;
    e.motion.flags &= ~0x20u;
  };
  switch (e.modes[3]) {
  case 0: {
    const int32_t frame = e.motion.motion_number == 15 ? 0 : 10 * 65536;
    change(9, 209, frame, 10);
    e.motion.flags &= ~0x40u;
    e.secondary |= 1;
    player_modes = {4, 0, 1, 0};
    e.zflags |= 0x20000;
    const bool back = e.zflags & 0x4000;
    const int32_t yaw =
        back ? int32_t(uint16_t(uint32_t(e.yaw) + 32768u)) : e.yaw;
    auto matrix = detail::ninja_identity();
    detail::ninja_translate(matrix, player.position);
    detail::ninja_rotate(matrix, {0, yaw, 0});
    e.position = detail::ninja_point(matrix, {0, 0, back ? -8.f : 8.844476f});
    ++e.modes[3];
  }
    [[fallthrough]];
  case 1:
    if (e.motion.frame / 65536 == 10) {
      e.timer = 0x50;
      ++e.modes[3];
    }
    break;
  case 2:
    if (e.motion.frame / 65536 == 27) {
      player_health =
          add(player_health,
              -(zombie_personality_at(original.zombie_work->personality)
                    .attack_bonus +
                5));
      if (player_health < 0)
        player_health = 0;
    }
    if ((e.motion.flags & 0x2000000) || e.timer <= 0) {
      const bool claire = s.player_id == 0;
      change(claire ? 36 : 35, claire ? 236 : 235, 0, 5);
      e.motion.flags |= 0x40000;
      player_modes[3] = 2;
      player_timer = 5;
      ++e.modes[3];
      e.timer = 0;
    }
    break;
  case 3: {
    const int32_t frame = e.motion.frame / 65536;
    if (e.motion.motion_number == 36) {
      if (frame == 24)
        tick_zombie_neck_blood_effect(s, work, 0);
      else if (frame == 25)
        cut(4877);
    } else if (e.motion.motion_number == 35) {
      if (frame == 33) {
        velocity = {};
        tick_zombie_head_fragment_effect(s, work, 1);
        seed = s.random_seed;
      } else if (frame == 34)
        cut(4869);
      else if (frame == 35)
        tick_zombie_neck_blood_effect(s, work, 0);
    }
    if (last_frame()) {
      e.motion.motion_step = 0;
      link();
      if (linked) {
        linked->motion.motion_step = 0;
        linked->modes = {4, 2, 0, 0};
      }
      e.zflags &= ~0x800fu;
      e.modes = {4, 2, 0, 0};
    }
    break;
  }
  default:
    break;
  }
  if (sound)
    s.pending_enemy_sounds.reserve(s.pending_enemy_sounds.size() + 1);
  e.commit(s.enemies[work]);
  if (linked)
    linked->commit(s.enemies[*body]);
  s.enemies[work].zombie_work->head_damage = head;
  s.enemies[work].damage_velocity = velocity;
  player.modes = player_modes;
  player.health = player_health;
  player.control1 = player_timer;
  s.random_seed = seed;
  if (sound)
    s.pending_enemy_sounds.push_back(*sound);
  rollback.committed = true;
}
} // namespace ashford
