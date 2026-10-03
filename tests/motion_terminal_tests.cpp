#include "motion.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
namespace {
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F fn) {
  bool caught = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  ck(caught, "Out-of-range terminal application must reject");
}
void put(Bytes &b, size_t at, uint32_t value, size_t count = 4) {
  for (size_t i = 0; i < count; ++i)
    b.at(at + i) = uint8_t(value >> (i * 8));
}
void flt(Bytes &b, size_t at, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, 4);
  put(b, at, bits);
}
Bytes fixture() {
  Bytes b(112);
  b[0] = 'M';
  b[1] = 'T';
  b[2] = 'N';
  b[3] = 0x10;
  put(b, 4, 16, 2);
  put(b, 6, 1, 2);
  put(b, 8, 0);
  put(b, 12, 80);
  put(b, 16, 12);
  put(b, 20, 2);
  put(b, 24, 3, 2);
  put(b, 26, 2, 2);
  put(b, 28, 20);
  put(b, 32, 56);
  for (size_t frame = 0; frame < 3; ++frame) {
    flt(b, 36 + frame * 12, float(2 + frame * 10));
    flt(b, 40 + frame * 12, float(4 + frame * 10));
    flt(b, 44 + frame * 12, float(6 + frame * 10));
    for (size_t axis = 0; axis < 3; ++axis)
      put(b, 72 + frame * 6 + axis * 2, uint16_t(100 + frame * 100 + axis), 2);
    put(b, 96 + frame * 2, uint16_t(0x1200 + frame), 2);
  }
  return b;
}
} // namespace
int main() {
  try {
    const auto motion = decode_motion(fixture());
    std::vector<JointPose> current(1);
    current[0].position = Vec3{8, 10, 12};
    current[0].angles = std::array<int32_t, 3>{0, 0, 0};
    const auto applied = apply_motion_pose(current, motion, 2 * 65536);
    ck(applied.pose[0].position->x == 2 &&
           *applied.pose[0].angles == std::array<int32_t, 3>{100, 101, 102},
       "Terminal compatibility sampling must avoid adjacent pointer bytes");
    ck(motion_attribute_at(motion, 2 * 65536, 0xffff) == 0x1200,
       "Terminal attributes must use the same bounded sampling index");
    for (uint8_t format : {uint8_t(0), uint8_t(0x10), uint8_t(0x20)}) {
      auto m = motion;
      m.flags = format;
      for (bool blended : {false, true}) {
        MotionApplicationOptions options;
        if (blended)
          options.blend_rate = .5f;
        const auto result = apply_motion_pose(current, m, 2 * 65536, options);
        ck(result.pose[0].position->x == (blended ? 5.f : 2.f) &&
               (*result.pose[0].angles)[0] == (blended ? 50 : 100),
           "Fast/Normal/Slow terminal blending mismatch");
        options.mode = 2;
        options.mirror_map = {0, -1};
        options.blend_rate.reset();
        const auto mirrored = apply_motion_pose(current, m, 2 * 65536, options);
        ck(mirrored.pose[0].position->x == -2 &&
               (*mirrored.pose[0].angles)[1] == uint16_t(-101),
           "Terminal mirrored channels mismatch");
      }
    }
    MotionApplicationOptions slow;
    slow.mode = 8;
    const auto halfway =
        apply_motion_pose(current, motion, 2 * 65536 + 32768, slow);
    ck(halfway.pose[0].position->x == 7 && (*halfway.pose[0].angles)[0] == 150,
       "Fractional terminal sampling must interpolate frame0 to frame1");
    const auto last = apply_motion_pose(current, motion, 65536 + 32768, slow);
    ck(last.pose[0].position->x == 7 && (*last.pose[0].angles)[0] == 150,
       "Ordinary final-frame interpolation must remain unchanged");
    auto minimal = fixture();
    minimal.resize(
        100); // Valid authored channels; no terminal attribute bytes.
    const auto bounded = decode_motion(minimal);
    ck(motion_attribute_at(bounded, 2 * 65536 + 65535, 0) == 0x1200 &&
           apply_motion_pose(current, bounded, 2 * 65536).pose[0].position->x ==
               2,
       "Boundary must not require adjacent bytes past the record");
    ck(sample_motion(motion, 4 * 65536)[0].position->x == 2,
       "Diagnostic preview looping must remain unchanged");
    bad([&] { apply_motion_pose(current, motion, -1); });
    bad([&] { apply_motion_pose(current, motion, 3 * 65536); });
    bad([&] { motion_attribute_at(motion, 3 * 65536, 0); });
    auto invalid = motion;
    invalid.attributes.pop_back();
    bad([&] { motion_attribute_at(invalid, 0, 0); });
    invalid.attributes.clear();
    ck(motion_attribute_at(invalid, -1, 0x4567) == 0x4567,
       "Missing attribute channel must retain previous metadata");
    invalid = motion;
    invalid.tracks[0].positions[0].x = NAN;
    bad([&] { apply_motion_pose(current, invalid, 2 * 65536); });
    ck(current[0].position->x == 8 && (*current[0].angles)[0] == 0 &&
           motion.tracks[0].positions[0].x == 2,
       "Failed boundary application must preserve inputs");
    int32_t frame = 2 * 65536;
    uint32_t direction = 0;
    ck(advance_motion_frame(2, 65536, 0, frame, direction) == 1 &&
           frame == 65536,
       "Terminal sampling must preserve original post-sample advancement");
    std::cout << "Source terminal motion checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
