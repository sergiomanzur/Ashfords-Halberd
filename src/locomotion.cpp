// Adapted from MIT AshfordFamily/recvx-decomp player.c::bhControlPlayerPad,
// bhCPM1_act_bas, bhCPM2_act_std/srt/sta/wlk/run/bak and pwksub::bhGetFrameNum.
#include "locomotion.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ashford {
std::vector<JointPose>
sample_player_locomotion_pose(const PlayerPackage &package,
                              const PlayerLocomotion &state,
                              const std::vector<JointPose> &previous,
                              std::array<uint16_t, 3> *angle_scratch) {
  if (package.models.empty() || state.motion >= package.motions.size() ||
      !package.motions[state.motion])
    throw std::runtime_error("Missing player pose assets");
  const auto &motion = *package.motions[state.motion];
  if (state.sampled_frame < 0 ||
      int64_t(state.sampled_frame) >= int64_t(motion.frames) * 65536)
    throw std::runtime_error("Player sampled frame outside motion");
  auto bind = actor_bind_pose(package.models[0]);
  if (!previous.empty() && previous.size() != bind.size())
    throw std::runtime_error("Player pose outside skeleton");
  auto current = previous.empty() ? std::move(bind) : previous;
  MotionApplicationOptions options;
  options.joint_flags.resize(current.size());
  for (size_t joint : {7u, 11u})
    if (joint < options.joint_flags.size())
      options.joint_flags[joint] = 8;
  if (state.sampled_blend_count)
    options.blend_rate = 0.000015258789f * (65536u - state.blend_rate);
  auto applied = apply_motion_pose(
      current, motion, state.sampled_frame, options,
      angle_scratch ? *angle_scratch : std::array<uint16_t, 3>{});
  if (angle_scratch)
    *angle_scratch = applied.angle_scratch;
  return std::move(applied.pose);
}

int32_t remap_player_frame(uint32_t old_count, uint32_t new_count,
                           int32_t frame) {
  if (!old_count || !new_count || old_count > 32767 || new_count > 32767 ||
      frame < 0 || int64_t(frame) >= int64_t(old_count) * 65536)
    throw std::runtime_error("Invalid player motion phase remap");
  float phase =
      float(new_count) * ((100.f / float(old_count)) * float(frame / 65536));
  int32_t result = int32_t(phase / 100.f) * 65536;
  if (result < 0)
    result = 0;
  if (uint32_t(result) > new_count)
    result = int32_t(new_count);
  return result;
}
PlayerLocomotion
advance_player_locomotion(PlayerLocomotion s, const PlayerPackage &package,
                          const PlayerTranslationTables &tables, uint16_t pad) {
  auto motion = [&](size_t index) -> const Motion & {
    if (index >= package.motions.size() || !package.motions[index] ||
        !package.motions[index]->frames ||
        package.motions[index]->frames > 32767)
      throw std::runtime_error("Missing or invalid baseline player motion");
    return *package.motions[index];
  };
  if (!std::isfinite(s.position.x) || !std::isfinite(s.position.y) ||
      !std::isfinite(s.position.z) || !std::isfinite(s.visual_anchor.x) ||
      !std::isfinite(s.visual_anchor.z) || s.frame < 0)
    throw std::runtime_error("Invalid baseline player state");
  if (!s.locked) {
    uint8_t next = 0;
    switch (pad & 15) {
    case 8:
      next = 1;
      break;
    case 4:
      next = 2;
      break;
    case 1:
      next = 3;
      break;
    case 9:
      next = 4;
      break;
    case 5:
      next = 5;
      break;
    case 2:
      next = 9;
      break;
    case 10:
      next = 10;
      break;
    case 6:
      next = 11;
      break;
    }
    if (next >= 3 && next <= 5) {
      if (!(s.previous_pad & 1))
        s.entering = true;
    } else if (next >= 9 && next <= 11) {
      if (!(s.previous_pad & 2))
        s.entering = true;
    } else if (next != s.mode)
      s.entering = true;
    s.mode = next;
  }
  if (s.mode != 0 && s.mode != 1 && s.mode != 2 && s.mode != 3 && s.mode != 4 &&
      s.mode != 5 && s.mode != 6 && s.mode != 9 && s.mode != 10 &&
      s.mode != 11 && s.mode != 13)
    throw std::runtime_error("Unsupported baseline locomotion state");
  if (s.frame >= int64_t(motion(s.motion).frames) * 65536)
    throw std::runtime_error("Baseline player frame outside motion");
  int32_t add = 65536;
  if (s.mode == 1 || s.mode == 2) {
    s.yaw = uint16_t(s.yaw + (s.mode == 1 ? -1 : 1) * int(182.04445f * 7.2f));
    add = s.mode == 1 ? -65536 : 65536;
  } else if (s.mode == 4 || s.mode == 5 || s.mode == 10 || s.mode == 11)
    s.yaw = uint16_t(s.yaw + ((s.mode == 4 || s.mode == 10) ? -1 : 1) *
                                 int(182.04445f * 4.f));
  auto enter = [&](size_t index, uint8_t count, uint16_t rate, int32_t frame) {
    motion(index);
    s.motion = index;
    s.blend_count = count;
    s.blend_rate = rate;
    s.frame = frame;
    s.entering = false;
    s.phase = 1;
  };
  auto speed = [&](size_t table) {
    return player_translation_speed(tables, table, size_t(s.frame / 65536)) *
           (s.water ? .8f : 1.f);
  };
  if (s.mode == 0) {
    if (s.entering) {
      enter(42, 5, 42598, 0);
      s.idle_ticks = 0;
      s.moving = false;
      s.stop_delta = {.3f * (s.visual_anchor.x - s.position.x), 0,
                      .3f * (s.visual_anchor.z - s.position.z)};
    }
    if (s.blend_count > 2) {
      s.position.x += s.stop_delta.x;
      s.position.z += s.stop_delta.z;
    }
    if (++s.idle_ticks > 300) {
      s.mode = 13;
      s.entering = true;
      s.locked = true;
    }
  } else if (s.mode == 1 || s.mode == 2) {
    if (s.entering) {
      enter(39, 8, 42598, 0);
      s.moving = false;
    }
  } else if (s.mode >= 3 && s.mode <= 5) {
    if (s.entering) {
      int32_t frame = s.preserve_walk_phase
                          ? remap_player_frame(motion(s.motion).frames,
                                               motion(0).frames, s.frame)
                          : 327680;
      s.preserve_walk_phase = false;
      enter(0, 8, 42598, frame);
    }
    s.moving = true;
    s.position = player_add_speed(s.position, s.yaw, speed(0));
    if (pad & 0x400) {
      s.mode = 6;
      s.entering = true;
      s.locked = true;
    }
  } else if (s.mode == 6) {
    if (s.entering)
      enter(4, 4, 42598,
            remap_player_frame(motion(s.motion).frames, motion(4).frames,
                               s.frame));
    s.moving = true;
    if (!(pad & 1)) {
      s.locked = false;
      s.mode = 0;
      s.entering = true;
    } else if (!(pad & 0x400)) {
      s.locked = false;
      s.preserve_walk_phase = true;
      s.mode = 3;
      s.entering = true;
    } else {
      if (pad & 8)
        s.yaw = uint16_t(s.yaw - int(182.04445f * 4.5f));
      if (pad & 4)
        s.yaw = uint16_t(s.yaw + int(182.04445f * 4.5f));
      s.position = player_add_speed(s.position, s.yaw, speed(3));
    }
  } else if (s.mode >= 9 && s.mode <= 11) {
    if ((pad & 0x400) && !(s.previous_pad & 0x400))
      throw std::runtime_error(
          "Quick-turn state requires the full player controller");
    if (s.entering)
      enter(16, 8, 49152,
            s.moving ? remap_player_frame(motion(s.motion).frames,
                                          motion(16).frames, s.frame)
                     : 1638400);
    s.position = player_add_speed(s.position, s.yaw, -speed(6), 32768);
  } else if (s.mode == 13) {
    if (s.entering)
      enter(9, 8, 49152, 0);
    if (s.phase == 1 &&
        uint32_t(s.frame / 65536) >= motion(s.motion).frames - 1) {
      enter(10, 4, 49152, 0);
      s.phase = 2;
    }
    if (pad & 0x1f) {
      s.locked = false;
      pad &= uint16_t(~15);
    }
  }
  const auto &selected = motion(s.motion);
  int64_t limit = int64_t(selected.frames) * 65536;
  if (s.frame < 0 || s.frame >= limit)
    throw std::runtime_error("Locomotion entry frame outside motion");
  s.sampled_frame = s.frame;
  int64_t next = int64_t(s.frame) + add;
  if (next < 0)
    next += limit;
  else if (next >= limit)
    next -= limit;
  s.frame = int32_t(next);
  s.sampled_blend_count = s.blend_count;
  if (s.blend_count)
    --s.blend_count;
  s.previous_pad = pad;
  if (!std::isfinite(s.position.x) || !std::isfinite(s.position.z))
    throw std::runtime_error("Locomotion position overflow");
  return s;
}
PlayerWorldState advance_player_world(PlayerWorldState s,
                                      const PlayerPackage &package,
                                      const PlayerTranslationTables &tables,
                                      const RoomCollision &room,
                                      const std::vector<RoomCut> &cuts,
                                      uint16_t pad, ActorContacts *contacts) {
  Vec3 previous = s.player.position;
  auto next = advance_player_locomotion(s.player, package, tables, pad);
  float target = next.motion == 0    ? 2.f
                 : next.motion == 4  ? 3.f
                 : next.motion == 16 ? -1.f
                                     : 0.f;
  s.front_offset += .5f * (target - s.front_offset);
  Vec3 front = player_add_speed(next.position, next.yaw, s.front_offset);
  front.y = next.position.y + 12.5f;
  std::optional<ActorContacts> staged_contacts;
  if (contacts) {
    staged_contacts = *contacts;
    staged_contacts->player.position = next.position;
    if (staged_contacts->player.flags & 8)
      collide_actor_pool(staged_contacts->player, staged_contacts->enemies);
    next.position = staged_contacts->player.position;
  }
  CollisionBody upper;
  upper.position = front;
  upper.floor = s.floor;
  upper.height = 1;
  auto upper_result = collide_player_walls_ex(room, upper, s.front_anchor);
  next.position.x += upper_result.position.x - front.x;
  next.position.z += upper_result.position.z - front.z;
  CollisionBody lower;
  lower.position = next.position;
  lower.floor = s.floor;
  lower.flags = 0x100;
  auto lower_result = collide_player_walls_ex(room, lower, previous);
  next.position = lower_result.position;
  s.floor = lower_result.floor;
  s.front_anchor = upper_result.position;
  next.visual_anchor = s.front_anchor;
  next.water = player_in_water(room, next.position);
  s.cut =
      select_camera_cut(cuts, s.cut, next.position.x, next.position.z, s.floor);
  s.wall_hits = upper_result.hits + lower_result.hits;
  s.player = next;
  if (staged_contacts) {
    staged_contacts->player.position = next.position;
    *contacts = std::move(*staged_contacts);
  }
  return s;
}
} // namespace ashford
