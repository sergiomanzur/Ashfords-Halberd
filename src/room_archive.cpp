// MIT recvx-decomp ps2_dvd_image.c::rdx_files, pinned USA archive order.
#include "room.hpp"
#include <algorithm>
#include <iterator>
#include <stdexcept>
namespace ashford {
size_t room_archive_index(uint8_t stage, uint8_t room, uint8_t room_case) {
  if (stage > 9 || room > 99 || room_case > 9)
    throw std::runtime_error("Room identity outside original filename range");
  static constexpr uint16_t keys[] = {
    0, 10, 20, 21, 30, 31, 40, 50, 60, 70,
    80, 90, 100, 110, 120, 130, 140, 150, 160, 1000,
    1001, 1002, 1020, 1021, 1030, 1040, 1050, 1060, 1070, 1080,
    1090, 1100, 1110, 1120, 1121, 1122, 1130, 1140, 2000, 2010,
    2011, 2020, 2030, 2031, 2040, 2050, 2060, 2070, 3000, 3010,
    3011, 3020, 3030, 3040, 3050, 3060, 3070, 3080, 3090, 3091,
    3100, 3110, 3120, 3130, 3140, 3150, 3160, 3170, 3180, 3190,
    3200, 3210, 3220, 3230, 3240, 4000, 4001, 4010, 4011, 4012,
    4020, 4030, 4040, 4050, 5000, 5001, 5010, 5500, 5510, 5520,
    5530, 5540, 5550, 5580, 5590, 5600, 5610, 5620, 5630, 5640,
    5650, 5660, 5670, 5680, 5690, 5700, 5710, 5800, 5810, 5820,
    5830, 5840, 6000, 6010, 6020, 6030, 6040, 6050, 6051, 6060,
    6070, 6080, 6090, 6100, 7000, 7010, 7020, 7030, 7031, 7040,
    7050, 7060, 7070, 7071, 7080, 7081, 7090, 7100, 7110, 7120,
    7130, 7140, 7150, 7160, 7170, 7180, 7181, 7190, 7191, 7200,
    7210, 7220, 7221, 7230, 7231, 7240, 7250, 8000, 8001, 8010,
    8020, 8030, 8040, 8050, 9000, 9010, 9020, 9030, 9040, 9050,
    9060, 9070, 9080, 9090, 9091, 9100, 9110, 9120, 9130, 9140,
    9150, 9160, 9170, 9180, 9190, 9200, 9210, 9220, 9230, 9240,
    9250, 9260, 9270, 9280, 9290, 9300, 9301, 9302, 9310, 9320,
    9321, 9340, 9350, 9360, 9370,
  };
  static_assert(std::size(keys) == 205);
  const auto key = uint16_t(stage * 1000 + room * 10 + room_case);
  const auto found = std::lower_bound(std::begin(keys), std::end(keys), key);
  if (found == std::end(keys) || *found != key)
    throw std::runtime_error("Missing authored room case in native archive");
  return size_t(found - std::begin(keys));
}
} // namespace ashford