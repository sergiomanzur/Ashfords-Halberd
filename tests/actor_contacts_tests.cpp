#include "actor_contacts.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ashford;
void ck(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
}
void near(float a, float b) {
  ck(std::abs(a - b) < .002f,
     "Contact displacement differs from original response");
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed, "Malformed contact did not fail");
}
ContactBody body(float z = 0) {
  ContactBody b;
  b.position.z = z;
  b.radius = 2;
  b.height = 10;
  b.flags = 0x49;
  return b;
}
int main() {
  try {
    auto enemy = body(), player = body(3);
    ck(collide_enemy_player(enemy, player), "Missing pair contact");
    near(enemy.position.z, -.5f);
    near(player.position.z, 3.5f);
    ck(enemy.state_flags == 4 && player.state_flags == 2,
       "Missing pair state flags");
    // Upstream float atan2(1,3)*10430.381 truncates to word3355.
    // Source quarter-table entries yield sin=.316138655, cos=.948682725.
    enemy = body();
    player = body(3);
    player.position.x = 1;
    player.secondary_flags = 1;
    ck(collide_enemy_player(enemy, player), "Missing angled fixed contact");
    ck(std::abs(enemy.position.x - (-.264554620f)) < .000001f &&
           std::abs(enemy.position.z - (-.794730902f)) < .000001f,
       "Angled pair did not retain original angle truncation/table");
    auto angled_player = body();
    std::vector<ContactBody> angled_pool{body(3)};
    angled_pool[0].position.x = 1;
    ck(collide_actor_pool(angled_player, angled_pool) == 1 &&
           std::abs(angled_player.position.x - (-.264554620f)) < .000001f &&
           std::abs(angled_player.position.z - (-.794730902f)) < .000001f,
       "Angled pool did not retain original angle truncation/table");
    for (bool fixed_enemy : {false, true}) {
      enemy = body();
      player = body(3);
      if (fixed_enemy)
        enemy.secondary_flags = 0x40;
      else
        player.secondary_flags = 1;
      ck(collide_enemy_player(enemy, player), "Missing fixed pair contact");
      near(enemy.position.z, -1);
      near(player.position.z, 3);
    }
    enemy = body();
    player = body(100);
    player.flags &= ~8u;
    player.normal_anchor.z = 3;
    ck(collide_enemy_player(enemy, player), "Normal-anchor contact missing");
    near(enemy.position.z, -.5f);
    near(player.position.z, 100.5f);
    enemy = body();
    enemy.position.z = 1;
    enemy.offset.z = -1;
    player = body(4);
    player.offset.z = -1;
    ck(collide_enemy_player(enemy, player), "Offset pair contact missing");
    near(enemy.position.z, .5f);
    near(player.position.z, 4.5f);
    for (uint32_t missing : {8u, 0x40u}) {
      enemy = body();
      player = body(3);
      enemy.flags &= ~missing;
      ck(!collide_enemy_player(enemy, player) && enemy.state_flags == 0,
         "Pair primary gate ignored");
    }
    enemy = body();
    player = body(3);
    enemy.secondary_flags = 1;
    ck(!collide_enemy_player(enemy, player), "Pair secondary gate ignored");
    enemy = body();
    player = body(3);
    enemy.state_flags = 0x40000000;
    ck(!collide_enemy_player(enemy, player), "Pair dead gate ignored");
    enemy = body();
    player = body(4);
    ck(!collide_enemy_player(enemy, player), "Horizontal tangency counted");
    enemy = body();
    player = body(3);
    player.position.y = 10;
    ck(collide_enemy_player(enemy, player),
       "Inclusive vertical boundary missed");
    enemy = body();
    player = body(3);
    player.position.y = 10.001f;
    ck(!collide_enemy_player(enemy, player), "Disjoint heights collided");
    enemy = body();
    player = body();
    ck(collide_enemy_player(enemy, player), "Coincident pair missed");
    near(enemy.position.z, 0);
    near(player.position.z, 0);
    player = body();
    std::vector<ContactBody> pool{body(1), body(-2)};
    ck(collide_actor_pool(player, pool) == 2, "Missing multiple pool contacts");
    near(player.position.z, -.5f);
    ck(pool[0].state_flags == 4 && pool[1].state_flags == 4,
       "Pool player flags missing");
    near(pool[0].position.z, 1);
    near(pool[1].position.z, -2);
    pool = {body(), body(1)};
    ck(collide_actor_pool(pool[0], pool, 0) == 1,
       "Enemy pool self exclusion failed");
    near(pool[0].position.z, -3);
    ck(pool[1].state_flags == 2, "Enemy pool flags missing");
    player = body();
    pool = {body(1)};
    pool[0].flags &= ~0x40u;
    ck(collide_actor_pool(player, pool) == 0,
       "Player-only enemy visibility gate ignored");
    pool = {body(), body(1)};
    pool[1].flags &= ~0x40u;
    ck(collide_actor_pool(pool[0], pool, 0) == 1,
       "Enemy wrongly used player-only gate");
    for (uint32_t primary : {0u, 1u, 8u}) {
      player = body();
      pool = {body(1)};
      pool[0].flags = primary;
      ck(collide_actor_pool(player, pool) == 0,
         "Inactive/nonphysical pool work collided");
    }
    player = body();
    pool = {body(1)};
    pool[0].state_flags = 0x1000000;
    ck(collide_actor_pool(player, pool) == 0, "Hidden pool work collided");
    player = body();
    pool = {body(1), body(50)};
    pool[0].flags |= 0x80;
    pool[0].linked_parent = 1;
    pool[1].state_flags = 0x1000000;
    ck(collide_actor_pool(player, pool) == 0, "Hidden linked parent collided");
    pool[1].state_flags = 0;
    ck(collide_actor_pool(player, pool) == 1,
       "Visible linked parent lost contact");
    player = body();
    player.secondary_flags = 1;
    pool = {body(1)};
    ck(collide_actor_pool(player, pool) == 0, "Subject secondary gate ignored");
    player = body();
    pool = {body(1), body(-2)};
    pool[1].flags |= 0x80;
    bad([&] { collide_actor_pool(player, pool); });
    near(player.position.z, 0);
    ck(pool[0].state_flags == 0,
       "Late invalid parent partially changed pool flags");
    enemy = body();
    player = body(3);
    player.radius = std::numeric_limits<float>::infinity();
    bad([&] { collide_enemy_player(enemy, player); });
    ck(enemy.state_flags == 0 && player.state_flags == 0,
       "Invalid pair partially committed");
    player = body();
    pool = std::vector<ContactBody>(129);
    bad([&] { collide_actor_pool(player, pool); });
    std::cout << "Native actor contact response passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
