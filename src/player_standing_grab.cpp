#include "enemy_motion_internal.hpp"
#include "ninja_matrix.hpp"
#include "zombie.hpp"
#include <limits>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::PlyDG01. Source bhSetMotion
// is called immediately on release; model calculation remains the caller's.
namespace ashford {
namespace {
int32_t word(uint32_t bits) {
  return bits < 0x80000000u ? int32_t(bits)
                            : int32_t(int64_t(bits) - 0x100000000ll);
}
float cached_xz(double v) {
  if (!std::isfinite(v) || std::fabs(v) > std::numeric_limits<float>::max())
    throw std::runtime_error("Invalid cached standing release position");
  return float(v);
}
constexpr uint32_t offsets[4] = {400, 425, 450, 425};
} // namespace
void tick_zombie_player_standing_grab(ScenarioState &s, size_t work,
                                      ScenarioEntity &p) {
  if (s.enemies.size() != 128 || work >= s.enemies.size() ||
      !(s.enemies[work].flags & 1) || s.enemies[work].id != 1 ||
      !s.enemies[work].zombie_work)
    throw std::runtime_error("Invalid standing player grab enemy work");
  for (const auto &entity : s.enemies)
    if (&entity == &p)
      throw std::runtime_error("Player aliases standing-grab enemy pool");
  const auto &e = s.enemies[work];
  auto m = detail::snapshot(p);
  auto bank = p.motions;
  auto modes = p.modes;
  auto zflags = e.zombie_flags;
  auto anchor = e.zombie_work->action_anchor;
  auto yaw_offset = e.grab_yaw_offset;
  auto angles = p.angles;
  auto position = p.position;
  auto yaw = p.yaw_correction, timer = p.control0;
  auto state_flags = p.state_flags, attack_flags = p.attack_flags,
       input = s.input_held;
  auto direction = p.frame_direction;
  auto attribute = p.motion_attribute;
  auto boundary = p.motion_boundary;
  auto scratch = s.motion_angle_scratch;
  std::optional<Vec3> movement_anchor;
  std::optional<int32_t> voice;
  std::optional<MotionApplicationResult> sampled;
  const bool back = zflags & 0x4000;
  auto character = [&] {
    if (s.player_id >= 4)
      throw std::runtime_error("Invalid standing-grab character");
  };
  auto clip = [&](uint32_t relative) {
    character();
    m.motion_number = offsets[s.player_id] + relative;
    if (!bank || m.motion_number >= bank->size() ||
        !bank->at(m.motion_number) || !bank->at(m.motion_number)->frames)
      throw std::runtime_error("Missing standing player grab clip");
  };
  auto start = [&] {
    bank = e.motions;
    m.frame = 0;
    m.blend_count = 5;
    m.blend_rate = 42598;
    m.motion_step = 65536;
    m.motion_mode = 0;
    clip(back ? 1 : 2);
    anchor = {0, 0, back ? -8.f : -8.8444757f};
    yaw_offset = back ? -32768 : 0;
    zflags |= 0x80000;
  };
  auto call_voice = [&] {
    if (!(e.flags & 0x10000))
      voice = 1026;
  };
  auto facing = [&](bool releasing) {
    character();
    const auto base =
        uint16_t(uint32_t(e.angles[1]) + uint32_t(int32_t(yaw_offset)));
    if (releasing)
      yaw =
          uint16_t(uint32_t(base) + (s.player_id == 0 && !back ? 32768u : 0u));
    else
      yaw = word(uint32_t(base) + (s.player_id == 0 && timer != 0
                                       ? uint32_t(m.frame / 65536) * 546u
                                       : 0u));
  };
  switch (modes[3]) {
  case 0:
    m.flags |= 0x1000;
    m.flags &= ~0x40000u;
    timer = 0;
    ++modes[3];
    [[fallthrough]];
  case 1: {
    const uint32_t frame = uint32_t(e.frame / 65536);
    if (frame == 11) {
      start();
      if (!back)
        timer = word(uint32_t(timer) + 1u);
    } else if (frame == 19)
      call_voice();
    facing(false);
    break;
  }
  case 2:
    m.frame = 0;
    m.blend_count = 0;
    m.blend_rate = 32768;
    m.motion_step = 65536;
    clip(back ? 4 : 5);
    facing(true);
    ++modes[3];
    break;
  case 3:
    facing(true);
    if (m.frame / 65536 == 0) {
      if (!p.model_work || p.model_work->joints.size() != p.joint_pose.size() ||
          p.model_work->first_joint >= p.model_work->joints.size() ||
          (p.model_work->joint_count &&
           (!*p.model_work->joint_count ||
            *p.model_work->joint_count >
                p.model_work->joints.size() - p.model_work->first_joint)))
        throw std::runtime_error("Missing standing release root cache");
      const auto &matrix =
          p.model_work->joints[p.model_work->first_joint].matrix;
      position.x = cached_xz(matrix[3]);
      position.z = cached_xz(matrix[11]);
      if (!p.player_work)
        throw std::runtime_error("Missing player movement EXP0");
      movement_anchor = p.player_work->movement_anchor;
      movement_anchor->x = position.x;
      movement_anchor->z = position.z;
      if (s.player_id == 0 && !back)
        angles[1] = uint16_t(uint32_t(angles[1]) + 32768u);
      input &= ~0xfu;
      m.flags &= ~0x11000u;
      m.flags |= 8;
      state_flags &= ~0x10000u;
      attack_flags = 0;
      if (!p.base_motions || p.base_motions->size() <= 42 ||
          !p.base_motions->at(42) || !p.base_motions->at(42)->frames)
        throw std::runtime_error("Missing retained standing motion42");
      bank = p.base_motions;
      modes = {1, 0, 0, 0};
      zflags &= ~0xa4080u;
      m.frame = 0;
      m.motion_number = 42;
      m.blend_rate = 49152;
      m.blend_count = 8;
      m.motion_step = 0;
      m.motion_mode |= 0x100;
      const auto &motion = bank->at(42).value();
      MotionApplicationOptions options;
      options.mode = m.motion_mode;
      options.joint_base = p.model_work->first_joint;
      for (const auto &joint : p.model_work->joints)
        options.joint_flags.push_back(uint8_t(joint.flags));
      options.blend_rate = .000015258789f * float(65536 - m.blend_rate);
      sampled =
          apply_motion_pose(p.joint_pose, motion, m.frame, options, scratch);
      scratch = sampled->angle_scratch;
      attribute = motion_attribute_at(motion, m.frame, attribute);
      boundary = advance_motion_frame(motion.frames, 0, m.motion_mode, m.frame,
                                      direction);
      m.flags &= ~4u;
    }
    break;
  case 4:
    m.flags |= 0x1000;
    m.flags &= ~0x40000u;
    timer = 0;
    start();
    angles[1] = back ? uint16_t(uint32_t(e.angles[1]) + 32768u) : e.angles[1];
    ++modes[3];
    [[fallthrough]];
  case 5:
    if (e.frame / 65536 == 19)
      call_voice();
    facing(false);
    break;
  default:
    break;
  }
  if (voice)
    s.pending_player_voices.reserve(s.pending_player_voices.size() + 1);
  auto &enemy = s.enemies[work];
  enemy.zombie_flags = zflags;
  enemy.zombie_work->action_anchor = anchor;
  enemy.grab_yaw_offset = yaw_offset;
  detail::commit(p, m);
  p.motions = bank;
  p.modes = modes;
  p.angles = angles;
  p.position = position;
  p.yaw_correction = yaw;
  p.control0 = timer;
  p.state_flags = state_flags;
  p.attack_flags = attack_flags;
  p.frame_direction = direction;
  p.motion_attribute = attribute;
  p.motion_boundary = boundary;
  if (movement_anchor)
    p.player_work->movement_anchor = *movement_anchor;
  if (sampled)
    p.joint_pose = std::move(sampled->pose);
  s.input_held = input;
  s.motion_angle_scratch = scratch;
  if (voice)
    s.pending_player_voices.push_back(*voice);
}
} // namespace ashford
