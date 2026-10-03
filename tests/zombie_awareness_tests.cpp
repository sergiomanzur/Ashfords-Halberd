#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Malformed awareness context did not fail");
}
ScenarioState fixture() {
  ScenarioState state;
  state.enemies.resize(128);
  auto &root = state.enemies[0];
  root.flags = 1;
  root.id = 1;
  root.position = {1000, 2000,
                   3000}; // Cached origin and source Y0 must be used.
  root.zombie_work.emplace();
  root.zombie_work->personality = 1;
  root.model_work.emplace();
  root.model_work->joints.resize(18);
  root.joint_pose.resize(18);
  root.joint_pose[11].angles = std::array<int32_t, 3>{111, 0, 222};
  return state;
}
int main() {
  try {
    auto state = fixture();
    auto &root = state.enemies[0];
    root.zombie_flags = 0x1000;
    tick_zombie_awareness(state, 0, {0, 0, -10});
    ck(root.zombie_work->awareness == 0x41 &&
           root.zombie_work->awareness_timeout == 15 &&
           root.zombie_flags == 0x1400,
       "Initial source awareness detection failed");
    for (size_t i = 0; i < 29; ++i)
      tick_zombie_awareness(state, 0, {0, 0, 10});
    ck(root.zombie_work->awareness == 0x5e &&
           root.zombie_work->awareness_timeout == 15,
       "Outside view cleared the latched counter before its final check");
    tick_zombie_awareness(state, 0, {0, 0, 10});
    ck(root.zombie_work->awareness == 0 &&
           root.zombie_work->awareness_timeout == 14 &&
           root.zombie_flags == 0x1400,
       "Last counter view loss or original notice flag changed");
    for (size_t i = 0; i < 260; ++i)
      tick_zombie_awareness(state, 0, {0, 0, 10});
    ck(root.zombie_work->awareness == 12 &&
           root.zombie_work->awareness_timeout == 10 &&
           root.zombie_flags == 0x1400,
       "Retail unsigned timeout wrap or permanent notice latch changed");
    root.zombie_flags = 0x1000;
    root.zombie_work->awareness = 0x80;
    root.zombie_work->awareness_timeout = 7;
    tick_zombie_awareness(state, 0, {0, 0, 10});
    ck(root.zombie_work->awareness == 0x81 &&
           root.zombie_work->awareness_timeout == 7 &&
           root.zombie_flags == 0x1000,
       "Reserved bits or unlatched timeout changed");
    root.zombie_work->awareness = 0x20;
    tick_zombie_awareness(state, 0, {0, 0, 10});
    ck(root.zombie_work->awareness == 0x41,
       "Source pending view bit did not promote to latch");
    root.zombie_work->awareness = 0xff;
    root.zombie_work->awareness_timeout = 0;
    root.zombie_work->personality = 255;
    root.joint_pose[11].angles.reset();
    tick_zombie_awareness(state, 0,
                          {INFINITY, 0, 0}); // Low31 skips view query.
    ck(root.zombie_work->awareness == 0 &&
           root.zombie_work->awareness_timeout == 255 &&
           root.zombie_flags == 0x1400,
       "Source skipped query/high-bit byte carry changed");
    root.zombie_work->personality = 1;
    root.joint_pose[11].angles = std::array<int32_t, 3>{111, 0, 222};
    root.zombie_work->awareness = 0;
    root.zombie_flags = 0;
    // atan2(3,1) is around13028, outside retained personality1 cone10922 but
    // inside model-version0's14563 cone; recomputation would incorrectly see
    // it.
    tick_zombie_awareness(state, 0, {-3, 0, -1});
    ck(root.zombie_work->awareness == 1 && root.zombie_flags == 0,
       "Awareness recomputed personality from the model version");
    root.zombie_work->personality = 0;
    root.zombie_work->awareness = 0;
    tick_zombie_awareness(state, 0, {-3, 0, -1});
    ck(root.zombie_work->awareness == 0x41,
       "Retained wider source cone was ignored");
    root.zombie_work->awareness = 0;
    root.zombie_flags = 0;
    root.model_work->joints[11].matrix[7] = 10000;
    tick_zombie_awareness(state, 0, {0, 0, -10});
    ck(root.zombie_work->awareness == 0x41,
       "Awareness probe did not force source Y0");
    auto &body = state.enemies[1];
    body.flags = 0x81;
    body.id = 1;
    body.pose_owner = 0;
    body.linked_parent = 0;
    body.zombie_work.emplace();
    body.angles[1] = -16384;
    root.angles[1] = 16384; // EneSearch uses own yaw, unlike RotNeck.
    (*root.joint_pose[11].angles)[1] = -16384;
    tick_zombie_awareness(state, 1, {0, 0, 10});
    ck(body.zombie_work->awareness == 0x41 && body.zombie_flags == 0x400 &&
           body.joint_pose.empty(),
       "Linked awareness did not use own yaw and shared signed neck pose");
    ck(state.random_seed == 1 && root.position.x == 1000 &&
           root.joint_pose[11].angles->at(0) == 111,
       "Awareness changed RNG, placement or pose");
    const auto old = *body.zombie_work;
    auto unchanged = [&](auto f) {
      bad(f);
      ck(body.zombie_flags == 0x400 &&
             body.zombie_work->awareness == old.awareness &&
             body.zombie_work->awareness_timeout == old.awareness_timeout,
         "Invalid awareness query partially changed scalar work");
    };
    body.zombie_work->personality = 255;
    unchanged([&] { tick_zombie_awareness(state, 1, {}); });
    body.zombie_work->personality = 0;
    body.pose_owner = 128;
    unchanged([&] { tick_zombie_awareness(state, 1, {}); });
    body.pose_owner = 0;
    root.joint_pose[11].angles.reset();
    unchanged([&] { tick_zombie_awareness(state, 1, {}); });
    root.joint_pose[11].angles = std::array<int32_t, 3>{};
    unchanged([&] { tick_zombie_awareness(state, 1, {INFINITY, 0, 0}); });
    std::cout << "Native zombie awareness passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
