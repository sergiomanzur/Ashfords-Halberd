#include "camera.hpp"
#include "effects.hpp"
#include "light.hpp"
#include "ninja_matrix.hpp"
#include "scenario.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ashford {
namespace {
int32_t signed_bits(uint32_t v) {
  int32_t r;
  std::memcpy(&r, &v, 4);
  return r;
}
int32_t add(int32_t a, uint32_t b) { return signed_bits(uint32_t(a) + b); }
int32_t integer(float value) {
  if (!std::isfinite(value) || double(value) < -2147483648. ||
      double(value) >= 2147483648.)
    throw std::runtime_error("Light counter outside source integer range");
  return int32_t(value);
}
float random_unit(ScenarioState &s) {
  return -int32_t(scenario_random(s)) / -2147483600.f;
}
void modulate(LightWork &l, ScenarioState &s, std::array<float, 3> &rgb,
              Vec3 &vector) {
  const uint32_t step = uint32_t(l.angular_speed) << 8;
  auto phase = [&](bool randomized) {
    uint32_t advance = step;
    if (randomized)
      advance = uint32_t(integer(float(signed_bits(step)) * random_unit(s)));
    l.counters[0] = int32_t((uint32_t(l.counters[0]) + advance) & 0x7fff);
  };
  auto save = [&] { l.accumulators = rgb; };
  auto dim = [&] {
    for (size_t c = 0; c < 3; ++c)
      l.accumulators[c] += .5f * (.333f * rgb[c] - l.accumulators[c]);
  };
  switch (l.type) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6: {
    const int form = (l.type - 1) % 3;
    float sine = ninja_sin_cos(uint16_t(l.counters[0]))[0];
    if (form < 2) {
      const float factor = form == 0 ? .25f : .5f,
                  base = form == 0 ? .75f : .5f;
      const float f = sine * factor;
      for (size_t c = 0; c < 3; ++c)
        rgb[c] = f * l.color[c] + l.color[c] * base;
    } else
      for (size_t c = 0; c < 3; ++c)
        rgb[c] = sine * l.color[c];
    phase(l.type >= 4);
    break;
  }
  case 7:
    rgb.fill(std::abs(ninja_sin_cos(uint16_t(uint32_t(s.thunder) << 6))[0]));
    break;
  case 8:
  case 9: {
    uint32_t step_angle =
        uint32_t(integer(182.04445f * (.5f * float(l.angular_speed)))) & 0xffff;
    l.work_angles[3] =
        add(l.work_angles[3], l.type == 8 ? 0u - step_angle : step_angle);
    vector =
        light_direction({l.work_angles[2], l.work_angles[3], l.work_angles[4]});
    break;
  }
  case 10:
    switch (l.mode) {
    case 0:
      save();
      ++l.mode;
      break;
    case 1:
      save();
      if (double(random_unit(s)) <= double(float(l.angular_speed)) * .01) {
        l.counters[0] = integer(6.f * random_unit(s)) + 3;
        ++l.mode;
      }
      break;
    case 2:
    case 3:
      dim();
      l.counters[0] = add(l.counters[0], 0xffffffffu);
      if (l.counters[0] <= 0) {
        save();
        if (l.mode == 2) {
          l.counters[0] = integer(6.f * random_unit(s)) + 3;
          ++l.mode;
        } else
          l.mode = 1;
      }
      break;
    }
    rgb = l.accumulators;
    break;
  case 11:
    switch (l.mode) {
    case 0:
      save();
      ++l.mode;
      break;
    case 1:
      dim();
      if (double(random_unit(s)) <= double(float(l.angular_speed)) * .01) {
        save();
        l.counters[0] = integer(6.f * random_unit(s)) + 3;
        ++l.mode;
      }
      break;
    case 2:
      dim();
      l.counters[0] = add(l.counters[0], 0xffffffffu);
      if (l.counters[0] <= 0) {
        save();
        l.mode = 1;
      }
      break;
    }
    rgb = l.accumulators;
    break;
  case 12:
    if (l.mode == 0 || l.mode == 1) {
      const auto mode = l.mode;
      if (mode == 1)
        for (auto &c : rgb)
          c *= .5f;
      l.counters[0] = add(l.counters[0], 0xffffffffu);
      if (l.counters[0] <= 0) {
        l.counters[0] = integer(float(l.angular_speed) * random_unit(s)) + 1;
        l.mode = mode ? 0 : 1;
      }
    }
    break;
  case 13:
  case 101:
    for (size_t c = 0; c < 3; ++c)
      rgb[c] = ninja_sin_cos(uint16_t(l.counters[0]))[1] * l.color[c];
    l.counters[0] = add(l.counters[0], step);
    if (l.counters[0] > 16383)
      l.flags &= l.type == 13 ? ~1u : ~2u;
    break;
  case 100:
    if (l.mode == 0)
      ++l.mode;
    else if (l.mode == 1)
      l.flags &= ~2u;
    break;
  }
}
struct LinkedWork {
  uint32_t flags;
  const ModelMatrix *matrix;
  const ActorModelWork *work;
};
LinkedWork linked_entity(const ScenarioEntity &e,
                         const std::vector<ScenarioEntity> *pool = nullptr) {
  if (!(e.flags & 1))
    return {e.flags, nullptr, nullptr};
  const auto *work = e.model_work ? &*e.model_work : nullptr;
  if (!work && e.pose_owner) {
    if (!pool || *e.pose_owner >= pool->size())
      throw std::runtime_error("Missing linked light model owner");
    const auto &owner = pool->at(*e.pose_owner);
    work = owner.model_work ? &*owner.model_work : nullptr;
  }
  return {e.flags, e.model_matrix ? &*e.model_matrix : nullptr, work};
}
LinkedWork resolve_link(const LightWork &l, const ScenarioState &s,
                        const ScenarioEntity *player) {
  switch (l.link) {
  case 1:
    if (!player)
      throw std::runtime_error("Missing linked light player");
    return linked_entity(*player);
  case 2:
    if (l.link_index >= s.enemy_slots.size() ||
        s.enemy_slots[l.link_index] >= s.enemies.size())
      throw std::runtime_error("Linked light enemy outside authored slot map");
    return linked_entity(s.enemies[s.enemy_slots[l.link_index]], &s.enemies);
  case 3:
    if (l.link_index >= s.objects.size())
      throw std::runtime_error("Linked light object outside room");
    return linked_entity(s.objects[l.link_index], &s.objects);
  case 4:
    if (l.link_index >= s.items.size())
      throw std::runtime_error("Linked light item outside room");
    return linked_entity(s.items[l.link_index], &s.items);
  case 5: {
    if (!s.effect_pool || l.link_index >= s.effect_slots.size() ||
        s.effect_slots[l.link_index] >= s.effect_pool->works.size())
      throw std::runtime_error("Linked light effect outside authored slot map");
    const auto &e = s.effect_pool->works[s.effect_slots[l.link_index]];
    return {e.flags, e.model_matrix ? &*e.model_matrix : nullptr,
            e.model_work ? &*e.model_work : nullptr};
  }
  default:
    throw std::runtime_error("Invalid active light link");
  }
}
bool in_screen(const Camera &camera, Vec3 position, uint8_t margin) {
  const auto p = camera_point(camera, position);
  if (camera.perspective < 2 || camera.perspective >= 32768)
    throw std::runtime_error("Invalid light camera perspective");
  const float distance =
      320.f / std::tan(float(camera.perspective / 2) * .0000958738f);
  // Source njProjectScreen offsets2048 and NaViewScreenMatrix flips Y.
  const float q = distance / p.z;
  const float x = 2048.f + (p.x * 1.174f) * q, y = 2048.f - p.y * q;
  const float clip = 2.f * margin;
  return x >= 2048.f - 320.f - clip && x <= 2048.f + 320.f + clip &&
         y >= 2048.f - 240.f - clip && y <= 2048.f + 240.f + clip;
}
NativeLighting control(RoomLights &work, ScenarioState &s,
                       const ScenarioEntity *player, const Camera &camera,
                       const CameraDefinition &definition,
                       uint32_t camera_flags) {
  const bool events = camera_flags & 2;
  if (events)
    s.flags[10][0] |= 0x1000000;
  else
    s.flags[10][0] &= ~0x1000000u;
  if (s.flags[11][0] & 0x80)
    return drawing_lights(work, s.lighting.value_or(NativeLighting{}), true,
                          false);
  if (events && work.normal.size() < 4)
    throw std::runtime_error(
        "Event light table requires four reserved normal slots");
  NativeLighting result;
  const bool live = s.flags[12][0] & 0x80;
  const size_t count = events ? 4 + work.events.size() : work.normal.size();
  for (size_t i = 0; i < count; ++i) {
    auto &l = events && i >= 4 ? work.events[i - 4] : work.normal[i];
    if ((l.flags & 3) != 3)
      continue;
    Vec3 position = live ? l.position : l.work_position,
         vector = live ? l.vector : l.work_vector;
    auto rgb = live ? l.color : l.work_color;
    float near = live ? l.near_range : l.work_near,
          far = live ? l.far_range : l.work_far;
    if (live)
      modulate(l, s, rgb, vector);
    if (l.link > 5) {
      l.link = 0;
      l.link_index = 0;
    }
    if (l.link) {
      const auto linked = resolve_link(l, s, player);
      if (!(linked.flags & 1)) {
        l.flags &= ~3u;
        continue;
      }
      const ModelMatrix *matrix = linked.matrix;
      if (l.link_joint) {
        if (!linked.work || l.link_joint >= linked.work->joints.size())
          throw std::runtime_error("Linked light joint outside current cache");
        matrix = &linked.work->joints[l.link_joint].matrix;
      }
      if (!matrix)
        throw std::runtime_error("Missing linked light model matrix");
      l.position = detail::ninja_point(detail::ninja_load(*matrix), l.local);
      position = l.position;
    }
    if (l.source == 2 && !result.has_direction) {
      result.has_direction = true;
      result.direction = {vector, rgb};
    } else if (l.source == 4 && result.point_count < 3 &&
               (events || !(definition.flags & 0x10) ||
                in_screen(camera, position, definition.light_clip))) {
      result.points[result.point_count++] = {position, rgb, near, far};
    }
    if (live) {
      l.work_position = position;
      l.work_vector = vector;
      l.work_color = rgb;
      l.work_near = near;
      l.work_far = far;
    }
  }
  if (result.has_direction) {
    s.flags[10][0] |= 0x10000;
    // Single setters receive cam.mtx transformed direction, before view flip.
    Camera rotation = camera;
    rotation.position = {};
    rotation.distance = 0;
    const auto v = camera_point(rotation, result.direction.vector);
    result.single_vector = {-v.x, -v.y, v.z};
    result.single_color = result.direction.color;
    result.single_intensity = 1;
  } else
    s.flags[10][0] &= ~0x10000u;
  return result;
}
} // namespace
Vec3 light_direction(std::array<int32_t, 3> angles) {
  auto matrix = detail::ninja_identity();
  detail::ninja_rotate(matrix, angles);
  return detail::ninja_vector(matrix, {0, 0, -1});
}
NativeLighting control_lights(RoomLights &work, ScenarioState &s,
                              const ScenarioEntity *player,
                              const Camera &camera,
                              const CameraDefinition &definition,
                              uint32_t camera_flags) {
  auto candidate = work;
  const auto flags = s.flags[10][0], seed = s.random_seed;
  try {
    auto result =
        control(candidate, s, player, camera, definition, camera_flags);
    work = std::move(candidate);
    return result;
  } catch (...) {
    s.flags[10][0] = flags;
    s.random_seed = seed;
    throw;
  }
}
} // namespace ashford
