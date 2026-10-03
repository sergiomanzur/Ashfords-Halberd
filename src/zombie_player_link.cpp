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
