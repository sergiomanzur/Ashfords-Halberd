#include "audio_room.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
template <class F> void rejects(F f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  check(caught, message);
}
void control_tests() {
  using namespace ashford;
  MidiVolumeFade fade;
  fade.request(true, 120);
  check(fade.remaining() == 21 && fade.driver_volume() == 1 && fade.active(),
        "Source120 begins at signed volume -127 with21 manager ticks");
  fade.tick();
  check(fade.driver_volume() == 8 && fade.remaining() == 20,
        "Signed float volume truncates before driver conversion");
  for (int i = 1; i < 21; ++i)
    fade.tick();
  check(fade.active() && fade.remaining() == 0 && fade.driver_volume() == 127,
        "Source fade does not complete until counter becomes negative");
  fade.tick();
  check(!fade.active() && !fade.stopped() && fade.driver_volume() == 127,
        "Source fade-in exact endpoint");
  fade.request(false, 120);
  fade.tick();
  check(fade.driver_volume() == 122, "Fade-out starts from retained volume");
  for (int i = 1; i < 21; ++i)
    fade.tick();
  check(fade.active() && !fade.stopped(),
        "Fade-out stop deferred to completion");
  fade.tick();
  check(fade.stopped() && !fade.active() && fade.driver_volume() == 1,
        "Source -127 stops playback but maps to driver1");
  fade.request(true, 400);
  check(fade.remaining() == 60, "Source400 means60ticks");
  fade.request(true, 250);
  check(fade.remaining() == 45, "Source250 means45ticks");
  fade.request(true, 0, -12);
  check(fade.remaining() == 1, "Zero timer uses minimum one tick");
  fade.tick();
  check(fade.active() && fade.driver_volume() == 116, "Custom signed limit");
  fade.tick();
  check(!fade.active() && fade.driver_volume() == 116, "Custom endpoint");
  fade.request(true, 120);
  fade.tick();
  fade.request(false, 0);
  fade.tick();
  fade.tick();
  check(fade.stopped(), "Interrupted fade-out finishes from current level");
  rejects([&] { fade.request(true, -1); }, "Negative source timer");
  rejects([&] { fade.request(true, 120, 1); }, "Positive source volume limit");
  rejects([&] { fade.request(true, 120, -128); },
          "Invalid source volume limit");
  RoomEffectDepth depth;
  depth.request(16);
  check(depth.tick() == 256, "Source first Hall depth step");
  for (int i = 1; i < 16; ++i)
    depth.tick();
  check(depth.level() == 4096 && depth.tick() == 4096,
        "Sixteenth exact depth target");
  depth.request(0);
  check(depth.tick() == 3840, "Room depth retains its previous level");
  depth.request(-1);
  check(depth.tick() == 3584, "Interrupted source depth ramp");
  for (int i = 1; i < 16; ++i)
    depth.tick();
  check(depth.level() == -256, "Signed room depth");
  depth.request(1);
  for (int i = 0; i < 15; ++i)
    depth.tick();
  depth.request(0);
  check(depth.tick() == 210, "Depth delta division truncates toward zero");
  for (int i = 1; i < 16; ++i)
    depth.tick();
  check(depth.level() == 0, "Last depth tick snaps residual to exact target");
}
} // namespace
int main() {
  try {
    control_tests();
    std::cout << "Native room audio controls passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
