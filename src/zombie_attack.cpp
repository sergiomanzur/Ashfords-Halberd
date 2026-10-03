#include "enemy_motion_internal.hpp"
#include "enemy_movement.hpp"
#include "enemy_targeting.hpp"
#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::MV05/EatCheck and
// njplus.c::npCollisionCheckSC with ps2_NaColi.c's float projection/order.
namespace ashford {
namespace {
float finite(float v) { return detail::ninja_finite(v); }
Vec3 vector(Vec3 v) {
  finite(v.x);
  finite(v.y);
  finite(v.z);
  return v;
}
Vec3 subtract(Vec3 a, Vec3 b) {
  return vector({a.x - b.x, a.y - b.y, a.z - b.z});
}
float dot(Vec3 a, Vec3 b) {
  return finite((a.x * b.x + a.y * b.y) + a.z * b.z);
}
float length(Vec3 v) { return std::sqrt(dot(v, v)); }
Vec3 unit(Vec3 v) {
  const float squared = dot(v, v);
  const float inv = squared ? 1.f / std::sqrt(squared) : 0.f;
  return vector({v.x * inv, v.y * inv, v.z * inv});
}
bool sphere_capsule(Vec3 center, const EnemyGeometry &capsule) {
  center = vector(center);
  const auto first = vector(capsule.capsule_start),
             last = vector(capsule.capsule_end);
  const float radius = finite(capsule.capsule_radius);
  if (radius < 0)
    throw std::runtime_error("Negative player capsule radius");
  const float sum = finite(1.f + radius);
  auto line = subtract(last, first);
  const auto normalized = unit(line);
  const auto relative = subtract(center, first);
  const float projection = dot(normalized, relative);
  Vec3 closest = vector({first.x + normalized.x * projection,
                         first.y + normalized.y * projection,
                         first.z + normalized.z * projection});
  const float segment_length = length(line);
  auto offset = subtract(closest, first);
  const float along = length(offset);
  const float inner = dot(unit(offset), unit(line));
  if (inner > 0) {
    if (along > finite(segment_length + sum))
      return false;
    if (along > segment_length)
      closest = last;
  } else {
    if (along > sum)
      return false;
    closest = first;
  }
  const auto delta = subtract(closest, center);
  return dot(delta, delta) <= finite(sum * sum);
}
const ScenarioEntity &root(const ScenarioState &s, size_t work) {
  if (s.enemies.size() != 128 || work >= s.enemies.size() ||
      !(s.enemies[work].flags & 1) || s.enemies[work].id != 1 ||
      !s.enemies[work].zombie_work)
    throw std::runtime_error("Invalid zombie attack work");
  return s.enemies[work];
}
const ActorModelWork &cache(const ScenarioState &s, const ScenarioEntity &e) {
  if (e.pose_owner) {
    if (*e.pose_owner >= s.enemies.size() ||
        !(s.enemies[*e.pose_owner].flags & 1) ||
        s.enemies[*e.pose_owner].pose_owner)
      throw std::runtime_error("Invalid capture cache owner");
    if (s.enemies[*e.pose_owner].model_work)
      return *s.enemies[*e.pose_owner].model_work;
  } else if (e.model_work)
    return *e.model_work;
  throw std::runtime_error("Missing capture cached model work");
}
Vec3 joint(const ActorModelWork &work, size_t index, bool planar = false) {
  if (index >= work.joints.size())
    throw std::runtime_error("Missing capture cached joint");
  const auto &m = work.joints[index].matrix;
  return vector({float(m[3]), planar ? 0.f : float(m[7]), float(m[11])});
}
bool eat(const ScenarioState &s, const ScenarioEntity &enemy,
         const ScenarioEntity &player, int32_t cone, float distance, int mode) {
  if ((enemy.zombie_flags & 0x20000000) || (player.flags & 2) ||
      (player.state_flags & 0x80000000))
    return false;
  auto ps = joint(cache(s, player), 3, true);
  const auto &owner = cache(s, enemy);
  auto eps = joint(owner, 10, true);
  ps.y = eps.y = finite(player.position.y);
  const float separation = length(subtract(eps, ps));
  switch (mode) {
  case 0:
    if (search_enemy_player(eps, enemy.angles[1], player.position, cone) ==
            -1 ||
        separation >= finite(distance) ||
        player.position.y != finite(enemy.position.y) || player.health < 0)
      return false;
    return true;
  case 1:
    return player.position.y == finite(enemy.position.y) && player.health >= 0;
  case 2: {
    const auto center = detail::ninja_point(
        detail::ninja_load(owner.joints[10].matrix), {0, 0, -4});
    return sphere_capsule(center, player.geometry) &&
           player.position.y == finite(enemy.position.y) &&
           !(player.flags & 2) && !(player.state_flags & 0x80000000) &&
           player.health >= 0;
  }
  default:
    return false;
  }
}
int32_t add(int32_t a, int32_t b) {
  const uint32_t v = uint32_t(a) + uint32_t(b);
  return v < 0x80000000u ? int32_t(v) : int32_t(int64_t(v) - 0x100000000ll);
}
} // namespace
bool zombie_capture_check(const ScenarioState &s, size_t work,
                          const ScenarioEntity &player, int32_t cone,
                          float distance, int mode) {
  return eat(s, root(s, work), player, cone, distance, mode);
}
void tick_zombie_lunge(ScenarioState &s, size_t work, ScenarioEntity &player,
                       const std::vector<int32_t> &playing) {
  const auto &e = root(s, work);
  for (const auto &other : s.enemies)
    if (&other == &player)
      throw std::runtime_error("Player aliases enemy attack pool");
  auto motion = detail::snapshot(e);
  auto modes = e.modes;
  auto zflags = e.zombie_flags;
  auto yaw = e.angles[1], timer = e.control0;
  auto anchor = e.zombie_work->action_anchor;
  auto player_flags = player.flags;
  auto player_modes = player.modes;
  auto seed = s.random_seed;
  std::optional<EnemySoundRequest> sound;
  std::optional<size_t> body;
  std::optional<detail::MotionState> body_motion;
  uint32_t body_flags = 0;
  std::array<uint8_t, 4> body_modes{};
  auto link = [&] {
    if (body_motion || !e.zombie_work->body)
      return;
    body = e.zombie_work->body;
    if (*body == work)
      throw std::runtime_error("Lunge body aliases root");
    const auto &b = root(s, *body);
    body_motion = detail::snapshot(b);
    body_flags = b.zombie_flags;
    body_modes = b.modes;
  };
  auto change = [&](uint32_t main, uint32_t linked, int32_t blend) {
    detail::change(motion, main, 0, blend);
    zflags &= ~0x3000000u;
    link();
    if (body_motion) {
      detail::change(*body_motion, linked, 0, blend);
      body_flags &= ~0x3000000u;
    }
  };
  auto probe = [&] {
    ScenarioEntity out;
    out.flags = motion.flags;
    out.position = e.position;
    out.angles[1] = yaw;
    out.zombie_flags = zflags;
    out.pose_owner = e.pose_owner;
    // Reference cache belongs to the original work, not the scalar probe.
    return out;
  };
  switch (modes[3]) {
  case 0: {
    change(122, 322, 8);
    zflags |= 0x1000000;
    if (body_motion)
      body_flags |= 0x1000000;
    yaw = enemy_direction(e.position, player.position);
    anchor = {e.position.x, 0, e.position.z};
    if (auto code = choose_zombie_sound(e, seed, 16782102, playing)) {
      vector(e.position);
      sound = EnemySoundRequest{work, e.position, *code};
    }
    ++modes[3];
  }
    [[fallthrough]];
  case 1: {
    const int32_t frame = motion.frame / 65536;
    if (frame < 13 && !(motion.flags & 0x80)) {
      auto turn = probe();
      yaw = add(yaw,
                enemy_turn_step(turn, joint(cache(s, player), 0, true), 1536));
    }
    bool hit = false;
    if (frame >= 8 && frame < 13 && !(player.flags & 4)) {
      // EatCheck consumes root cache through its original owner; yaw/flags
      // come from this staged action, so no geometry copy is needed.
      auto attack = probe();
      attack.pose_owner = e.pose_owner.value_or(work);
      if (eat(s, attack, player, 3640, 5.5f, 0)) {
        player_flags |= 0x10004;
        zflags |= 0x80;
        motion.flags &= ~0x40u;
        player_modes = {4, 0, 0, 4};
        zflags |= 0x20000;
        if (uint16_t(uint32_t(player.angles[1]) - uint32_t(yaw) + 16384u) >=
            32768)
          zflags |= 0x4000;
        else
          zflags &= ~0x4000u;
        if (s.player_id == 1 || s.player_id == 3)
          change(121, 321, 5);
        else
          change(85, 285, 5);
        yaw = enemy_direction(e.position, player.position);
        zflags = (zflags & ~0xfu) | 5u;
        modes = {2, 0, 0, 2};
        if (body_motion)
          body_modes = modes;
        timer = 75;
        hit = true;
      }
    }
    if (frame == 12 && !hit)
      ++modes[3];
    break;
  }
  case 2: {
    if (!e.motions || motion.motion_number >= e.motions->size() ||
        !e.motions->at(motion.motion_number) ||
        !e.motions->at(motion.motion_number)->frames)
      throw std::runtime_error("Missing lunge recovery frame count");
    if (motion.frame / 65536 ==
        int64_t(e.motions->at(motion.motion_number)->frames) - 1) {
      modes = {1, 1, 1, 0};
      link();
      if (body_motion)
        body_modes = modes;
    }
    break;
  }
  default:
    break;
  }
  if (sound)
    s.pending_enemy_sounds.reserve(s.pending_enemy_sounds.size() + 1);
  auto &enemy = s.enemies[work];
  detail::commit(enemy, motion);
  enemy.modes = modes;
  enemy.zombie_flags = zflags;
  enemy.angles[1] = yaw;
  enemy.control0 = timer;
  enemy.zombie_work->action_anchor = anchor;
  if (body_motion) {
    detail::commit(s.enemies[*body], *body_motion);
    s.enemies[*body].zombie_flags = body_flags;
    s.enemies[*body].modes = body_modes;
  }
  player.flags = player_flags;
  player.modes = player_modes;
  s.random_seed = seed;
  if (sound)
    s.pending_enemy_sounds.push_back(*sound);
}
} // namespace ashford
