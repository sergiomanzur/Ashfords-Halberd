#include "enemy_targeting.hpp"
#include <cmath>
#include <iostream>
#include <limits>
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
  ck(failed, "Invalid targeting input did not fail");
}
void views() {
  const Vec3 origin{};
  ck(search_enemy_player(origin, 0, {0, 0, -10}, 5461) == 0,
     "Forward player missing from view cone");
  ck(search_enemy_player(origin, 0, {0, 0, 10}, 5461) == -1 &&
         search_enemy_player(origin, 0, {10, 0, 0}, 5461) == -1,
     "Rear/side player inside narrow view cone");
  ck(search_enemy_player(origin, 16384, {-10, 0, 0}, 5461) == 16384 &&
         search_enemy_player(origin, -16384, {10, 0, 0}, 5461) == -16384,
     "Quarter-turn source yaw or signed return changed");
  ck(search_enemy_player(origin, 0, {1, 0, -3}, 5461) == -3355 &&
         enemy_direction(origin, {1, 0, -3}) == -3355,
     "Original float atan2 truncation changed");
  ck(search_enemy_player(origin, 0, {.00015f, 0, -1}, 5461) == -2 &&
         enemy_direction(origin, {.00015f, 0, -1}) == -1,
     "Valid view yaw -1 was not remapped without changing direction query");
  ck(search_enemy_player(origin, 0, {0, 20, -10}, 5461) == -1,
     "View cone lost three-dimensional normalization");
  ck(search_enemy_player(origin, 0, origin, 5461) == -1 &&
         search_enemy_player(origin, 0, origin, 0) == -32768 &&
         enemy_direction(origin, origin) == 0,
     "Source coincident-vector or zero-cone behavior changed");
  ck(search_enemy_player(origin, 0, {10, 0, 0}, 0) == -16384 &&
         search_enemy_player(origin, 0, {10, 0, 0}, 32768) == -16384 &&
         search_enemy_player(origin, 0, {10, 0, 0}, 65536) == -1,
     "Zero-cone fallback was confused with word-wrapped nonzero cone");
  ck(search_enemy_player(origin, 65536, {0, 0, -10}, 5461) == 0,
     "Actor yaw did not wrap as a Ninja angle");
  // Cone r=16384 has a zero cosine in the source quarter table. Its side
  // boundary uses strict a<er, so exact side targets are outside.
  ck(search_enemy_player(origin, 0, {-10, 0, 0}, 16384) == -1 &&
         search_enemy_player(origin, 0, {10, 0, -.01f}, 16384) == -16373 &&
         search_enemy_player(origin, 0, {10, 0, .01f}, 16384) == -1,
     "View boundary did not retain strict source comparison");
  const float nan = std::numeric_limits<float>::quiet_NaN();
  bad([&] { search_enemy_player({nan, 0, 0}, 0, {}, 0); });
  bad([&] { search_enemy_player({}, 0, {0, nan, 0}, 0); });
  bad([&] { search_enemy_player({}, 0, {1e30f, 0, 0}, 0); });
}
void necks() {
  const Vec3 origin{}, front{0, 0, -10}, right{10, 0, 0}, left{-10, 0, 0};
  ck(turn_enemy_neck(origin, front, 500, 9102, 0) == 0 &&
         turn_enemy_neck(origin, front, 65536 - 500, 9102, 0) == 0,
     "Neck did not snap to a close target across the word seam");
  ck(turn_enemy_neck(origin, front, 2000, 9102, 0) == 1090 &&
         turn_enemy_neck(origin, front, 65536 - 2000, 9102, 0) == 64446,
     "Zero-angle neck branch did not use original signed-side step");
  ck(turn_enemy_neck(origin, left, 0, 9102, 0) == 910 &&
         turn_enemy_neck(origin, right, 0, 9102, 0) == 64626,
     "Neck step direction differs from original 910 words");
  ck(turn_enemy_neck(origin, left, 9000, 9102, 0) == 9102 &&
         turn_enemy_neck(origin, right, 65536 - 9000, 9102, 0) == 56434,
     "Neck overshoot was not clamped to both source limits");
  ck(turn_enemy_neck(origin, left, 16384, 9102, 0) == 16384,
     "Close out-of-range target should retain current neck yaw");
  ck(turn_enemy_neck(origin, left, 0, 9102, 16384) == 0 &&
         turn_enemy_neck(origin, front, 0, 9102, 65536) == 0,
     "Neck target did not subtract wrapped actor yaw");
  ck(turn_enemy_neck(origin, left, 0, 100, 0) == 100 &&
         turn_enemy_neck(origin, right, 0, 100, 0) == 65436,
     "Narrow valid limit did not retain source step clamping");
  bad([&] { turn_enemy_neck(origin, front, 0, -1, 0); });
  bad([&] { turn_enemy_neck(origin, front, 0, 32768, 0); });
  bad([&] { turn_enemy_neck({INFINITY, 0, 0}, front, 0, 9102, 0); });
}
CollisionRecord trigger(uint8_t action = 0) {
  CollisionRecord record;
  record.flags = 1;
  record.type = 2;
  record.floor = -1;
  record.position = {2, 123, 4};
  record.size = {6, 0, 8};
  record.parameters = {1, action, 222, 111};
  return record;
}
void floors() {
  RoomCollision room;
  room.floors = {trigger()};
  std::vector<CollisionRecord> dynamic{trigger()};
  auto query = [&](Vec3 position, int32_t floor = -1, uint8_t id = 1,
                   uint8_t action = 0) {
    return enemy_floor_trigger(room, dynamic, position, floor, id, action);
  };
  ck(query({2, -10000, 4}) == &room.floors[0] &&
         query({8, 99999, 12}) == &room.floors[0],
     "Floor trigger changed static priority, inclusive XZ or ignored Y");
  ck(!query({1.999f, 0, 4}) && !query({8.001f, 0, 12}) &&
         !query({2, 0, 3.999f}) && !query({8, 0, 12.001f}) &&
         !query({3, 0, 5}, 0) && !query({3, 0, 5}, -1, 2) &&
         !query({3, 0, 5}, -1, 1, 1),
     "Floor trigger bounds or source identity filters changed");
  for (auto flags : {0, 2}) {
    room.floors[0].flags = flags;
    ck(query({3, 0, 5}) == &dynamic[0],
       "Inactive static record blocked dynamic");
  }
  room.floors[0] = trigger();
  room.floors[0].type = 1;
  ck(query({3, 0, 5}) == &dynamic[0],
     "Non-trigger static record blocked dynamic");
  room.floors.clear();
  dynamic.insert(dynamic.begin(), trigger(1));
  ck(query({3, 0, 5}) == &dynamic[1] &&
         query({3, 0, 5}, -1, 1, 1) == &dynamic[0],
     "Dynamic trigger source order changed");
  dynamic.clear();
  room.floors = {trigger()};
  room.floors[0].size = {};
  ck(query({2, 0, 4}) == &room.floors[0] && !query({2.001f, 0, 4}),
     "Zero-size source trigger behavior changed");
  room.floors[0].size.x = -1;
  ck(!query({2, 0, 4}), "Negative source extent created a trigger");
  room.floors[0].size.x = INFINITY;
  bad([&] { query({2, 0, 4}); });
  room.floors[0].size.x = std::numeric_limits<float>::max();
  room.floors[0].position.x = std::numeric_limits<float>::max();
  bad([&] { query({2, 0, 4}); });
  room.floors[0] = trigger();
  ck(query({3, INFINITY, 5}) == &room.floors[0],
     "Unused Y incorrectly changed a source XZ-only floor query");
  bad([&] { query({INFINITY, 0, 5}); });
}
Vec3 joint_origin(const ActorJointWork &joint) {
  return {float(joint.matrix[3]), float(joint.matrix[7]),
          float(joint.matrix[11])};
}
void cached_neck() {
  ScenarioState state;
  state.enemies.resize(128);
  auto &root = state.enemies[0];
  root.flags = 1;
  root.id = 1;
  root.angles = {123, 0, 456};
  root.position = {900, 999, 900}; // Cached joint origins must take precedence.
  root.model_work.emplace();
  root.model_work->joints.resize(18);
  root.model_work->joints[11].matrix[3] = 2;
  root.model_work->joints[11].matrix[11] = 4;
  root.joint_pose.resize(18);
  root.joint_pose[11].angles = std::array<int32_t, 3>{222, 0, 333};
  root.joint_pose[11].position = Vec3{7, 8, 9};
  root.damage.fill(99);
  root.frame = 12345;
  root.motion_number = 202;
  root.control3 = 67;
  ActorModelWork player;
  player.joints.resize(6);
  player.joints[5].matrix[3] = 12;
  player.joints[5].matrix[11] = 4;
  const auto old_work = *root.model_work;
  rotate_zombie_neck(state, 0, 11, player, 5);
  ck((*root.joint_pose[11].angles)[1] == 64626 &&
         (*root.joint_pose[11].angles)[0] == 222 &&
         (*root.joint_pose[11].angles)[2] == 333 &&
         root.joint_pose[11].position->x == 7 && root.position.x == 900 &&
         root.damage[0] == 99 && root.frame == 12345 &&
         root.motion_number == 202 && root.control3 == 67,
     "Cached neck wrapper changed unrelated work or used actor origin");
  for (size_t joint = 0; joint < old_work.joints.size(); ++joint)
    ck(root.model_work->joints[joint].matrix == old_work.joints[joint].matrix,
       "Neck wrapper recalculated prior cached matrices");
  auto &body = state.enemies[1];
  body.id = 1;
  body.flags = 0x81;
  body.linked_parent = 0;
  body.pose_owner = 0;
  body.angles[1] = 16384; // Own yaw must be ignored for linked neck turning.
  (*root.joint_pose[11].angles)[1] = 0;
  rotate_zombie_neck(state, 1, 11, player, 5);
  ck((*root.joint_pose[11].angles)[1] == 64626 && body.joint_pose.empty(),
     "Linked body did not share owner pose and parent yaw");
  root.angles[1] = -16384;
  (*root.joint_pose[11].angles)[1] = 500;
  rotate_zombie_neck(state, 1, 11, player, 5);
  ck((*root.joint_pose[11].angles)[1] == 0,
     "Linked neck did not use signed parent yaw");
  auto unchanged = [&](auto f) {
    bad(f);
    ck((*root.joint_pose[11].angles)[1] == 0 &&
           root.model_work->joints[11].matrix == old_work.joints[11].matrix,
       "Rejected neck input partially committed yaw or matrices");
  };
  unchanged([&] { rotate_zombie_neck(state, 128, 11, player, 5); });
  unchanged([&] { rotate_zombie_neck(state, 1, 18, player, 5); });
  unchanged([&] { rotate_zombie_neck(state, 1, 11, player, 6); });
  body.linked_parent.reset();
  unchanged([&] { rotate_zombie_neck(state, 1, 11, player, 5); });
  body.linked_parent = 128;
  unchanged([&] { rotate_zombie_neck(state, 1, 11, player, 5); });
  body.linked_parent = 0;
  body.pose_owner = 128;
  unchanged([&] { rotate_zombie_neck(state, 1, 11, player, 5); });
  body.pose_owner = 0;
  root.pose_owner = 1;
  unchanged([&] { rotate_zombie_neck(state, 1, 11, player, 5); });
  root.pose_owner.reset();
  root.model_work.reset();
  bad([&] { rotate_zombie_neck(state, 1, 11, player, 5); });
  ck((*root.joint_pose[11].angles)[1] == 0,
     "Missing ML_WORK changed source neck yaw");
  root.model_work = old_work;
  player.joints[5].matrix[3] = INFINITY;
  unchanged([&] { rotate_zombie_neck(state, 1, 11, player, 5); });
  player.joints[5].matrix[3] = 12;
  root.joint_pose[11].angles.reset();
  bad([&] { rotate_zombie_neck(state, 1, 11, player, 5); });
  ck(!root.joint_pose[11].angles, "Missing neck pose was created on rejection");
}
void signed_authored_neck() {
  // Source CNK_OBJECT angles are signed32 even though motion tracks are
  // unsigned16. A forward target takes a different branch for raw -2000.
  ActorModel model;
  model.model.resize(16 + 18 * 52);
  model.model[0] = 'M';
  model.model[1] = 'D';
  model.model[2] = 'L';
  model.model[3] = 128;
  model.model[4] = 16;
  model.model[6] = 18;
  const uint32_t negative = uint32_t(-2000);
  for (size_t byte = 0; byte < 4; ++byte)
    model.model[16 + 11 * 52 + 24 + byte] = uint8_t(negative >> (byte * 8));
  ScenarioState state;
  state.enemies.resize(128);
  auto &root = state.enemies[0];
  root.flags = 1;
  root.id = 1;
  root.joint_pose = actor_bind_pose(model);
  ck((*root.joint_pose[11].angles)[1] == -2000,
     "Bind pose did not retain the source signed32 object angle");
  root.model_work.emplace();
  root.model_work->joints.resize(18);
  ActorModelWork player;
  player.joints.resize(6);
  player.joints[5].matrix[11] = -10;
  rotate_zombie_neck(state, 0, 11, player, 5);
  ck((*root.joint_pose[11].angles)[1] == 62626,
     "Authored signed neck angle took the unsigned source branch");
  rotate_zombie_neck(state, 0, 11, player, 5);
  ck((*root.joint_pose[11].angles)[1] == 63536,
     "Next neck update did not use the canonical source output word");
}
int main() {
  try {
    views();
    necks();
    floors();
    cached_neck();
    signed_authored_neck();
    std::cout << "Native enemy targeting primitives passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
