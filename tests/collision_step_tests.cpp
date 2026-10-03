#include "collision.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
namespace {
void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
template <class F> void bad(F fn) {
  bool failed = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Invalid consumed step data accepted");
}
CollisionRecord area() {
  CollisionRecord r;
  r.flags = 1;
  r.type = 2;
  r.floor = -2;
  r.parameters[0] = 1;
  r.position = {10, 7, 20};
  r.size = {4, 99, 6};
  return r;
}
void same(Vec3 a, Vec3 b, const char *message) {
  ck(a.x == b.x && a.y == b.y && a.z == b.z, message);
}
} // namespace
int main() {
  try {
    RoomCollision room;
    room.miscellaneous = {area()};
    CollisionBody body;
    body.position = {12, 999, 23};
    body.floor = -2;
    body.radius = 5;
    body.state_flags = 0x20081;
    // Inclusive center/edges and first static-then-dynamic match, Y unconsumed.
    for (Vec3 p : {Vec3{10, 0, 20}, Vec3{14, 0, 26}, body.position})
      ck(query_step_area(room, {area()}, -2, p.x, p.z) == 0,
         "Inclusive/static-first step query");
    ck(!query_step_area(room, {}, -2, 14.001f, 23),
       "Outside step edge accepted");
    ck(!query_step_area(room, {}, -1, 12, 23), "Wrong step floor accepted");
    for (int field = 0; field < 4; ++field) {
      auto filtered = room;
      auto &r = filtered.miscellaneous[0];
      if (field == 0)
        r.flags = 0;
      if (field == 1)
        r.type = 1;
      if (field == 2)
        r.parameters[0] = 0;
      if (field == 3)
        r.floor = 1;
      ck(query_step_area(filtered, {area()}, -2, 12, 23) == 1,
         "Dynamic merged-index/filter mismatch");
    }
    ck(check_step_state(room, {}, body) == 0x20081,
       "Step state flag set failed");
    for (uint32_t bits : {0x10u, 0x20u, 0x30u}) {
      auto b = body;
      b.state_flags |= bits;
      ck(check_step_state(room, {}, b) == (b.state_flags & ~0x20000u),
         "Source state30 gate ignored");
    }
    auto outside = body;
    outside.position.x = 99;
    ck(check_step_state(room, {}, outside) == 0x81,
       "Missing area did not clear step flag");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto unused = room;
    unused.miscellaneous[0].position.y = nan;
    unused.miscellaneous[0].size.y = nan;
    body.position.y = nan;
    body.height = nan;
    ck(query_step_area(unused, {}, -2, 12, 23) == 0 &&
           check_step_state(unused, {}, body) == 0x20081,
       "Step query consumed unrelated Y/height");
    body.position.y = 999;
    auto result = build_step_limits(room, {}, body, {});
    ck(result.state_flags == body.state_flags && result.remembered_area == 0 &&
           result.walls.size() == 4,
       "Current area/temporary boundaries not retained");
    const Vec3 positions[] = {
        {0, 7, 7.5f}, {16.5f, 7, 10}, {0, 7, 28.5f}, {-2.5f, 7, 10}};
    const Vec3 sizes[] = {{24, 1, 10}, {10, 1, 26}, {24, 1, 10}, {10, 1, 26}};
    for (size_t i = 0; i < 4; ++i) {
      const auto &r = result.walls[i];
      same(r.position, positions[i],
           "Literal N/E/S/W boundary position mismatch");
      same(r.size, sizes[i], "Literal N/E/S/W boundary size mismatch");
      ck(r.flags == 1 && r.type == 1 && r.floor == -2 &&
             r.attribute == 0x800000 &&
             r.parameters == std::array<uint8_t, 4>{},
         "Temporary boundary metadata differs");
    }
    ck(room.walls.empty(), "Pure step service mutated permanent room walls");
    auto fallback = build_step_limits(room, {}, outside, 0);
    ck(fallback.remembered_area == 0 && fallback.walls.size() == 4,
       "Previous area not used when current point outside");
    auto changed = room;
    changed.miscellaneous[0].flags = 0;
    changed.miscellaneous[0].floor = 7;
    ck(build_step_limits(changed, {}, outside, 0).walls.size() == 4,
       "Source fallback incorrectly filtered old area flags/floor");
    const auto absent = build_step_limits(room, {}, outside, {});
    ck(absent.walls.empty() && !absent.remembered_area &&
           absent.state_flags == 0x81,
       "No current/remembered area must clear20000");
    ck(build_step_limits(room, {}, body, 999).remembered_area == 0,
       "Unused stale remembered index must be replaced by current area");
    bad([&] { build_step_limits(room, {}, outside, 999); });
    auto empty = RoomCollision{};
    ck(build_step_limits(empty, {area()}, body, {}).remembered_area == 0,
       "Dynamic area alone must use merged index0");
    ck(build_step_limits(room, {area()}, outside, 1).remembered_area == 1,
       "Remembered dynamic area index not retained");
    // Any valid neighboring area covers a probe, including dynamic entries.
    auto adjacent = area();
    adjacent.position = {11, 0, 17};
    adjacent.size = {2, 1, 1};
    auto partial = build_step_limits(room, {adjacent}, body, {});
    ck(partial.walls.size() == 3 && partial.walls[0].position.x == 16.5f,
       "Dynamic north coverage did not preserve remaining E/S/W order");
    auto large = area();
    large.position = {0, 0, 0};
    large.size = {100, 0, 100};
    ck(build_step_limits(room, {large}, body, {}).walls.empty(),
       "Covered cardinal probes generated walls");
    auto within = body;
    within.radius = 1;
    ck(build_step_limits(room, {}, within, {}).walls.empty(),
       "Interior probes generated walls");
    // A remembered inactive area is used directly. If only north is uncovered,
    // the source never reads that old area's depth (nor its original height).
    auto old = room;
    old.miscellaneous[0].flags = 0;
    old.miscellaneous[0].size.z = nan;
    std::vector<CollisionRecord> covered;
    for (Vec3 p : {Vec3{16, 0, 22}, Vec3{11, 0, 27}, Vec3{6, 0, 22}}) {
      auto r = area();
      r.position = p;
      r.size = {2, 0, 2};
      covered.push_back(r);
    }
    const auto north_only = build_step_limits(old, covered, body, 0);
    ck(north_only.walls.size() == 1 && north_only.remembered_area == 0,
       "North-only fallback consumed unused old area depth");
    same(north_only.walls[0].size, {24, 1, 10},
         "North-only fallback source width differs");
    // Limit creation has no state30 gate of its own.
    auto gated = body;
    gated.state_flags |= 0x30;
    ck(build_step_limits(room, {}, gated, {}).walls.size() == 4,
       "Limits incorrectly consumed state30 gate");
    bad([&] { query_step_area(room, {}, -2, nan, 23); });
    auto malformed = room;
    malformed.miscellaneous[0].position.x = nan;
    bad([&] { query_step_area(malformed, {}, -2, 12, 23); });
    bad([&] { build_step_limits(unused, {}, body, {}); });
    auto radius = body;
    radius.radius = nan;
    bad([&] { build_step_limits(room, {}, radius, {}); });
    radius.radius = -1;
    bad([&] { build_step_limits(room, {}, radius, {}); });
    malformed = room;
    malformed.miscellaneous[0].position.x = std::numeric_limits<float>::max();
    malformed.miscellaneous[0].size.x = std::numeric_limits<float>::max();
    bad([&] { query_step_area(malformed, {}, -2, 12, 23); });
    auto huge = body;
    huge.radius = std::numeric_limits<float>::max();
    bad([&] { build_step_limits(room, {}, huge, {}); });
    malformed = room;
    malformed.miscellaneous[0].flags = 0;
    malformed.miscellaneous[0].position.x = nan;
    ck(query_step_area(malformed, {area()}, -2, 12, 23) == 1,
       "Filtered area consumed malformed geometry");
    std::cout
        << "Source step queries/state and four temporary boundaries pass\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
