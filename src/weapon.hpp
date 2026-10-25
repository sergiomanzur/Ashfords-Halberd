#pragma once
#include "actor.hpp"
#include "weapon_table.hpp"
#include "weapon_tables.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace ashford {
// Native ports of weapon.c::bhSetWeapon and bhObjWpn. They operate on the
// fields the originals touch; the full player/object work is bound elsewhere.
struct WeaponObjectWork {
  uint32_t flags = 0;
  uint32_t id = 0;
  uint32_t type = 0;
  uint32_t floor = 0;
  uint32_t link_kind = 0; // lkono
  int32_t ax = 0, ay = 0, az = 0;
};
struct PlayerWeaponView {
  uint32_t flags = 0;                     // plp->flg
  int32_t equipped = 0;                   // plp->wpnr_no
  int32_t floor = 0;                      // plp->flr_no
  int32_t mode0 = 0, mode1 = 0;           // plp->mode0/mode1
  uint32_t exp0 = 0;                      // *(int *)plp->exp0
  std::array<uint32_t, 14> joint_flags{}; // plp->mlwP->owP[n].flg
};
inline void set_weapon(WeaponObjectWork &op, PlayerWeaponView &player,
                       int weapon_number, int flag) {
  op = WeaponObjectWork{};
  op.flags = 0x81;
  op.id = 1210;
  op.type = uint32_t(weapon_number) & 0xff;
  op.floor = uint32_t(player.floor);
  if (weapon_number != 0)
    op.flags |= 0x40000;
  else
    op.flags &= ~0x40000u;
  op.link_kind = flag == 0 ? 9 : 13;
  player.exp0 = player.equipped < 10 ? 0 : 1;
  auto clear = [&](size_t joint) { player.joint_flags[joint] &= ~0x2u; };
  if (player.equipped > 1) {
    player.flags |= 0x20000;
    clear(7);
    clear(8);
    clear(9);
  } else {
    player.flags &= ~0x20000u;
    if (player.equipped != 1) {
      clear(7);
      clear(8);
      clear(9);
    }
  }
  clear(11);
  clear(12);
  clear(13);
}
inline void object_weapon_angles(WeaponObjectWork &op,
                                 const PlayerWeaponView &player) {
  if (op.link_kind != 9)
    return;
  if (op.type >= 21)
    throw std::out_of_range("Weapon type outside original angle table");
  if (player.mode1 == 0 || player.mode0 == 7) {
    op.ax = WpnAngTab[op.type][0];
    op.ay = WpnAngTab[op.type][1];
    op.az = WpnAngTab[op.type][2];
  } else {
    op.ax = op.ay = op.az = 0;
  }
}
// weapon.c::bhCheckBullet / bhCountBullet. `pip` is swork.pip: element 0's low
// 16 bits select the equipped slot; each slot holds ammo in its low 16 bits
// and an infinite-ammo flag at bit 27. `attack_flags` is plp->at_flg and
// `game_flags` is sys->gm_flg (bit 0x40000 is raised when ammo runs out).
inline size_t bullet_slot(const std::vector<uint32_t> &pip) {
  if (pip.empty())
    throw std::runtime_error("Missing ammunition work");
  size_t slot = uint16_t(pip[0]);
  if (slot >= pip.size())
    throw std::out_of_range("Ammunition slot outside work");
  return slot;
}
inline int16_t check_bullet(const std::vector<uint32_t> &pip,
                            uint32_t attack_flags) {
  const auto word = pip[bullet_slot(pip)];
  if (((word >> 16) & 0x800) || (attack_flags & 0x40000000u))
    return -1;
  return int16_t(word & 0xffff);
}
inline bool count_bullet(std::vector<uint32_t> &pip, uint32_t attack_flags,
                         uint32_t &game_flags) {
  auto &word = pip[bullet_slot(pip)];
  if (((word >> 16) & 0x800) || (attack_flags & 0x40000000u))
    return true;
  int16_t rounds = int16_t(word & 0xffff);
  if (rounds < 1)
    return false;
  --rounds;
  if (rounds < 1)
    game_flags |= 0x40000;
  word = (word & 0xffff0000u) | (uint32_t(rounds) & 0xffffu);
  return true;
}
struct WeaponPackage {
  ActorModel right_model;
  ActorModel left_model;
  std::vector<std::optional<Motion>> motions;
};
// Decodes a weapon package buffer (from SYSTEM.AFS entry 20 + wpn_no)
// following dread.c::bhReadWeaponData format: right model, right textures,
// left model, left textures, then motion bank (slots 100+).
WeaponPackage decode_weapon_package(const Bytes &data, size_t key_limit = 2000000);

struct GunAttackWork {
  float px = 0, py = 0, pz = 0;
  float vx = 0, vy = 0, vz = 0;
  float gx = 0, gy = 0, gz = 0;
  uint32_t at_flg = 0;
  int32_t wpn_no = 0;
  float r = 0, l = 0, rn = 0, rmax = 0;
  int32_t ax = 0, ay = 0;

  Vec3 player_pos() const { return {px, py, pz}; }
  Vec3 dir() const { return {vx, vy, vz}; }
  Vec3 gun_pos() const { return {gx, gy, gz}; }
  void set_player_pos(const Vec3 &p) { px = p.x; py = p.y; pz = p.z; }
  void set_dir(const Vec3 &d) { vx = d.x; vy = d.y; vz = d.z; }
  void set_gun_pos(const Vec3 &g) { gx = g.x; gy = g.y; gz = g.z; }
};

struct GunHitResult {
  bool hit = false;
  int32_t enemy_index = -1;
  int32_t joint_index = -1;
  int32_t damage = 0;
  Vec3 hit_position{};
};

// weapon.c::bhCheckGunAtari core logic: checks gun ray against enemy capsules,
// selects closest target, applies distance-band damage (Near/Mid/Far) from tables.
GunHitResult check_gun_atari(const GunAttackWork &gap,
                             ScenarioState &scenario);

} // namespace ashford
