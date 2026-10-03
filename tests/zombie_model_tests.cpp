#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void bad(F fn) {
  bool rejected = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected, "Invalid CalcEnemy input did not reject");
}
ModelMatrix base(float x, float y, float z) {
  return {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1};
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  ActorModel model;
  model.model.resize(16 + 18 * 52);
  auto put = [&](size_t p, uint32_t value) {
    for (size_t i = 0; i < 4; ++i)
      model.model[p + i] = uint8_t(value >> (8 * i));
  };
  model.model[0] = 'M';
  model.model[1] = 'D';
  model.model[2] = 'L';
  model.model[3] = 128;
  model.model[4] = 16;
  model.model[6] = 18;
  put(8, 0xffffffffu);
  for (size_t joint = 0; joint < 18; ++joint) {
    const auto at = 16 + joint * 52;
    put(at, 6);
    put(at + 4, 0xffffffffu);
    if (joint)
      put(at + 12, 0x3f800000u);
    put(at + 44, joint == 17 ? 0xffffffffu : uint32_t((joint + 1) * 52));
    put(at + 48, 0xffffffffu);
  }
  auto &root = s.enemies[0];
  root.flags = 1;
  root.id = 1;
  root.zombie_work.emplace();
  root.position = {100, 200, 300};
  root.model = std::make_shared<ActorModel>(std::move(model));
  root.joint_pose = actor_bind_pose(*root.model);
  ActorPlacement placement;
  placement.position = root.position;
  root.model_work =
      calculate_actor_model_work(*root.model, root.joint_pose, {}, placement);
  root.model_matrix = root.model_work->base_matrix;
  root.geometry.capsule_start = {11, 22, 33};
  root.geometry.capsule_end = {44, 55, 66};
  root.geometry.capsule_radius = 7;
  root.geometry.height = 123;
  auto &body = s.enemies[1];
  body.flags = 0x81;
  body.id = 1;
  body.zombie_work.emplace();
  body.zombie_flags = 0x80000000u;
  body.linked_parent = 0;
  body.pose_owner = 0;
  body.position = root.position;
  body.model = root.model;
  body.geometry.capsule_start = {77, 88, 99};
  body.geometry.capsule_radius = 8;
  body.geometry.collision_radius = 1.7f;
  return s;
}
int main() {
  try {
    auto s = fixture();
    auto cache = *s.enemies[0].model_work;
    s.enemies[0].position.y = 300;
    s.enemies[0].joint_pose[0].position->y = 999;
    s.enemies[0].model.reset();
    s.enemies[0].linked_parent = 999;
    s.enemies[0].model_matrix =
        base(0, std::numeric_limits<float>::quiet_NaN(), 0);
    calculate_zombie_model(s, 0);
    ck(s.enemies[0].geometry.height == -90 &&
           s.enemies[0].geometry.collision_height == -90 &&
           s.enemies[0].model_work->joints[10].matrix ==
               cache.joints[10].matrix &&
           s.enemies[0].geometry.capsule_radius == 7,
       "Ordinary root must read prior cache without touching unused "
       "pose/model/parent/base");
    s = fixture();
    s.enemies[0].position.y = 400;
    calculate_zombie_model(s, 1);
    ck(s.enemies[0].model_work->joints[10].matrix[7] == 410 &&
           s.enemies[0].geometry.capsule_start.y == 410 &&
           s.enemies[0].geometry.capsule_end.y == 405.5f &&
           s.enemies[0].geometry.capsule_radius == 2 &&
           s.enemies[1].geometry.height == 210 &&
           s.enemies[1].geometry.capsule_radius == 8 &&
           s.enemies[0].geometry.height == 123 &&
           s.enemies[0].model_matrix->at(7) == 400,
       "Attached source parent refresh/capsule/own height failed");
    for (int offset : {80, 84, 88, 140})
      for (int damage : {-2, -1}) {
        s = fixture();
        auto &work = *s.enemies[0].zombie_work;
        if (offset == 80)
          work.damage_80 = damage;
        else if (offset == 84)
          work.damage_84 = damage;
        else if (offset == 88)
          work.damage_88 = damage;
        else
          work.damage_8c = damage;
        calculate_zombie_model(s, 1);
        const bool trunk = (offset == 88 || offset == 140) && damage == -1;
        ck(s.enemies[0].geometry.capsule_start.y == (trunk ? 212.5f : 210.f) &&
               s.enemies[0].geometry.capsule_end.y == (trunk ? 203.f : 205.5f),
           "Capsule must inspect exact minus1 at EXP88/8c only");
      }
    s = fixture();
    s.enemies[1].flags &= ~0x80u;
    s.enemies[1].position = {700, 500, 900};
    calculate_zombie_model(s, 1);
    ck(s.enemies[0].model_work->joints[7].matrix[7] == 207 &&
           s.enemies[0].model_work->joints[8].matrix[7] == 501 &&
           s.enemies[0].model_work->joints[10].matrix[7] == 503 &&
           s.enemies[1].geometry.capsule_start.y == 506 &&
           s.enemies[1].geometry.capsule_end.y == 502 &&
           s.enemies[1].geometry.capsule_radius == 2.5f &&
           s.enemies[1].geometry.height == 3 &&
           s.enemies[0].geometry.capsule_radius == 7 &&
           s.enemies[0].model_work->first_joint == 0 &&
           !s.enemies[0].model_work->joint_count &&
           s.enemies[0].model_work->base_matrix[7] == 200 &&
           s.enemies[0].model_matrix->at(7) == 200 &&
           s.enemies[1].model_matrix->at(7) == 500,
       "Detached shared suffix must retain prefix/range and independent actor "
       "matrices");
    s.enemies[0].flags |= 0x1000;
    s.enemies[0].position.y = 999;
    calculate_zombie_model(s, 1);
    ck(s.enemies[0].model_work->joints[7].matrix[7] == 207,
       "Parent primary1000 must retain its own base after shared body suffix "
       "calculation");
    s = fixture();
    s.enemies[1].flags &= ~0x80u;
    s.enemies[1].pose_owner.reset();
    s.enemies[1].joint_pose = s.enemies[0].joint_pose;
    s.enemies[1].model_work = s.enemies[0].model_work;
    s.enemies[1].position = {700, 500, 900};
    calculate_zombie_model(s, 1);
    ck(s.enemies[0].model_work->joints[10].matrix[7] == 210 &&
           s.enemies[1].model_work->joints[10].matrix[7] == 503 &&
           s.enemies[1].model_work->joints[7].matrix[7] == 207 &&
           s.enemies[1].geometry.capsule_start.y == 213 &&
           s.enemies[1].geometry.capsule_end.y == 209 &&
           s.enemies[1].geometry.height == 3 &&
           s.enemies[1].model_matrix->at(7) == 500,
       "Detached distinct model work must keep parent and suffix caches "
       "separate");
    s = fixture();
    s.enemies[1].flags = 0x1001;
    s.enemies[1].position.y = 500;
    s.enemies[1].model_matrix = base(10, 50, 30);
    calculate_zombie_model(s, 1);
    ck(s.enemies[1].geometry.height == -447 &&
           s.enemies[1].model_matrix->at(7) == 50,
       "Detached primary1000 must consume its own retained matrix and preserve "
       "negative height");
    s = fixture();
    auto &frozen = s.enemies[0].model_work->joints[8];
    frozen.flags = 1;
    frozen.matrix = base(400, 500, 600);
    calculate_zombie_model(s, 1);
    ck(s.enemies[0].model_work->joints[8].matrix[7] == 500 &&
           s.enemies[0].model_work->joints[10].matrix[7] == 502 &&
           s.enemies[1].geometry.height == 302,
       "Frozen O_WORK matrix must drive descendant capsule and height");
    s = fixture();
    s.enemies[0].geometry.collision_radius = 1.7f;
    s.enemies[0].zombie_secondary_flags = 0x240;
    calculate_zombie_model(s, 0);
    ck(std::abs(s.enemies[0].geometry.collision_radius - 1.9f) < .00001 &&
           s.enemies[0].zombie_secondary_flags == 0x240,
       "Active source radius recovery failed");
    s.enemies[0].geometry.collision_radius = 2.4f;
    calculate_zombie_model(s, 0);
    ck(s.enemies[0].geometry.collision_radius == 2.5f &&
           s.enemies[0].zombie_secondary_flags == 0x40,
       "Radius clamp must clear only200");
    s = fixture();
    s.enemies[0].geometry.collision_radius =
        std::numeric_limits<float>::quiet_NaN();
    s.enemies[0].model_work->joints[10].matrix[3] =
        std::numeric_limits<float>::quiet_NaN();
    calculate_zombie_model(s, 0);
    ck(s.enemies[0].geometry.height == 10 &&
           std::isnan(s.enemies[0].geometry.collision_radius),
       "Ordinary height must ignore unused XZ/radius");
    for (int invalid = 0; invalid < 5; ++invalid) {
      s = fixture();
      const auto old = s.enemies[0].model_work->joints[10].matrix;
      s.enemies[0].position.y = 400;
      s.enemies[1].zombie_secondary_flags = 0x200;
      if (invalid == 0)
        s.enemies[1].position.y = std::numeric_limits<float>::quiet_NaN();
      if (invalid == 1)
        s.enemies[1].geometry.collision_radius =
            std::numeric_limits<float>::quiet_NaN();
      if (invalid == 2)
        s.enemies[1].linked_parent = 999;
      if (invalid == 3) {
        s.enemies[1].flags = 0x1001;
        s.enemies[1].model_matrix.reset();
      }
      if (invalid == 4) {
        s.enemies[0].joint_pose[8].angles =
            std::array<int32_t, 3>{1000, 2000, 3000};
        s.enemies[0].model_work->joints[10].flags = 1;
        s.enemies[0].model_work->joints[10].matrix[7] =
            std::numeric_limits<double>::quiet_NaN();
      }
      bad([&] { calculate_zombie_model(s, 1); });
      ck((invalid == 4 || s.enemies[0].model_work->joints[10].matrix == old) &&
             s.enemies[0].model_matrix->at(7) == 200 &&
             s.enemies[0].geometry.capsule_radius == 7 &&
             (invalid == 1 ? std::isnan(s.enemies[1].geometry.collision_radius)
                           : s.enemies[1].geometry.collision_radius == 1.7f) &&
             s.enemies[1].zombie_secondary_flags == 0x200,
         "Invalid CalcEnemy partially committed caches/capsule/base/radius "
         "flags");
    }
    std::cout << "Native CalcEnemy parent/suffix caches, limb fields, capsules "
                 "and atomic updates passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
