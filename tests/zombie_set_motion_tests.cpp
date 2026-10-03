#include "effects.hpp"
#include "zombie.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < .001f; }
template <class F> void bad(F fn) {
  bool caught = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    caught = true;
  }
  ck(caught, "Invalid SetMtn input did not reject");
}
ScenarioState fixture() {
  ScenarioState s;
  s.enemies.resize(128);
  ActorModel model;
  model.model.resize(16 + 18 * 52);
  auto put = [&](size_t p, uint32_t v) {
    for (size_t i = 0; i < 4; ++i)
      model.model[p + i] = uint8_t(v >> (8 * i));
  };
  model.model[0] = 'M';
  model.model[1] = 'D';
  model.model[2] = 'L';
  model.model[3] = 128;
  model.model[4] = 16;
  model.model[6] = 18;
  put(8, 0xffffffffu);
  for (size_t j = 0; j < 18; ++j) {
    const size_t p = 16 + j * 52;
    put(p, 6);
    put(p + 4, 0xffffffffu);
    if (j)
      put(p + 12, 0x3f800000u);
    put(p + 44, j == 17 ? 0xffffffffu : uint32_t((j + 1) * 52));
    put(p + 48, 0xffffffffu);
  }
  auto bank = std::make_shared<MotionBank>(512);
  for (uint32_t number : {0u, 1u, 2u, 7u, 8u, 32u, 40u, 41u, 85u, 117u, 120u,
                          121u, 125u, 200u, 202u, 240u, 241u, 325u}) {
    Motion motion;
    motion.flags = 0x10;
    motion.frames = 80;
    motion.tracks.resize(number >= 200 ? 10 : 18);
    for (auto &track : motion.tracks)
      track.angles.assign(80, std::array<uint16_t, 3>{});
    for (int frame = 0; frame < 80; ++frame)
      motion.tracks[0].positions.push_back({5.f + frame, 9, 10.f + 2 * frame});
    (*bank)[number] = std::move(motion);
  }
  auto &e = s.enemies[0];
  e.flags = 1;
  e.id = 1;
  e.modes[0] = 1;
  e.zombie_work.emplace();
  e.position = {100, 10, 200};
  e.model = std::make_shared<ActorModel>(std::move(model));
  e.motions = bank;
  e.joint_pose = actor_bind_pose(*e.model);
  ActorPlacement placement;
  placement.position = e.position;
  e.model_work =
      calculate_actor_model_work(*e.model, e.joint_pose, {}, placement);
  auto &body = s.enemies[1];
  body.flags = 0x81;
  body.id = 1;
  body.modes[0] = 1;
  body.zombie_work.emplace();
  body.zombie_flags = 0x80000000u;
  body.motion_number = 200;
  body.pose_owner = 0;
  body.linked_parent = 0;
  body.model = e.model;
  body.motions = bank;
  body.position = e.position;
  return s;
}
ZombieMotionEffects dry() {
  return {[](ScenarioState &, size_t, int32_t) {},
          [](ScenarioState &, size_t, int32_t) {}};
}
int main() {
  try {
    auto s = fixture();
    auto hooks = dry();
    auto &e = s.enemies[0];
    for (size_t work : {0u, 1u}) {
      auto protected_pose = fixture();
      const size_t joint = work ? 8 : 0;
      protected_pose.enemies[0].model_work->joints[joint].flags = 2;
      protected_pose.enemies[0].joint_pose[joint].angles =
          std::array<int32_t, 3>{123, 456, 789};
      tick_zombie_motion(protected_pose, work, hooks);
      ck(protected_pose.enemies[0].joint_pose[joint].angles ==
             std::array<int32_t, 3>{123, 456, 789},
         "SetMtn must retain O_WORK protected root/body angles");
    }
    {
      auto skipped_pose = fixture();
      auto bank =
          std::make_shared<MotionBank>(*skipped_pose.enemies[0].motions);
      bank->at(0)->tracks.resize(17);
      skipped_pose.enemies[0].motions = bank;
      skipped_pose.enemies[0].model_work->joints[0].flags = 4;
      tick_zombie_motion(skipped_pose, 0, hooks);
      ck(skipped_pose.enemies[0].joint_pose[0].position->x == 0 &&
             skipped_pose.enemies[0].joint_pose[1].position->x == 5 &&
             skipped_pose.enemies[0].joint_pose[1].position->z == 10,
         "SetMtn O_WORK skip must retain Normal channel cursor");
    }
    e.frame = 65536;
    e.zombie_flags = 0x1000000;
    e.speed = 55;
    ck(tick_zombie_motion(s, 0, hooks) == 0, "Root SetMtn boundary changed");
    ck(e.frame == 2 * 65536 && e.joint_pose[0].position->x == 0 &&
           e.joint_pose[0].position->z == 0 &&
           e.joint_pose[0].position->y == 9 && near(e.position.x, 101) &&
           near(e.position.z, 202) && e.speed == 55,
       "Root channel zeroing and captured-frame displacement failed");
    s = fixture();
    s.enemies[0].frame = 79 * 65536;
    ck(tick_zombie_motion(s, 0, hooks) == 1 && s.enemies[0].frame == 0 &&
           (s.enemies[0].flags & 0x2000000),
       "Loop boundary must set primary2000000");
    tick_zombie_motion(s, 0, hooks);
    ck(!(s.enemies[0].flags & 0x2000000),
       "Non-boundary must clear primary2000000");
    s = fixture();
    s.enemies[0].zombie_flags = 0x10;
    s.enemies[0].flags |= 0x2000000;
    s.enemies[0].pose_owner = 999;
    s.enemies[0].motion_number = 999;
    ck(tick_zombie_motion(s, 0) == 0 && s.enemies[0].frame == 0 &&
           (s.enemies[0].flags & 0x2000000),
       "Frozen source gate must ignore unused owner/motion/effects");
    s = fixture();
    bad([&] { tick_zombie_motion(s, 0); });
    ck(s.enemies[0].frame == 0, "Missing effects dependency mutated motion");
    s.enemies[0].modes[0] = 5;
    tick_zombie_motion(s, 0);
    ck(s.enemies[0].frame == 65536,
       "Mode>=5 must bypass unused effect handlers");
    s = fixture();
    std::vector<int> order;
    s.enemies[0].frame = 17 * 65536;
    s.enemies[0].motion_number = 7;
    s.enemies[0].zombie_work->personality = 0;
    ZombieMotionEffects traced{
        [&](ScenarioState &state, size_t work, int32_t frame) {
          ck(work == 0 && frame == 17 && state.enemies[0].frame == 18 * 65536,
             "Kami must see sampled work and incoming frame");
          order.push_back(1);
          state.random_seed = 2;
        },
        [&](ScenarioState &state, size_t, int32_t frame) {
          ck(frame == 17 && state.random_seed == 2,
             "Water must run after Kami");
          order.push_back(2);
        }};
    tick_zombie_motion(s, 0, traced);
    ck(order == std::vector<int>{1, 2} && s.random_seed == 2207042835u &&
           s.pending_enemy_sounds.size() == 1,
       "Effects must run synchronously before motion-table voice RNG");
    for (uint32_t motion : {0u, 1u, 32u}) {
      s = fixture();
      auto &root = s.enemies[0];
      root.frame = 65536;
      root.motion_number = motion;
      root.zombie_flags = 0x2000000;
      root.zombie_work->target_x = 90;
      root.zombie_work->target_z = 200;
      tick_zombie_motion(s, 0, hooks);
      ck(near(root.speed, std::sqrt(5.f)) &&
             root.joint_pose[0].position->x == 0 &&
             root.joint_pose[0].position->z == 0 &&
             root.angles[1] == (motion == 0 ? 0 : 256),
         "Root speed/turn motion gate failed");
    }
    s = fixture();
    s.enemies[0].frame = 65536;
    s.enemies[0].zombie_flags = 0x3000000;
    tick_zombie_motion(s, 0, hooks);
    ck(s.enemies[0].speed == 0 && near(s.enemies[0].position.x, 101),
       "Root translation must take priority over speed branch");
    s = fixture();
    s.enemies[1].frame = 65536;
    s.enemies[1].zombie_flags |= 0x2000000;
    tick_zombie_motion(s, 1, hooks);
    ck(s.enemies[1].frame == 2 * 65536 && s.enemies[0].frame == 0 &&
           s.enemies[0].joint_pose[0].position->x == 0 &&
           s.enemies[0].joint_pose[8].position->x == 0 &&
           s.enemies[0].joint_pose[8].position->z == 0 &&
           s.enemies[0].joint_pose[8].position->y == 9 &&
           near(s.enemies[1].speed, std::sqrt(5.f)) &&
           near(s.enemies[1].position.z, 200 - std::sqrt(5.f)),
       "Linked body suffix/speed/counters failed");
    s = fixture();
    s.enemies[1].zombie_flags |= 0x1000000;
    tick_zombie_motion(s, 1, hooks);
    ck(s.enemies[0].joint_pose[8].position->x == 5 &&
           s.enemies[1].position.x == 100,
       "Body must ignore root displacement flag");
    for (bool front : {false, true})
      for (bool side : {false, true}) {
        s = fixture();
        s.enemies[0].zombie_flags = 0x8000000 | (front ? 0x2000 : 0);
        s.enemies[0].zombie_secondary_flags = side ? 0x20 : 0;
        tick_zombie_motion(s, 1, hooks);
        auto a = *s.enemies[0].joint_pose[8].angles;
        ck(a[0] == (front ? 2048 : -2048) &&
               a[1] == ((front == side) ? 2048 : -2048) && a[2] == 0 &&
               s.enemies[1].zombie_work->damage_rotation_frame == 1,
           "Original recoil sign table selection failed");
        s.enemies[1].zombie_work->damage_rotation_frame = 7;
        tick_zombie_motion(s, 1, hooks);
        ck(s.enemies[1].zombie_work->damage_rotation_frame == 0 &&
               !(s.enemies[0].zombie_flags & 0x8000000),
           "Eighth recoil sample must clear parent flag/reset byte");
      }
    for (auto sample : std::vector<std::array<int, 3>>{{8, 10, 200},
                                                       {120, 16, 1000},
                                                       {85, 7, 200},
                                                       {121, 11, 1000},
                                                       {0, 41, 200},
                                                       {40, 18, 900},
                                                       {41, 20, 900},
                                                       {125, 39, 0},
                                                       {117, 0, 100}}) {
      s = fixture();
      s.enemies[0].motion_number = sample[0];
      s.enemies[0].frame = sample[1] * 65536;
      s.enemies[0].zombie_flags = 0x10000000;
      tick_zombie_motion(s, 0, hooks);
      ck((s.enemies[0].model_flags & 2) &&
             s.enemies[0].shape_count == sample[2],
         "Original mouth table/captured frame failed");
    }
    s = fixture();
    s.enemies[0].motion_number = 2;
    s.enemies[0].zombie_flags = 0x10000000;
    s.enemies[0].model_flags = 0x42;
    s.enemies[0].shape_count = 99;
    tick_zombie_motion(s, 0, hooks);
    ck(s.enemies[0].model_flags == 0x40 && s.enemies[0].shape_count == 0,
       "Unhandled active mouth motion must clear shape flag/scalar");
    for (bool type10 : {false, true}) {
      s = fixture();
      s.enemies[0].type = type10 ? 10 : 0;
      s.enemies[0].zombie_flags = type10 ? 0x10000000 : 0;
      s.enemies[0].model_flags = 2;
      s.enemies[0].shape_count = 99;
      tick_zombie_motion(s, 0, hooks);
      ck(!(s.enemies[0].zombie_flags & 0x10000000) &&
             s.enemies[0].model_flags == 2 && s.enemies[0].shape_count == 99,
         "Disabled/type10 mouth must preserve stale shape work");
    }
    s = fixture();
    s.enemies[0].modes[0] = 5;
    s.enemies[0].motion_number = 511;
    s.enemies[0].flags |= 0x2000000;
    ck(tick_zombie_motion(s, 0) == 0 && s.enemies[0].frame == 0 &&
           !(s.enemies[0].flags & 0x2000000),
       "Null record must retain counters and clear boundary flag");
    s = fixture();
    s.enemies[0].flags |= 0x40000;
    tick_zombie_motion(s, 0, hooks);
    ck(near(s.enemies[0].position.x, 95) && near(s.enemies[0].position.z, 190),
       "SetMtn must apply original foot callback after pose sampling");
    {
      auto native = fixture();
      native.enemies[0].motion_number = 8;
      native.enemies[0].frame = 28 * 65536;
      tick_zombie_motion(native, 0, native_zombie_motion_effects());
      ck(native.enemies[0].frame == 29 * 65536 && native.effect_pool &&
             native.effect_pool->works[0].id == 5 &&
             native.effect_pool->works[1].id == 302 &&
             native.enemies[0].damage_joint == 11,
         "Native effects must use incoming frame before channel advance");
      auto failed = fixture();
      auto &root = failed.enemies[0];
      root.motion_number = 8;
      root.frame = 28 * 65536;
      root.damage_joint = 6;
      root.damage_velocity = {7, 8, 9};
      auto old_pool = std::make_shared<NativeEffectPool>();
      old_pool->works[0].flags = 1;
      old_pool->works[0].id = 108;
      failed.effect_pool = old_pool;
      const auto old_pose = root.joint_pose;
      auto failure = native_zombie_motion_effects();
      failure.water = [](ScenarioState &state, size_t, int32_t) {
        state.random_seed = 999;
        throw std::runtime_error("Failure after native Kami creation");
      };
      bad([&] { tick_zombie_motion(failed, 0, failure); });
      ck(failed.effect_pool == old_pool && root.damage_joint == 6 &&
             root.damage_velocity.x == 7 && root.damage_velocity.y == 8 &&
             root.damage_velocity.z == 9 && root.frame == 28 * 65536 &&
             failed.random_seed == 1 &&
             root.joint_pose[0].position->x == old_pose[0].position->x,
         "Late SetMtn failure must restore native effects and damage fields");
    }
    for (int invalid = 0; invalid < 4; ++invalid) {
      s = fixture();
      auto before = s.enemies[0].joint_pose;
      if (invalid == 0) {
        s.enemies[0].motion_number = 8;
        s.enemies[0].frame = 71 * 65536;
        s.enemies[0].zombie_flags = 0x10000000;
      }
      if (invalid == 1) {
        s.enemies[0].frame = 65536;
        s.enemies[0].zombie_flags = 0x1000000;
        auto bank = std::make_shared<MotionBank>(*s.enemies[0].motions);
        bank->at(0)->tracks[0].positions.resize(1);
        s.enemies[0].motions = bank;
      }
      if (invalid == 2) {
        s.enemies[0].zombie_flags = 0x8000000;
        s.enemies[1].zombie_work->damage_rotation_frame = 8;
      }
      ZombieMotionEffects failure = hooks;
      if (invalid == 3)
        failure.water = [](ScenarioState &state, size_t work, int32_t) {
          state.random_seed = 999;
          state.pending_enemy_sounds.push_back({work, {}, 42});
          throw std::runtime_error("Effect backend failure");
        };
      const auto work = invalid == 2 ? 1u : 0u;
      const auto frame = s.enemies[work].frame;
      bad([&] { tick_zombie_motion(s, work, failure); });
      ck(s.enemies[work].frame == frame && s.random_seed == 1 &&
             s.pending_enemy_sounds.empty() && s.enemies[0].position.x == 100 &&
             s.enemies[0].joint_pose[0].position->x == before[0].position->x &&
             s.enemies[0].joint_pose[8].angles == before[8].angles,
         "Failed SetMtn partially committed pose/scalars/requests");
    }
    std::cout << "Native SetMtn channels, callbacks, recoil, mouth tables and "
                 "rollback passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
