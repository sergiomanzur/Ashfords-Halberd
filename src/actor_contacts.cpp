#include "actor_contacts.hpp"
#include "ninja_math.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp hitchk.c::bhCheckPlayer/bhCheckEnemies.
namespace ashford {
namespace {
void finite(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Nonfinite actor contact value");
}
void vector(Vec3 v) {
  finite(v.x);
  finite(v.y);
  finite(v.z);
}
void validate(const ContactBody &body) {
  vector(body.position);
  vector(body.offset);
  finite(body.radius);
  finite(body.height);
  if (body.radius < 0 || body.height < 0)
    throw std::runtime_error("Negative actor contact dimensions");
}
Vec3 center(const ContactBody &body) {
  Vec3 v{body.position.x + body.offset.x, body.position.y + body.offset.y,
         body.position.z + body.offset.z};
  vector(v);
  return v;
}
float distance(float x, float z) {
  const float squared = x * x + z * z;
  finite(squared);
  return std::sqrt(squared);
}
bool overlaps(Vec3 first, const ContactBody &a, Vec3 second,
              const ContactBody &b, float length) {
  const float sum = a.radius + b.radius;
  const float top_a = first.y + a.height, top_b = second.y + b.height;
  finite(sum);
  finite(top_a);
  finite(top_b);
  return length < sum && second.y <= top_a && top_b >= first.y;
}
Vec3 outside(Vec3 first, const ContactBody &a, Vec3 second,
             const ContactBody &b) {
  const auto angle =
      int32_t(10430.381f * std::atan2(second.x - first.x, second.z - first.z));
  const auto trig = ninja_sin_cos(uint16_t(angle));
  const float sum = a.radius + b.radius;
  Vec3 position = a.position;
  position.x = (second.x - sum * trig[0]) - a.offset.x;
  position.z = (second.z - sum * trig[1]) - a.offset.z;
  vector(position);
  return position;
}
} // namespace
bool collide_enemy_player(ContactBody &enemy, ContactBody &player) {
  if (!(enemy.flags & 8) || enemy.state_flags & 0x40000000 ||
      enemy.secondary_flags & 1 || !(enemy.flags & 0x40))
    return false;
  if (&enemy == &player)
    throw std::runtime_error("Aliased player/enemy contact");
  validate(enemy);
  validate(player);
  const auto e = center(enemy);
  auto p = center(player);
  if (!(player.flags & 8)) {
    vector(player.normal_anchor);
    p = {player.normal_anchor.x + player.offset.x,
         player.normal_anchor.y + player.offset.y,
         player.normal_anchor.z + player.offset.z};
    vector(p);
  }
  const float x = p.x - e.x, z = p.z - e.z, length = distance(x, z);
  if (!overlaps(e, enemy, p, player, length))
    return false;
  auto moved_enemy = enemy, moved_player = player;
  if (player.secondary_flags & 1 || enemy.secondary_flags & 0x40)
    moved_enemy.position = outside(e, enemy, p, player);
  else {
    const float margin = .5f * (player.radius + enemy.radius - length);
    // VU reciprocal square root of zero saturates; zero components stay zero.
    const float inverse = length ? 1.f / length : 0;
    const float dx = (x * inverse) * margin, dz = (z * inverse) * margin;
    moved_player.position.x += dx;
    moved_player.position.z += dz;
    moved_enemy.position.x -= dx;
    moved_enemy.position.z -= dz;
    vector(moved_player.position);
    vector(moved_enemy.position);
  }
  moved_enemy.state_flags |= 4;
  moved_player.state_flags |= 2;
  enemy = std::move(moved_enemy);
  player = std::move(moved_player);
  return true;
}
size_t collide_actor_pool(ContactBody &subject, std::vector<ContactBody> &pool,
                          std::optional<size_t> subject_slot) {
  if (pool.size() > 128 || (subject_slot && (*subject_slot >= pool.size() ||
                                             &subject != &pool[*subject_slot])))
    throw std::runtime_error("Invalid actor contact work pool");
  if (!subject_slot)
    for (const auto &body : pool)
      if (&body == &subject)
        throw std::runtime_error("Player contact aliases enemy pool");
  if (subject.secondary_flags & 1)
    return 0;
  validate(subject);
  const auto origin = center(subject);
  auto updated = pool;
  std::array<Vec3, 128> positions;
  size_t count = 0;
  for (size_t i = 0; i < pool.size(); ++i) {
    const auto &body = pool[i];
    if (!(body.flags & 1) || !(body.flags & 8) ||
        body.state_flags & 0x1000000 || (subject_slot && i == *subject_slot) ||
        (!subject_slot && !(body.flags & 0x40)))
      continue;
    if (body.flags & 0x80) {
      if (!body.linked_parent || *body.linked_parent >= pool.size())
        throw std::runtime_error("Missing linked contact parent");
      if (pool[*body.linked_parent].state_flags & 0x1000000)
        continue;
    }
    validate(body);
    const auto other = center(body);
    const auto length = distance(origin.x - other.x, origin.z - other.z);
    if (!overlaps(origin, subject, other, body, length))
      continue;
    positions[count++] = outside(origin, subject, other, body);
    updated[i].state_flags |= subject_slot ? 2 : 4;
  }
  if (!count)
    return 0;
  auto result = subject;
  if (count == 1)
    result.position = positions[0];
  else {
    float x = 0, z = 0;
    for (size_t i = 0; i < count; ++i) {
      x += positions[i].x;
      z += positions[i].z;
    }
    result.position.x = x / count;
    result.position.z = z / count;
    vector(result.position);
  }
  if (subject_slot)
    updated[*subject_slot] = result;
  // Copy back without reallocating: subject may refer into this vector.
  for (size_t i = 0; i < pool.size(); ++i)
    pool[i] = std::move(updated[i]);
  if (!subject_slot)
    subject = std::move(result);
  return count;
}
} // namespace ashford
