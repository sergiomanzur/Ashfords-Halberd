#include "weapon.hpp"
#include "capsule.hpp"
#include "scenario.hpp"
#include "weapon_table.hpp"
#include "weapon_tables.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace ashford {

namespace {
struct Reader {
  const Bytes &b;
  void range(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Weapon package range exceeds input");
  }
  uint32_t u32(size_t p) const {
    range(p, 4);
    return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) |
           (uint32_t(b[p + 2]) << 16) | (uint32_t(b[p + 3]) << 24);
  }
  Bytes copy(size_t p, size_t n) const {
    range(p, n);
    return Bytes(b.begin() + p, b.begin() + p + n);
  }
};
} // namespace

WeaponPackage decode_weapon_package(const Bytes &data, size_t key_limit) {
  if (data.size() > 32 * 1024 * 1024)
    throw std::runtime_error("Weapon package exceeds size limit");
  Reader r{data};
  size_t p = 0;

  WeaponPackage package;

  // 1. Right model
  uint32_t msz = r.u32(p);
  p += 4;
  if (msz != 0) {
    package.right_model.model = r.copy(p, msz);
    p += msz;
  }

  // 2. Right textures
  uint32_t tex_sz = r.u32(p);
  if (tex_sz & 0x80000000u) {
    p = (p + 4 + 63) & ~size_t(63);
    tex_sz &= 0x7fffffffu;
  } else {
    p += 4;
  }
  if (tex_sz != 0) {
    package.right_model.textures = r.copy(p, tex_sz);
    p += tex_sz;
  }

  // 3. Left model
  msz = r.u32(p);
  p += 4;
  if (msz != 0) {
    package.left_model.model = r.copy(p, msz);
    p += msz;
  }

  // 4. Left textures
  tex_sz = r.u32(p);
  if (tex_sz & 0x80000000u) {
    p = (p + 4 + 63) & ~size_t(63);
    tex_sz &= 0x7fffffffu;
  } else {
    p += 4;
  }
  if (tex_sz != 0) {
    package.left_model.textures = r.copy(p, tex_sz);
    p += tex_sz;
  }

  // 5. Motions (slots 100+)
  if (p + 4 <= data.size()) {
    uint32_t mtn_total = r.u32(p);
    p += 4;
    if (mtn_total != 0) {
      size_t mtn_end = p + mtn_total;
      if (mtn_end > data.size())
        throw std::runtime_error("Weapon motion block exceeds file size");
      while (p + 4 <= mtn_end) {
        int32_t mtsz = int32_t(r.u32(p));
        if (mtsz == -1) {
          p += 4;
          break;
        }
        p += 4;
        if (mtsz > 0) {
          auto record = r.copy(p, size_t(mtsz));
          p += size_t(mtsz);
          auto motion = decode_motion(record, key_limit);
          for (const auto &track : motion.tracks)
            key_limit -= track.positions.size() + track.angles.size();
          package.motions.push_back(std::move(motion));
        } else {
          package.motions.push_back(std::nullopt);
        }
      }
    }
  }

  return package;
}

GunHitResult check_gun_atari(const GunAttackWork &gap,
                             ScenarioState &scenario) {
  GunHitResult result;
  int32_t wno = gap.wpn_no;
  if (gap.at_flg & 0x100) {
    wno = 6;
  }
  if (wno < 0 || size_t(wno) >= (sizeof(WpnTab) / sizeof(WpnTab[0]))) {
    wno = 2; // Handgun default
  }
  const auto &tab = WpnTab[wno];
  const uint32_t wp_flg = tab.flags;

  Vec3 dir{gap.vx, gap.vy, gap.vz};
  float dlen = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  if (dlen > 1e-6f) {
    dir.x /= dlen;
    dir.y /= dlen;
    dir.z /= dlen;
  } else {
    dir = {0, 0, 1};
  }

  float len = gap.l > 0 ? gap.l : 1000.0f;
  float rn = gap.rn;
  float rmax = gap.rmax;
  if ((gap.at_flg & 4) || (gap.at_flg & 8)) {
    rn *= 2.0f;
    rmax *= 1.2f;
  }

  Capsule3 gatr;
  gatr.start = Vec3{gap.px, gap.py, gap.pz};
  gatr.end = Vec3{gap.px + dir.x * len, gap.py + dir.y * len, gap.pz + dir.z * len};
  gatr.radius = gap.r;

  float best_dist = 0;
  int32_t best_id = -1;
  int32_t best_kno = 1;
  Vec3 best_pos{};

  for (size_t i = 0; i < scenario.enemies.size(); ++i) {
    auto &enemy = scenario.enemies[i];
    if (!(enemy.flags & 1) || (enemy.flags & 2))
      continue;
    if (enemy.state_flags & 0x1000000)
      continue;

    Capsule3 ene_cap;
    bool has_authored_cap = enemy.geometry.capsule_radius > 0.0f &&
                            (enemy.geometry.capsule_start.x != enemy.geometry.capsule_end.x ||
                             enemy.geometry.capsule_start.y != enemy.geometry.capsule_end.y ||
                             enemy.geometry.capsule_start.z != enemy.geometry.capsule_end.z);
    if (has_authored_cap) {
      ene_cap.start = enemy.geometry.capsule_start;
      ene_cap.end = enemy.geometry.capsule_end;
      ene_cap.radius = enemy.geometry.capsule_radius;
    } else {
      float r = enemy.geometry.collision_radius > 0 ? enemy.geometry.collision_radius : 2.5f;
      float h = enemy.geometry.collision_height > 0 ? enemy.geometry.collision_height : 15.0f;
      ene_cap.start = Vec3{enemy.position.x, enemy.position.y + h, enemy.position.z};
      ene_cap.end = Vec3{enemy.position.x, enemy.position.y, enemy.position.z};
      ene_cap.radius = r;
    }

    Vec3 mid{(ene_cap.start.x + ene_cap.end.x) * 0.5f,
             (ene_cap.start.y + ene_cap.end.y) * 0.5f,
             (ene_cap.start.z + ene_cap.end.z) * 0.5f};
    Vec3 proj{};
    distance_point_line(mid, Line3{gatr.start, dir}, &proj);

    Vec3 sca{proj.x - gap.gx, proj.y - gap.gy, proj.z - gap.gz};
    float dist_proj = std::sqrt(sca.x * sca.x + sca.y * sca.y + sca.z * sca.z);
    float spread = rn * dist_proj;
    if (rmax > 0 && spread > rmax) {
      spread = rmax;
    }

    Capsule3 test_gatr = gatr;
    test_gatr.radius = gap.r + spread;

    if (!(wp_flg & 0x20000000)) {
      Vec3 v0{dir.x, 0, dir.z};
      Vec3 v1{proj.x - gap.px, 0, proj.z - gap.pz};
      float v0_len = std::sqrt(v0.x * v0.x + v0.z * v0.z);
      float v1_len = std::sqrt(v1.x * v1.x + v1.z * v1.z);
      if (v0_len > 1e-6f && v1_len > 1e-6f) {
        float dot = (v0.x * v1.x + v0.z * v1.z) / (v0_len * v1_len);
        if (dot < 0.0f)
          continue;
      }
    }

    Vec3 hit_pt{};
    if (!collide_capsules_ex(test_gatr, ene_cap, hit_pt))
      continue;

    Vec3 to_hit = Vec3{gap.gx - hit_pt.x, gap.gy - hit_pt.y, gap.gz - hit_pt.z};
    float dist = std::sqrt(to_hit.x * to_hit.x + to_hit.y * to_hit.y + to_hit.z * to_hit.z);

    if (dist < (len + test_gatr.radius)) {
      if (best_id == -1 || dist < best_dist) {
        best_dist = dist;
        best_id = int32_t(i);
        best_pos = hit_pt;
        best_kno = 1;
        if (enemy.model_work && !enemy.model_work->joints.empty()) {
          float min_jd = 1e9f;
          for (size_t j = 1; j < enemy.model_work->joints.size(); ++j) {
            const auto &mtx = enemy.model_work->joints[j].matrix;
            float jx = hit_pt.x - float(mtx[3]);
            float jy = hit_pt.y - float(mtx[7]);
            float jz = hit_pt.z - float(mtx[11]);
            float jd = jx * jx + jy * jy + jz * jz;
            if (jd < min_jd) {
              min_jd = jd;
              best_kno = int32_t(j);
            }
          }
        }
      }
    }
  }

  if (best_id == -1)
    return result;

  auto &hit_enemy = scenario.enemies[best_id];
  hit_enemy.flags |= 4;
  hit_enemy.damage_position = best_pos;
  hit_enemy.damage_velocity = dir;
  hit_enemy.damage_joint = best_kno;

  uint16_t ene_id = hit_enemy.id;
  if (ene_id >= 40)
    ene_id = 1;

  int32_t dmg = 0;
  if (best_dist < 15.0f) {
    dmg = EneDamNear[ene_id][wno];
    hit_enemy.combo_flags |= 0x10;
  } else if (best_dist < 40.0f) {
    dmg = EneDamMid[ene_id][wno];
    hit_enemy.combo_flags |= 0x20;
  } else {
    dmg = EneDamFar[ene_id][wno];
    hit_enemy.combo_flags |= 0x40;
  }

  if (size_t(best_kno) < hit_enemy.damage.size()) {
    hit_enemy.damage[best_kno] += dmg;
  }

  hit_enemy.health -= dmg;
  if (hit_enemy.health <= 0) {
    hit_enemy.health = 0;
    hit_enemy.flags |= 2;
    hit_enemy.flags &= ~0x28u;
    hit_enemy.modes[0] = 4;
    hit_enemy.state_flags |= 0x1000000;
  }

  result.hit = true;
  result.enemy_index = best_id;
  result.joint_index = best_kno;
  result.damage = dmg;
  result.hit_position = best_pos;
  return result;
}

} // namespace ashford
