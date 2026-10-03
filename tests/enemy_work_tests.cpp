#include "room.hpp"
#include "scenario.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool value) {
  if (!value)
    throw std::runtime_error("Enemy work check failed");
}
template <class F> void bad(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  ck(rejected);
}
int main() {
  try {
    RoomCollision collision;
    std::vector<ScenarioEntity> pool(128);
    pool[0].flags = 1;
    pool[1].health = 999;
    pool[1].linked_parent = 0;
    pool[1].model = std::make_shared<ActorModel>();
    RoomEntity record;
    record.flags = 1;
    record.id = 67;
    record.type = 18;
    record.floor = 8;
    record.model_version = 3;
    record.position = {10, 0, -4};
    record.rotation_x = 123;
    record.rotation_y = -128;
    record.rotation_z = 456;
    auto slot = allocate_enemy_work(pool, record, collision);
    ck(slot && *slot == 1);
    const auto &work = pool[1];
    ck(work.flags == 0x8001 && work.model_flags == 0x20 && work.id == 67 &&
       work.type == 18 && work.model_version == 3 && work.floor == 0 &&
       work.angles == std::array<int32_t, 3>{0, -128, 0} &&
       work.position.x == 10 && work.position.z == -4 && work.health == 0 &&
       !work.model && !work.linked_parent && work.motion_step == 65536 &&
       work.clip_joints == std::array<int8_t, 8>{1, -1, 0, 0, 0, 0, 0, 0});
    record.id = 41;
    ck(allocate_enemy_work(pool, record, collision) == 2);
    ck(pool[2].clip_joints ==
       std::array<int8_t, 8>{1, -1, 5, 8, 12, 15, 19, -1});
    record.id = 91;
    ck(allocate_enemy_work(pool, record, collision) == 3);
    ck(pool[3].model_flags == 0x120 &&
       pool[3].clip_joints ==
           std::array<int8_t, 8>{1, -1, 5, 17, 21, 24, 28, -1});
    record.position.x = std::numeric_limits<float>::infinity();
    bad([&] { allocate_enemy_work(pool, record, collision); });
    ck(pool[4].flags == 0);
    record.position.x = 0;
    auto invalid_ground = collision;
    invalid_ground.ground[2] = std::numeric_limits<float>::quiet_NaN();
    bad([&] { allocate_enemy_work(pool, record, invalid_ground); });
    ck(pool[4].flags == 0);
    for (auto &entity : pool)
      entity.flags |= 1;
    ck(!allocate_enemy_work(pool, record, collision));
    ck(pool[3].id == 91);
    pool.resize(127);
    bad([&] { allocate_enemy_work(pool, record, collision); });

    ScenarioState state;
    state.collision = collision;
    state.enemies.resize(128);
    auto &parent = state.enemies[0];
    parent.flags = 1;
    parent.id = 1;
    parent.type = 7;
    parent.model_version = 35;
    parent.model = std::make_shared<ActorModel>();
    parent.model_bank = std::make_shared<ActorModelBank>();
    parent.motions = std::make_shared<MotionBank>(512);
    parent.position = {8, 0, 9};
    auto linked = link_zombie_enemy(state, 0, 11, 34);
    ck(linked && *linked == 1);
    const auto &child = state.enemies[1];
    ck(child.flags == 0x81 && child.id == 34 && child.model_flags == 0x21 &&
       child.linked_parent == 0 && child.linked_joint == 11 &&
       child.linked_offset.x == 0 && child.linked_offset.y == 0 &&
       child.linked_offset.z == 0 && child.position.x == 0 && child.type == 0 &&
       child.model_version == 0 && child.model == parent.model &&
       child.model_bank == parent.model_bank &&
       child.motions == parent.motions);
    auto copied = state;
    ck(copied.enemies[1].linked_parent == 0 &&
       copied.enemies[1].model == copied.enemies[0].model);
    state.enemies[0].model.reset();
    ck(bool(copied.enemies[1].model));
    bad([&] { link_zombie_enemy(state, 128, 0, 32); });
    bad([&] { link_zombie_enemy(copied, 0, -1, 32); });
    bad([&] { link_zombie_enemy(copied, 0, 64, 32); });
    copied.collision.reset();
    bad([&] { link_zombie_enemy(copied, 0, 0, 32); });
    ck(copied.enemies[2].flags == 0);
    copied.collision = collision;
    for (auto &entity : copied.enemies)
      entity.flags |= 1;
    ck(!link_zombie_enemy(copied, 0, 0, 32));
    std::cout << "Enemy allocation and linked ownership passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
