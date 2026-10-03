#include "ninja_math.hpp"
#include "scenario.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ashford {
namespace {
int32_t add(int32_t value, uint32_t amount) {
  uint32_t bits = uint32_t(value) + amount;
  return bits < 0x80000000u ? int32_t(bits)
                            : int32_t(int64_t(bits) - 0x100000000ll);
}
void rebuild(ScenarioEntity &entity) {
  if (entity.flags & 0x1080)
    throw std::runtime_error("Unsupported linked or retained prop matrix");
  if (!entity.model)
    throw std::runtime_error("Missing active prop model");
  if (entity.joint_pose.empty())
    entity.joint_pose = actor_bind_pose(*entity.model);
  entity.rendered_model = pose_actor_rotated(*entity.model, entity.joint_pose,
                                             entity.position, entity.angles);
}
void previous(ScenarioEntity &entity, uint32_t unsupported) {
  if (entity.flags & unsupported)
    throw std::runtime_error(
        "Unsupported linked, weapon, collision or fading prop");
  entity.previous_position = entity.position;
  entity.previous_angles = entity.angles;
}
} // namespace
void tick_scenario_props(ScenarioState &state) {
  if (!(state.flags[6][0] & 4))
    return;
  bool attachments_only = state.flags[10][0] & 0x40000000u;
  size_t count = attachments_only ? std::min(size_t(4), state.objects.size())
                                  : state.objects.size();
  for (size_t i = 0; i < count; ++i) {
    auto &entity = state.objects[i];
    if (!(entity.flags & 1) ||
        ((entity.state_flags & 0x1000000) && !state.player_sleep_ticks))
      continue;
    previous(entity, 0xe90080u);
    if (entity.id >= 1200) {
      if (entity.id < 1201 || entity.id > 1209)
        throw std::runtime_error("Unsupported special prop controller");
    } else if (entity.type == 2) {
      float increment = 182.04445f * (0.5f * entity.angular_speed);
      if (!std::isfinite(increment) || increment >= 2147483648.f ||
          increment < -2147483648.f)
        throw std::runtime_error(
            "Prop angular increment outside integer range");
      entity.angles[1] =
          add(entity.angles[1], uint32_t(int32_t(increment)) & 0xffff);
    } else if (entity.type == 3) {
      entity.angles[2] =
          int32_t(65536.f * ninja_sin_cos(uint16_t(entity.control0))[0]) / 32;
      entity.control0 = add(entity.control0, 1024);
    } else if (entity.type != 0 && !(entity.type >= 13 && entity.type < 100)) {
      throw std::runtime_error("Unsupported prop controller");
    }
    rebuild(entity);
  }
  if (attachments_only)
    return;
  for (auto &entity : state.items) {
    if (!(entity.flags & 1) || (entity.state_flags & 0x1000000))
      continue;
    previous(entity, 0x200000u);
    if (entity.angular_speed) {
      switch (entity.control0) {
      case 1:
        entity.control1 = add(entity.control1, 1);
        if (entity.control1 > entity.control2) {
          entity.model_flags |= 0x80;
          entity.highlight_yaw = 0;
          entity.control1 = 0;
          entity.control0 = 2;
        }
        break;
      case 2:
        entity.highlight_yaw = add(entity.highlight_yaw, 8192);
        entity.control1 = add(entity.control1, 1) & 7;
        if (!entity.control1) {
          entity.model_flags &= ~0x80u;
          entity.control0 = 3;
        }
        break;
      default:
        entity.model_flags &= ~0x80u;
        entity.control0 = 1;
        entity.control1 = 0;
        entity.control2 =
            int32_t(30.f * (float(scenario_random(state)) / 2.1474836E9f)) + 60;
        break;
      }
    }
    rebuild(entity);
  }
}
} // namespace ashford
