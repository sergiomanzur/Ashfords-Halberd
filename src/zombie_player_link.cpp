#include "ninja_matrix.hpp"
#include "stateful_wall_internal.hpp"
#include "zombie.hpp"
#include "zombie_player_link_tables.hpp"
#include <stdexcept>
namespace ashford {
Vec3 zombie_player_link_offset(uint8_t player, uint32_t motion, int32_t frame) {
  if (player > 3)
    throw std::runtime_error("Invalid PlayerLink character");
  constexpr uint32_t bases[] = {400, 425, 450, 425};
  if (motion < bases[player])
    throw std::runtime_error("Invalid PlayerLink motion");
  size_t clip;
  switch (motion - bases[player]) {
  case 1:
    clip = 0;
    break;
  case 2:
    clip = 1;
    break;
  case 4:
    clip = 2;
    break;
  case 5:
    clip = 3;
    break;
  default:
    throw std::runtime_error("Invalid PlayerLink motion");
  }
  const size_t count = clip < 2 ? 60 : 50;
  if (frame < 0 || size_t(frame) >= count)
    throw std::runtime_error("Invalid PlayerLink authored frame");
  using namespace detail;
  const PlayerLinkXZ *tables[4][4] = {{cl_mtn01_xz.data(), cl_mtn02_xz.data(),
                                       cl_mtn04_xz.data(), cl_mtn05_xz.data()},
                                      {ch_mtn01_xz.data(), ch_mtn02_xz.data(),
                                       ch_mtn04_xz.data(), ch_mtn05_xz.data()},
                                      {st_mtn01_xz.data(), st_mtn02_xz.data(),
                                       st_mtn04_xz.data(), st_mtn05_xz.data()},
                                      {ch_mtn01_xz.data(), ch_mtn02_xz.data(),
                                       ch_mtn04_xz.data(), ch_mtn05_xz.data()}};
  const auto offset = tables[player][clip][frame];
  return {offset.x, 0.f, offset.z};
}
} // namespace ashford
namespace ashford {
void link_zombie_player(ScenarioState &s, size_t root, ScenarioEntity &p) {
  if (s.enemies.size() != 128 || root >= s.enemies.size() ||
      !(s.enemies[root].flags & 1) || s.enemies[root].id != 1 ||
      !s.enemies[root].zombie_work)
    throw std::runtime_error("Invalid PlayerLink enemy work");
  for (const auto &actor : s.enemies)
    if (&actor == &p)
      throw std::runtime_error("PlayerLink player aliases enemy pool");
  auto &e = s.enemies[root];
  auto enemy = detail::ninja_identity();
  detail::ninja_translate(enemy, e.position);
  detail::ninja_rotate(enemy, e.angles);
  const auto anchor = detail::ninja_point(enemy, e.zombie_work->action_anchor);
  if (!p.model_matrix)
    throw std::runtime_error("Missing PlayerLink per-work matrix");
  const auto previous =
      detail::ninja_point(detail::ninja_load(*p.model_matrix), {});
  const Vec3 position{anchor.x, p.position.y, anchor.z};
  auto angles = p.angles;
  angles[1] =
      uint16_t(uint32_t(e.angles[1]) + uint32_t(int32_t(e.grab_yaw_offset)));
  Vec3 offset{};
  if (p.flags & 0x1000)
    offset = zombie_player_link_offset(s.player_id, p.motion_number,
                                       p.frame / 65536);
  auto player_matrix = detail::ninja_identity();
  detail::ninja_translate(player_matrix, position);
  detail::ninja_rotate(player_matrix, angles);
  const auto proposed = detail::ninja_point(player_matrix, offset);
  std::optional<ModelMatrix> rebuilt;
  if (p.flags & 0x1000) {
    auto matrix = detail::ninja_identity();
    detail::ninja_translate(matrix, proposed);
    detail::ninja_rotate(matrix, {angles[0], p.yaw_correction, angles[2]});
    rebuilt = detail::ninja_store(matrix);
  }
  const auto old_position = p.position;
  const auto old_angles = p.angles;
  Vec3 corrected;
  // The source Wall2 sees updated player work XZ/yaw, while its proposal also
  // includes the authored offset. Restore those scalars if any late dependency
  // fails; the collision transaction validates our enemy correction pre-commit.
  p.position = position;
  p.angles = angles;
  try {
    detail::run_stateful_wall(
        s, p, p, proposed, previous, p.geometry.radius, p.geometry.height, true,
        [&](const StatefulWallResult &r) {
          corrected = {detail::ninja_finite(e.position.x + r.displacement.x),
                       detail::ninja_finite(e.position.y + r.displacement.y),
                       detail::ninja_finite(e.position.z + r.displacement.z)};
        });
  } catch (...) {
    p.position = old_position;
    p.angles = old_angles;
    throw;
  }
  if (rebuilt)
    p.model_matrix = *rebuilt;
  e.position = corrected;
}
} // namespace ashford
