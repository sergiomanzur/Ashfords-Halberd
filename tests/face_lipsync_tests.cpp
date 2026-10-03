#include "face_lipsync.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
namespace {
void ck(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
}
void near(float a, float b) {
  if (std::abs(a - b) >= 0.00001f)
    std::cerr << "Actual " << a << ", expected " << b << '\n';
  ck(std::abs(a - b) < 0.00001f, "Incorrect lipsync parameter");
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void flt(Bytes &b, size_t p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put(b, p, v);
}
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Malformed lipsync input accepted");
}
Bytes fixture() {
  Bytes b(3 * 164);
  // Shape zero neutral; packed angle words must never be decoded as floats.
  for (size_t i = 1; i < 3; ++i) {
    flt(b, i * 164 + 12 * 4, float(i * 4));
    flt(b, i * 164 + 13 * 4, float(i * 8));
    put(b, i * 164 + 128, uint32_t(i == 1 ? -32768 : 32768));
    flt(b, i * 164 + 132, float(i * 2));
    put(b, i * 164 + 148, uint32_t(i == 1 ? -16384 : 16384));
    put(b, i * 164 + 152, uint32_t(i == 1 ? 8192 : -8192));
    flt(b, i * 164 + 156, float(i * 3));
  }
  return b;
}
} // namespace
int main() {
  try {
    // Original motion header: two enemies, room motion, mask, then lip bank.
    Bytes room(1500);
    flt(room, 0, 10);
    put(room, 16, 64);
    put(room, 64 + 0x88, 2);
    put(room, 64 + 8, 1100);
    put(room, 24, 400);
    put(room, 400 + 4 * 4, 440);
    put(room, 440, 5);
    room[444] = 1;
    room[445] = 12;
    room[446] = 2;
    room[447] = 4;
    room[448] = 255;
    put(room, 449, 0); // Empty clip preserves index one.
    put(room, 453, 1);
    room[457] = 255;
    put(room, 458, 0xffffffff);
    const auto resident = decode_room_lip_clips(room);
    ck(resident.size() == 3 && resident[0] == Bytes({1, 12, 2, 4, 255}) &&
           resident[1].empty() && resident[2] == Bytes({255}),
       "Room lip clip indexing");
    auto absent = room;
    put(absent, 416, 0);
    ck(decode_room_lip_clips(absent).empty(), "Absent resident lip bank");
    put(absent, 24, 0);
    ck(decode_room_lip_clips(absent).empty(), "Absent motion header");
    bad([&] {
      auto b = room;
      put(b, 416, 410);
      decode_room_lip_clips(b);
    });
    bad([&] {
      auto b = room;
      put(b, 412, 460);
      decode_room_lip_clips(b);
    });
    bad([&] {
      auto b = room;
      put(b, 458, 1);
      b[462] = 255;
      put(b, 32, 463); // Bound bank before missing sentinel, retain valid ROM.
      decode_room_lip_clips(b);
    });
    bad([&] {
      auto b = room;
      put(b, 440, 0xffffffff - 1);
      decode_room_lip_clips(b);
    });
    const auto bytes = fixture();
    auto shapes = decode_lip_shapes(bytes);
    ck(shapes.size() == 3, "Packed shape count");
    ck(shapes[1].jaw_angle == -32768, "Signed packed jaw angle");
    auto clip = start_lip_playback({1, 12, 2, 4, 255}, shapes.size());
    FaceParameters p;
    p.muscle.fill(9);
    p.eye = {5, 6, 7};
    tick_lip_playback(p, shapes, clip, 1u << 12);
    near(p.muscle[12], 4);
    near(p.muscle[13], 9);
    near(p.jaw_angle, -180);
    near(p.tongue.x, -90);
    near(p.tongue.y, 45);
    near(p.eye.x, 5);
    ck(clip.active && clip.count == 2, "Two-tick advance");
    tick_lip_playback(p, shapes, clip,
                      1u << 12); // Exactly ten remaining: direct setter.
    near(p.jaw_angle, -180);
    tick_lip_playback(p, shapes, clip, 1u << 12); // Eight remaining: 20% blend.
    near(p.muscle[12], 4.8f);
    near(p.jaw_angle, -108);
    near(p.tongue.x, -54);
    near(p.eye.y, 6);
    for (int i = 0; i < 3; ++i)
      tick_lip_playback(p, shapes, clip, 1u << 12);
    ck(clip.count == 12, "First segment duration");
    tick_lip_playback(p, shapes, clip, 1u << 12);
    near(p.muscle[12], 3.2f);
    near(p.jaw_angle, 72); // Four ticks => level 0.4.
    tick_lip_playback(p, shapes, clip, 1u << 12);
    near(p.muscle[12], 1.6f);
    near(p.jaw_angle, 36);
    tick_lip_playback(p, shapes, clip, 1u << 12);
    ck(!clip.active, "Sentinel terminates playback");
    near(p.muscle[12], 0);
    near(p.jaw_angle, 0);
    near(p.muscle[13], 9);
    near(p.eye.z, 7);
    p.muscle[12] = 17;
    tick_lip_playback(p, shapes, clip, 1u << 12);
    near(p.muscle[12], 17); // Inactive callers do not restart the clip.
    auto odd = start_lip_playback({1, 1, 2, 3, 255}, 3);
    tick_lip_playback(p, shapes, odd, 1u << 12);
    near(p.muscle[12], 0.4f);
    tick_lip_playback(p, shapes, odd, 1u << 12);
    near(p.muscle[12], 1.6f);
    ck(odd.count == 3 && odd.cursor == 2, "Odd duration carries one tick");
    tick_lip_playback(p, shapes, odd, 1u << 12);
    ck(!odd.active, "Odd clip termination");
    auto empty = start_lip_playback({255}, 3);
    tick_lip_playback(p, shapes, empty, 1u << 12);
    ck(!empty.active, "Empty clip termination");
    for (const auto test : {std::pair<int, float>{0, 4.8f},
                            {1, 4.16f},
                            {2, 5.7888546f},
                            {3, 4.3820109f},
                            {4, 4.0f},
                            {77, 4.8f}}) {
      shapes[1].interpolation = int16_t(test.first);
      auto c = start_lip_playback({1, 12, 2, 12, 255}, 3);
      for (int i = 0; i < 3; ++i)
        tick_lip_playback(p, shapes, c, 1u << 12);
      near(p.muscle[12], test.second);
    }
    bad([&] {
      auto b = bytes;
      b.pop_back();
      decode_lip_shapes(b);
    });
    bad([&] { decode_lip_shapes({}); });
    bad([&] {
      auto b = bytes;
      put(b, 48, 0x7fc00000);
      decode_lip_shapes(b);
    });
    bad([&] { start_lip_playback({1}, 3); });
    bad([&] { start_lip_playback({1, 0, 255}, 3); });
    bad([&] { start_lip_playback({3, 2, 255}, 3); });
    bad([&] { start_lip_playback({1, 2}, 3); });
    bad([&] { start_lip_playback({255}, 0); });
    // Unused eyes/frame words are not ordinary shape-key float/frame data.
    auto ignored = bytes;
    put(ignored, 136, 0x7fc00000);
    put(ignored, 160, 0xffff8000);
    ck(decode_lip_shapes(ignored).size() == 3, "Unused lip words decoded");
    // A late packed-angle overflow must discard earlier muscle interpolation
    // and the segment cursor/counter transition.
    auto late = start_lip_playback({1, 12, 2, 12, 255}, 3);
    for (int i = 0; i < 2; ++i)
      tick_lip_playback(p, shapes, late, 1u << 12);
    shapes[1].interpolation = 0;
    shapes[1].tongue_y = INT32_MIN;
    shapes[2].tongue_y = INT32_MAX;
    p.muscle[12] = 23;
    const auto old_parameters = p;
    const auto old_playback = late;
    bad([&] { tick_lip_playback(p, shapes, late, 1u << 12); });
    near(p.muscle[12], 23);
    ck(p.muscle == old_parameters.muscle &&
           p.jaw_angle == old_parameters.jaw_angle &&
           p.jaw_translation == old_parameters.jaw_translation &&
           p.eye.x == old_parameters.eye.x && p.eye.y == old_parameters.eye.y &&
           p.eye.z == old_parameters.eye.z &&
           p.tongue.x == old_parameters.tongue.x &&
           p.tongue.y == old_parameters.tongue.y &&
           p.tongue.z == old_parameters.tongue.z,
       "Late failure changed facial parameters");
    ck(late.count == old_playback.count && late.cursor == old_playback.cursor &&
           late.time == old_playback.time &&
           late.current == old_playback.current &&
           late.next == old_playback.next &&
           late.active == old_playback.active &&
           late.segments.size() == old_playback.segments.size(),
       "Late failure changed playback");
    for (size_t i = 0; i < late.segments.size(); ++i)
      ck(late.segments[i].shape == old_playback.segments[i].shape &&
             late.segments[i].duration == old_playback.segments[i].duration,
         "Late failure changed clip segments");
    shapes = decode_lip_shapes(bytes);
    auto broken = start_lip_playback({1, 12, 2, 12, 255}, 3);
    shapes[1].interpolation = 0;
    shapes[1].muscle[12] = INFINITY;
    p.muscle[12] = 23;
    bad([&] { tick_lip_playback(p, shapes, broken, 1u << 12); });
    near(p.muscle[12], 23);
    ck(broken.cursor == 0 && broken.count == 0 && broken.active,
       "Failed update changed playback");
    ck(bytes == fixture(), "Source bytes mutated");
    std::cout << "Packed facial lipsync decoding and playback passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
