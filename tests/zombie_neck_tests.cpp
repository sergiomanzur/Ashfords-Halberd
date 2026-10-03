#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void bad(F f) {
  bool caught = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  ck(caught, "Invalid neck context did not reject");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  auto &root = s.enemies[0];
  root.flags = 1;
  root.id = 1;
  root.zombie_work.emplace();
  root.angles[1] = 0;
  root.model_work.emplace();
  root.model_work->joints.resize(12);
  root.joint_pose.resize(12);
  root.joint_pose[8].angles = std::array<int32_t, 3>{1, 2, 3};
  root.joint_pose[11].angles = std::array<int32_t, 3>{4, 5, 6};
  auto &b = s.enemies[1];
  b.flags = 0x81;
  b.id = 1;
  b.modes[0] = 1;
  b.linked_parent = 0;
  b.pose_owner = 0;
  b.zombie_work.emplace();
  b.zombie_flags = 0x80000000;
  b.torso_yaw = -1234;
  b.zombie_work->neck_yaw = -2000;
  return s;
}
int main() {
  try {
    auto s = fixture();
    ActorModelWork player;
    player.joints.resize(6);
    player.joints[5].matrix[11] = -10;
    s.enemies[0].zombie_flags = 0x1000;
    tick_zombie_neck(s, 1, player);
    ck(s.enemies[0].joint_pose[8].angles->at(1) == -1234 &&
           s.enemies[0].joint_pose[11].angles->at(1) == -2000 &&
           s.enemies[1].zombie_work->neck_yaw == -2000,
       "Override must write own short wax and retained signed neck");
    s = fixture();
    s.enemies[1].zombie_flags |= 0x1000;
    tick_zombie_neck(s, 1, {});
    ck(s.enemies[0].joint_pose[11].angles->at(1) == -2000,
       "Body override must bypass unused player/cache query");
    for (int variant = 0; variant < 4; ++variant) {
      s = fixture();
      s.enemies[variant & 1].zombie_flags |= 0x800;
      s.enemies[(variant >> 1) & 1].zombie_work->awareness = 0x40;
      tick_zombie_neck(s, 1, player);
      ck(s.enemies[1].zombie_work->neck_yaw == 62626 &&
             s.enemies[0].joint_pose[11].angles->at(1) == 62626 &&
             s.enemies[0].joint_pose[8].angles->at(1) == 2,
         "Parent/body flag unions or signed cached neck rotation failed");
    }
    s = fixture();
    s.enemies[0].angles[1] = 16384;
    s.enemies[1].angles[1] = -16384;
    s.enemies[0].zombie_flags = 0x800;
    s.enemies[1].zombie_work->awareness = 0x40;
    s.enemies[1].zombie_work->neck_yaw = 0;
    tick_zombie_neck(s, 1, player);
    ck(s.enemies[1].zombie_work->neck_yaw == 64626,
       "Linked search must use parent yaw");
    for (auto distance : {35.f, 36.f}) {
      s = fixture();
      s.enemies[0].zombie_flags = 0x800;
      s.enemies[0].zombie_work->awareness = 0x40;
      s.enemies[1].zombie_work->player_distance = distance;
      s.enemies[1].zombie_work->neck_yaw = 2000;
      tick_zombie_neck(s, 1, {});
      ck(s.enemies[1].zombie_work->neck_yaw == 1090,
         "Distance35 must decay without consuming player");
    }
    for (auto pair : {std::pair<int32_t, int32_t>{0, 0},
                      {900, 0},
                      {910, 0},
                      {32767, 31857},
                      {32768, 33678},
                      {64626, 0},
                      {65535, 0},
                      {-2000, 0}}) {
      s = fixture();
      s.enemies[1].zombie_work->neck_yaw = pair.first;
      tick_zombie_neck(s, 1, {});
      ck(s.enemies[1].zombie_work->neck_yaw == pair.second &&
             s.enemies[0].joint_pose[11].angles->at(1) == pair.second,
         "Original signed neck decay/clamp changed");
    }
    s = fixture();
    s.enemies[0].joint_pose.clear();
    s.enemies[0].model_work.reset();
    tick_zombie_neck(s, 0, {});
    s.enemies[1].modes[0] = 5;
    s.enemies[1].linked_parent.reset();
    tick_zombie_neck(s, 1, {});
    ck(s.enemies[1].zombie_work->neck_yaw == -2000,
       "No-op gates consumed unused owner/parent context");
    for (int invalid = 0; invalid < 5; ++invalid) {
      s = fixture();
      s.enemies[0].zombie_flags = 0x800;
      s.enemies[0].zombie_work->awareness = 0x40;
      auto p = player;
      if (invalid == 0)
        s.enemies[1].linked_parent = 128;
      if (invalid == 1)
        s.enemies[1].pose_owner = 128;
      if (invalid == 2)
        s.enemies[0].joint_pose[11].angles.reset();
      if (invalid == 3)
        p.joints.clear();
      if (invalid == 4)
        p.joints[5].matrix[3] = INFINITY;
      bad([&] { tick_zombie_neck(s, 1, p); });
      ck(s.enemies[1].zombie_work->neck_yaw == -2000 &&
             s.enemies[0].joint_pose[8].angles->at(1) == 2,
         "Invalid neck update partially committed pose or retained scalar");
      if (s.enemies[0].joint_pose[11].angles)
        ck(s.enemies[0].joint_pose[11].angles->at(1) == 5,
           "Failed tracking committed temporary seed neck yaw");
    }
    std::cout << "Native zombie neck controller tests passed\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
