#include "actor_contacts.hpp"
#include "enemy_targeting.hpp"
#include "ninja_matrix.hpp"
#include "stateful_wall.hpp"
#include "zombie.hpp"
#include <stdexcept>
// Native adaptation of MIT recvx-decomp en01.c::CollCheck/CollCheckPush/
// CollCheckWall and zonzon.c::CalcPartsPos. No retained pose/asset copies.
namespace ashford {
namespace {
float finite(float v) { return detail::ninja_finite(v); }
Vec3 add(Vec3 a, Vec3 b) {
  return {finite(a.x + b.x), finite(a.y + b.y), finite(a.z + b.z)};
}
struct Meta {
  uint32_t zombie_flags = 0, model_flags = 0;
  std::array<uint8_t, 4> modes{};
  Vec3 velocity{};
  bool blocked = false;
};
ContactBody contact(const ScenarioEntity &e) {
  ContactBody b;
  b.position = e.position;
  b.offset = e.collision_offset;
  b.radius = e.geometry.collision_radius;
  b.height = e.geometry.collision_height;
  b.flags = e.flags;
  b.state_flags = e.state_flags;
  b.secondary_flags = e.secondary_flags;
  b.linked_parent = e.linked_parent;
  return b;
}
void scalar_work(ScenarioEntity &to, const ScenarioEntity &from) {
  to.flags = from.flags;
  to.state_flags = from.state_flags;
  to.secondary_flags = from.secondary_flags;
  to.position = from.position;
  to.previous_position = from.previous_position;
  to.angles = from.angles;
  to.modes = from.modes;
  to.floor = from.floor;
  to.geometry = from.geometry;
  to.collision_offset = from.collision_offset;
  to.push_count = from.push_count;
  to.push_index = from.push_index;
  to.step_area = from.step_area;
}
struct Frame {
  ScenarioState &state;
  ScenarioEntity &original_player;
  ScenarioEntity player, subject;
  ScenarioState wall_state;
  bool walls = false;
  std::vector<ContactBody> bodies;
  ContactBody player_contact;
  std::array<Meta, 128> meta{};
  size_t selected = 0;
  Frame(ScenarioState &s, ScenarioEntity &p) : state(s), original_player(p) {
    bodies.reserve(128);
    for (size_t i = 0; i < 128; ++i) {
      const auto &e = s.enemies[i];
      bodies.push_back(contact(e));
      meta[i] = {e.zombie_flags, e.model_flags, e.modes, e.damage_velocity,
                 e.zombie_work ? e.zombie_work->path_blocked : false};
    }
    scalar_work(player, p);
    player_contact = contact(p);
  }
  const RoomCollision &room() const {
    if (!state.collision)
      throw std::runtime_error("Missing zombie collision room");
    return walls ? *wall_state.collision : *state.collision;
  }
  const std::vector<CollisionRecord> &dynamic_walls() const {
    return walls ? wall_state.dynamic_walls : state.dynamic_walls;
  }
  void prepare_walls() {
    if (walls)
      return;
    room();
    wall_state.collision = state.collision;
    wall_state.dynamic_walls = state.dynamic_walls;
    wall_state.dynamic_miscellaneous = state.dynamic_miscellaneous;
    wall_state.dynamic_floors = state.dynamic_floors;
    wall_state.effect_pool = state.effect_pool;
    wall_state.flags = state.flags;
    walls = true;
  }
  void select(size_t i) {
    selected = i;
    scalar_work(subject, state.enemies[i]);
    subject.position = bodies[i].position;
    subject.collision_offset = bodies[i].offset;
    subject.state_flags = bodies[i].state_flags;
    subject.secondary_flags = bodies[i].secondary_flags;
  }
  void absorb() {
    auto &b = bodies[selected];
    b.position = subject.position;
    b.offset = subject.collision_offset;
    b.state_flags = subject.state_flags;
    b.secondary_flags = subject.secondary_flags;
    player_contact.position = player.position;
    player_contact.state_flags = player.state_flags;
    player_contact.flags = player.flags;
    player_contact.secondary_flags = player.secondary_flags;
  }
  void player_hit() {
    auto &e = bodies[selected];
    if (!(e.flags & 8) || (e.state_flags & 0x40000000) ||
        (e.secondary_flags & 1) || !(e.flags & 0x40))
      return;
    if (!(player_contact.flags & 8)) {
      if (!original_player.player_work)
        throw std::runtime_error("Missing player normal contact anchor");
      player_contact.normal_anchor = original_player.player_work->normal_anchor;
    }
    collide_enemy_player(e, player_contact);
    subject.position = e.position;
    subject.state_flags = e.state_flags;
    player.position = player_contact.position;
    player.state_flags = player_contact.state_flags;
  }
  void enemies_hit() {
    collide_actor_pool(bodies[selected], bodies, selected);
    subject.position = bodies[selected].position;
    subject.state_flags = bodies[selected].state_flags;
  }
  const ScenarioEntity &owner() const {
    const size_t index = state.enemies[selected].pose_owner.value_or(selected);
    if (index >= 128 || !(state.enemies[index].flags & 1) ||
        (index != selected && state.enemies[index].pose_owner))
      throw std::runtime_error("Invalid zombie collision pose owner");
    return state.enemies[index];
  }
  const ModelMatrix &cached_source(size_t i) const {
    const auto &o = owner();
    if (!o.model_work || i >= o.model_work->joints.size())
      throw std::runtime_error("Missing zombie collision cached joint");
    return o.model_work->joints[i].matrix;
  }
  detail::NinjaMatrix cached(size_t i) const {
    return detail::ninja_load(cached_source(i));
  }
  detail::NinjaMatrix world() const {
    auto m = detail::ninja_identity();
    detail::ninja_translate(m, subject.position);
    detail::ninja_rotate(m, subject.angles);
    return m;
  }
  detail::NinjaMatrix parts(size_t row, size_t count) const {
    static constexpr uint8_t tree[3][7] = {{0, 1, 8, 9, 15, 16, 17},
                                           {0, 1, 8, 9, 12, 13, 14},
                                           {0, 1, 8, 9, 10, 0, 0}};
    if (row >= 3 || count > 7)
      throw std::runtime_error("Invalid source parts path");
    auto m = world();
    const auto &pose = owner().joint_pose;
    for (size_t i = 0; i < count; ++i) {
      const auto joint = tree[row][i];
      if (joint >= pose.size() || !pose[joint].position)
        throw std::runtime_error("Missing zombie collision current joint");
      detail::ninja_translate(m, *pose[joint].position);
      detail::ninja_rotate(
          m, pose[joint].angles.value_or(std::array<int32_t, 3>{}));
    }
    return m;
  }
  Vec3 old(size_t joint) const {
    // These source branches read cached XZ only, then supply work's prior Y.
    const auto &m = cached_source(joint);
    auto scalar = [](double value) {
      if (!std::isfinite(value) ||
          std::abs(value) > std::numeric_limits<float>::max())
        throw std::runtime_error(
            "Collision cached XZ outside source float range");
      return float(value);
    };
    return {scalar(m[3]), subject.previous_position.y, scalar(m[11])};
  }
  void wall2(Vec3 n, Vec3 o, float radius, float height) {
    prepare_walls();
    const auto r =
        check_enemy_wall2(wall_state, subject, player, n, o, radius, height);
    subject.position = add(subject.position, r.displacement);
    absorb();
  }
  void primary_wall() {
    prepare_walls();
    CollisionBody body;
    body.position = subject.position;
    body.radius = subject.geometry.radius;
    body.floor = subject.floor;
    body.state_flags = subject.state_flags;
    subject.state_flags =
        check_step_state(room(), state.dynamic_miscellaneous, body);
    const auto n = add(subject.position, subject.collision_offset);
    const auto o = add(subject.previous_position, subject.collision_offset);
    const auto r = collide_stateful_walls_ex(wall_state, subject, player, n, o,
                                             subject.geometry.radius,
                                             subject.geometry.height);
    subject.position = add(subject.position, r.displacement);
    absorb();
  }
  void wall() {
    auto p = player.position;
    p.y = finite(.5f + p.y);
    meta[selected].blocked =
        bool(query_collision_line(room(), dynamic_walls(), subject.position, p)
                 .wall);
    const auto shape = meta[selected].zombie_flags & 15;
    const auto &e = state.enemies[selected];
    if (shape == 1 && (subject.flags & 0x10)) {
      if (e.type == 2 || e.type == 3 || e.type == 9) {
        const auto m = world(), prior = cached(0);
        auto n = detail::ninja_point(m, {0, 0, -2.5f});
        auto o = detail::ninja_point(prior, {0, 0, -2.5f});
        auto n2 = detail::ninja_point(m, {0, 0, 2.4f});
        auto o2 = detail::ninja_point(prior, {0, 0, 2.4f});
        o.y = o2.y = subject.previous_position.y;
        wall2(n, o, subject.geometry.radius, subject.geometry.height);
        if (!(meta[selected].zombie_flags & 0x20000))
          wall2(n2, o2, subject.geometry.radius, subject.geometry.height);
      } else {
        primary_wall();
        if (!(meta[selected].zombie_flags & 0x8000000)) {
          auto o = old(10);
          auto n = detail::ninja_point(parts(2, 5), {});
          n.y = subject.position.y;
          wall2(n, o, 2, 2);
        }
      }
    } else if (shape == 2) {
      primary_wall();
      if (e.zombie_work->damage_80 != -1 && e.zombie_work->damage_84 != -1) {
        auto o = old(14), other = old(17);
        o.x = finite((o.x + other.x) / 2.f);
        o.z = finite((o.z + other.z) / 2.f);
        auto n = detail::ninja_point(parts(0, 7), {});
        auto n2 = detail::ninja_point(parts(1, 7), {});
        n.x = finite((n.x + n2.x) / 2.f);
        n.z = finite((n.z + n2.z) / 2.f);
        n.y = subject.position.y;
        wall2(n, o, 3, 2);
      }
    } else if (shape == 3) {
      auto o = old(1);
      auto n = detail::ninja_point(parts(2, 2), {});
      n.y = subject.position.y;
      wall2(n, o, 1.5f, 2);
    } else if (shape == 4) {
      auto n = detail::ninja_point(world(), {0, 0, -3});
      auto o = detail::ninja_point(cached(8), {0, 0, -3});
      wall2(n, o, subject.geometry.radius, subject.geometry.height);
    } else if (shape == 5) {
      auto o = old(0);
      auto n = detail::ninja_point(parts(2, 1), {});
      n.y = subject.position.y;
      wall2(n, o, subject.geometry.radius, subject.geometry.height);
    } else if (shape == 6) {
      auto m = parts(2, 2), prior = cached(1);
      auto n = detail::ninja_point(m, {0, -2.5f, 0});
      auto o = detail::ninja_point(prior, {0, -2.5f, 0});
      auto n2 = detail::ninja_point(m, {0, 2.4f, 0});
      auto o2 = detail::ninja_point(prior, {0, 2.4f, 0});
      n.y = n2.y = subject.position.y;
      o.y = o2.y = subject.previous_position.y;
      wall2(n, o, subject.geometry.radius, subject.geometry.height);
      wall2(n2, o2, subject.geometry.radius, subject.geometry.height);
    }
  }
  void push() {
    for (auto &b : bodies)
      if ((b.flags & 1) && !(b.flags & 0x80))
        b.state_flags &= ~2u;
    subject.state_flags = bodies[selected].state_flags;
    player_hit();
    const auto saved = subject.position;
    enemies_hit();
    bool whit = false;
    for (size_t i = 0; i < 128; ++i) {
      const auto &e = state.enemies[i];
      auto &m = meta[i];
      const auto &b = bodies[i];
      if ((e.id != 1 && e.id != 26) || !(e.flags & 1) || (e.flags & 0x82) ||
          i == selected || e.type == 2 || e.type == 3 || e.type == 6 ||
          e.type == 7 || e.type == 9 || e.type == 10 || m.modes[0] != 1 ||
          !e.zombie_work || (m.zombie_flags & 0x440200))
        continue;
      if (b.state_flags & 2) {
        m.zombie_flags |= 0x200;
        const bool front = ((uint32_t(subject.angles[1]) + 32768u -
                             uint32_t(e.angles[1]) + 16384u) &
                            65535u) >= 32768;
        if (front)
          m.zombie_flags |= 0x2000;
        else
          m.zombie_flags &= ~0x2000u;
        const auto angle =
            uint16_t(uint32_t(e.angles[1]) +
                     uint16_t(enemy_direction(b.position, subject.position)));
        const auto r1 = uint16_t(uint32_t(e.angles[1]) - 5461u);
        const auto r2 = uint16_t(uint32_t(e.angles[1]) + 5461u);
        if (r2 >= angle || angle >= r1) {
          const auto sc = ninja_sin_cos(uint16_t(e.angles[1]));
          m.velocity = {-sc[0], 0, -sc[1]};
          m.modes[2] = e.id == 26 ? 2 : front ? 4 : 3;
        } else
          m.modes[2] = 2;
        m.modes[0] = 3;
        m.modes[1] = m.modes[3] = 0;
        // Original source writes epw->exp1 mode0, not the pushed target's body.
        if (state.enemies[selected].zombie_work->body) {
          const size_t body = *state.enemies[selected].zombie_work->body;
          if (body >= 128 || body == selected ||
              !(state.enemies[body].flags & 1))
            throw std::runtime_error("Invalid pusher body mode target");
          meta[body].modes[0] = 3;
        }
      }
      if (b.state_flags & 1)
        whit = true;
    }
    if (!whit)
      subject.position = saved;
    absorb();
  }
  void surface(size_t slot) {
    auto &b = bodies[slot];
    if (state.flags[5][0] & 0x100) {
      meta[slot].model_flags &= ~0x40u;
      const auto &r = room();
      auto inspect = [&](const std::vector<CollisionRecord> &records) {
        for (const auto &f : records) {
          if (!(f.flags & 1) || f.type != 3 ||
              f.floor != state.enemies[slot].floor)
            continue;
          const auto x = finite(b.position.x), z = finite(b.position.z);
          const auto right = finite(finite(f.position.x) + finite(f.size.x));
          const auto back = finite(finite(f.position.z) + finite(f.size.z));
          if (x >= f.position.x && x <= right && z >= f.position.z &&
              z <= back) {
            if (f.parameters[0] == 2)
              meta[slot].model_flags |= 0x40;
            return true;
          }
        }
        return false;
      };
      if (!inspect(r.floors))
        inspect(state.dynamic_floors);
    }
  }
  void commit() {
    for (size_t i = 0; i < 128; ++i) {
      auto &e = state.enemies[i];
      const auto &b = bodies[i];
      const auto &m = meta[i];
      e.position = b.position;
      e.collision_offset = b.offset;
      e.state_flags = b.state_flags;
      e.secondary_flags = b.secondary_flags;
      e.zombie_flags = m.zombie_flags;
      e.model_flags = m.model_flags;
      e.modes = m.modes;
      e.damage_velocity = m.velocity;
      if (e.zombie_work)
        e.zombie_work->path_blocked = m.blocked;
    }
    auto &e = state.enemies[selected];
    e.push_count = subject.push_count;
    e.push_index = subject.push_index;
    e.step_area = subject.step_area;
    original_player.position = player.position;
    original_player.state_flags = player.state_flags;
    original_player.flags = player.flags;
    original_player.modes = player.modes;
    if (walls) {
      state.collision = std::move(wall_state.collision);
      state.dynamic_walls = std::move(wall_state.dynamic_walls);
      state.effect_pool = std::move(wall_state.effect_pool);
      state.flags[5][0] = wall_state.flags[5][0];
    }
  }
};
} // namespace
void tick_zombie_collision(ScenarioState &s, size_t slot, ScenarioEntity &p) {
  if (s.enemies.size() != 128 || slot >= 128 || !(s.enemies[slot].flags & 1) ||
      s.enemies[slot].id != 1 || !s.enemies[slot].zombie_work)
    throw std::runtime_error("Invalid zombie collision work");
  for (const auto &e : s.enemies)
    if (&e == &p)
      throw std::runtime_error("Collision player aliases enemy work");
  Frame f(s, p);
  const auto &e = s.enemies[slot];
  size_t selected = slot;
  if (e.zombie_flags & 0x80000000) {
    if (e.flags & 0x80) {
      if (!e.linked_parent || *e.linked_parent >= 128 ||
          *e.linked_parent == slot ||
          !(s.enemies[*e.linked_parent].flags & 1) ||
          !s.enemies[*e.linked_parent].zombie_work)
        throw std::runtime_error("Missing zombie collision parent");
      selected = *e.linked_parent;
    }
    f.select(selected);
    if (!(f.subject.flags & 2)) {
      if (f.subject.flags & 8) {
        if (f.meta[selected].zombie_flags & 0x100)
          f.push();
        else {
          if (e.flags & 0x80) {
            const auto base = f.old(0);
            f.subject.collision_offset = {
                finite(base.x - f.subject.position.x), 0,
                finite(base.z - f.subject.position.z)};
          } else
            f.subject.collision_offset = {};
          f.absorb();
          if (!(f.meta[selected].zombie_flags & 0x40)) {
            const bool force = f.meta[selected].zombie_flags & 0x40000;
            if (force) {
              f.subject.secondary_flags &= ~1u;
              f.absorb();
            }
            f.player_hit();
            if (force) {
              f.subject.secondary_flags |= 1;
              f.absorb();
            }
            f.enemies_hit();
          }
        }
      }
      f.wall();
    }
  } else {
    f.select(slot);
    if (!(e.flags & 2) && (e.zombie_flags & 0x40000000))
      f.wall();
    else
      f.surface(slot);
  }
  // Source water check acts on incoming work, even when walls used its parent.
  if ((s.flags[5][0] & 0x40) && !(f.bodies[slot].state_flags & 0x100000) &&
      query_water(f.room(), s.dynamic_floors, f.bodies[slot].position))
    f.bodies[slot].state_flags |= 0x100000;
  f.commit();
}
} // namespace ashford
