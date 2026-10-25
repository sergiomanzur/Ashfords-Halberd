#include "weapon.hpp"
#include "scenario.hpp"
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace ashford;
namespace {
void ck(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
// Extract every integer inside the braces of "<name>[..][..] =" in upstream.
std::vector<long> upstream_numbers(const std::string &text,
                                   const std::string &name) {
  auto at = text.find(name + "[");
  ck(at != std::string::npos, "Upstream table missing");
  auto open = text.find('{', text.find('=', at));
  int depth = 0;
  size_t end = open;
  for (; end < text.size(); ++end) {
    if (text[end] == '{')
      ++depth;
    else if (text[end] == '}' && --depth == 0)
      break;
  }
  ck(depth == 0, "Upstream table unterminated");
  std::vector<long> numbers;
  for (size_t i = open; i < end;) {
    if (std::isdigit((unsigned char)text[i]) ||
        (text[i] == '-' && std::isdigit((unsigned char)text[i + 1]))) {
      char *stop = nullptr;
      numbers.push_back(std::strtol(text.c_str() + i, &stop, 10));
      i = size_t(stop - text.c_str());
    } else if (text[i] == '/' && text[i + 1] == '/') {
      while (i < end && text[i] != '\n')
        ++i;
    } else
      ++i;
  }
  return numbers;
}
template <class T, size_t R, size_t C>
void same(const T (&table)[R][C], const std::vector<long> &up, const char *m) {
  ck(up.size() == R * C, m);
  for (size_t r = 0; r < R; ++r)
    for (size_t c = 0; c < C; ++c)
      ck(table[r][c] == up[r * C + c], m);
}
void tables(const std::string &text) {
  same(EneDamNear, upstream_numbers(text, "EneDamNear"), "Near table differs");
  same(EneDamMid, upstream_numbers(text, "EneDamMid"), "Mid table differs");
  same(EneDamFar, upstream_numbers(text, "EneDamFar"), "Far table differs");
  same(WpnAngTab, upstream_numbers(text, "WpnAngTab"), "Angle table differs");
}
static_assert(sizeof(WeaponTableEntry) == 0x68, "WPN_TAB size");
static_assert(offsetof(WeaponTableEntry, radius) == 0x18, "WPN_TAB r offset");
static_assert(offsetof(WeaponTableEntry, fire_position1) == 0x28, "fps1");
static_assert(offsetof(WeaponTableEntry, light_type) == 0x4c, "ltp");
static_assert(offsetof(WeaponTableEntry, hit_rate) == 0x54, "hrate");
static_assert(offsetof(WeaponTableEntry, sound_weapon) == 0x5c, "snd_wpno");
static_assert(offsetof(WeaponTableEntry, vibration_type) == 0x64, "vib_tp");
std::vector<double> flatten(const WeaponTableEntry &e) {
  return {double(e.flags),
          double(e.attack_count),
          double(e.defend_count),
          double(e.attack_cycle_count),
          double(e.effect_y_count),
          double(e.action_count0),
          double(e.action_count1),
          double(e.action_count2),
          double(e.radius),
          double(e.length),
          double(e.range_rate),
          double(e.range_max),
          double(e.fire_position1.x),
          double(e.fire_position1.y),
          double(e.fire_position1.z),
          double(e.fire_position2.x),
          double(e.fire_position2.y),
          double(e.fire_position2.z),
          double(e.capsule_position.x),
          double(e.capsule_position.y),
          double(e.capsule_position.z),
          double(e.light_type),
          double(e.light_r),
          double(e.light_g),
          double(e.light_b),
          double(e.light_near),
          double(e.light_far),
          double(e.hit_rate),
          double(e.effect_scale),
          double(e.sound_weapon),
          double(e.hit_effect),
          double(e.sound0),
          double(e.sound1),
          double(e.vibration_type),
          double(e.vibration_count),
          double(e.reserve1),
          double(e.reserve2)};
}
void weapon_table(const std::string &player_text) {
  auto at = player_text.find("WpnTab[23]");
  ck(at != std::string::npos, "Upstream WpnTab missing");
  auto open = player_text.find('{', player_text.find('=', at));
  int depth = 0;
  size_t end = open;
  for (; end < player_text.size(); ++end) {
    if (player_text[end] == '{')
      ++depth;
    else if (player_text[end] == '}' && --depth == 0)
      break;
  }
  ck(depth == 0, "Upstream WpnTab unterminated");
  std::vector<double> numbers;
  for (size_t i = open; i < end;) {
    const char c = player_text[i];
    if (std::isdigit((unsigned char)c) ||
        (c == '-' && std::isdigit((unsigned char)player_text[i + 1]))) {
      char *stop = nullptr;
      numbers.push_back(std::strtod(player_text.c_str() + i, &stop));
      i = size_t(stop - player_text.c_str());
    } else
      ++i;
  }
  constexpr size_t fields = 37;
  ck(numbers.size() == 23 * fields, "WpnTab upstream number count");
  for (size_t entry = 0; entry < 23; ++entry) {
    const auto native = flatten(WpnTab[entry]);
    ck(native.size() == fields, "WpnTab field count");
    for (size_t field = 0; field < fields; ++field)
      ck(float(native[field]) == float(numbers[entry * fields + field]),
         "WpnTab value differs from upstream");
  }
  // Spot checks of values consumed by hit detection.
  ck(WpnTab[2].flags == 1140850944u && WpnTab[2].radius == 1.0f,
     "Weapon 2 data");
  ck(WpnTab[3].flags == 268451974u && WpnTab[3].length == 200.0f &&
         WpnTab[3].range_max == 4.0f,
     "Weapon 3 data");
}
void set_weapon_flags() {
  PlayerWeaponView p;
  p.floor = 3;
  p.joint_flags.fill(0xffffffff);
  p.flags = 0x5;
  WeaponObjectWork op;
  op.ax = 99; // must be cleared like npSetMemory
  // Unarmed (wpnr_no 0): clears 20000, hides 7-9 and 11-13, exp0=0.
  p.equipped = 0;
  set_weapon(op, p, 0, 0);
  ck(op.flags == 0x81 && op.id == 1210 && op.type == 0 && op.floor == 3 &&
         op.link_kind == 9 && op.ax == 0,
     "Unarmed object setup");
  ck(!(p.flags & 0x20000) && p.exp0 == 0, "Unarmed player flags");
  for (size_t j : {7, 8, 9, 11, 12, 13})
    ck(!(p.joint_flags[j] & 2), "Unarmed joint hidden");
  ck((p.joint_flags[0] & 2) && (p.joint_flags[10] & 2), "Other joints kept");
  // wpnr_no 1 keeps joints 7-9 visible, clears 20000.
  p.joint_flags.fill(0xffffffff);
  p.equipped = 1;
  set_weapon(op, p, 1, 1);
  ck(op.flags == 0x40081 && op.type == 1 && op.link_kind == 13, "Weapon 1");
  ck(!(p.flags & 0x20000) && (p.joint_flags[7] & 2), "Weapon 1 joints");
  ck(!(p.joint_flags[11] & 2), "Weapon 1 hides 11");
  // wpnr_no 2: sets 20000 and hides 7-9; wpnr_no >= 10 sets exp0.
  p.joint_flags.fill(0xffffffff);
  p.equipped = 10;
  set_weapon(op, p, 0x1ff, 0);
  ck(op.type == 0xff && (p.flags & 0x20000) && p.exp0 == 1 &&
         !(p.joint_flags[8] & 2),
     "Weapon 10");
}
void angles() {
  PlayerWeaponView p;
  WeaponObjectWork op;
  op.link_kind = 9;
  op.type = 10;
  p.mode1 = 0;
  object_weapon_angles(op, p);
  ck(op.ax == 4551 && op.ay == 1092 && op.az == 0, "Type 10 angles");
  op.type = 14;
  p.mode1 = 1;
  object_weapon_angles(op, p);
  ck(op.ax == 0 && op.ay == 0 && op.az == 0, "Aimed zero angles");
  p.mode0 = 7;
  object_weapon_angles(op, p);
  ck(op.ay == 60439 && op.az == 546, "mode0 7 uses table");
  op.link_kind = 13;
  op.ax = 5;
  object_weapon_angles(op, p);
  ck(op.ax == 5, "Non-9 link kind untouched");
  op.link_kind = 9;
  op.type = 21;
  bool rejected = false;
  try {
    object_weapon_angles(op, p);
  } catch (const std::out_of_range &) {
    rejected = true;
  }
  ck(rejected, "Out-of-range weapon type must reject");
}
void bullets() {
  // Slot 2 selected, 2 rounds.
  std::vector<uint32_t> pip{2, 0, 2};
  uint32_t game = 0x1;
  ck(check_bullet(pip, 0) == 2, "Check reports rounds");
  ck(count_bullet(pip, 0, game) && (pip[2] & 0xffff) == 1 && game == 0x1,
     "First shot leaves one round");
  ck(count_bullet(pip, 0, game) && (pip[2] & 0xffff) == 0 && game == 0x40001,
     "Last round raises empty flag");
  ck(!count_bullet(pip, 0, game) && (pip[2] & 0xffff) == 0,
     "Empty weapon cannot fire");
  // Upper half is preserved while counting.
  pip[2] = 0x12340003u;
  game = 0;
  ck(count_bullet(pip, 0, game) && pip[2] == 0x12340002u, "Upper bits kept");
  // Infinite flag (bit 27) never decrements and reports -1.
  pip[2] = 0x08000005u;
  game = 0;
  ck(check_bullet(pip, 0) == -1 && count_bullet(pip, 0, game) &&
         pip[2] == 0x08000005u && game == 0,
     "Infinite ammo");
  // Attack-flag 0x40000000 behaves as infinite.
  pip[2] = 4;
  ck(check_bullet(pip, 0x40000000u) == -1 &&
         count_bullet(pip, 0x40000000u, game) && pip[2] == 4,
     "Attack flag infinite");
  // A negative short count cannot fire.
  pip[2] = 0xffffu;
  ck(check_bullet(pip, 0) == -1, "Negative rounds read as -1");
  ck(!count_bullet(pip, 0, game), "Negative rounds cannot fire");
  // Malformed work rejects.
  auto bad = [&](std::vector<uint32_t> work) {
    bool failed = false;
    try {
      check_bullet(work, 0);
    } catch (const std::exception &) {
      failed = true;
    }
    ck(failed, "Malformed ammunition work must reject");
  };
  bad({});
  bad({5, 1});
  bad({1});
}
void test_decode_weapon_package() {
  // Construct a minimal valid weapon package buffer:
  // Right model: 4 bytes len (0x8) + 8 bytes data
  // Right tex: 4 bytes len (0x4) + 4 bytes data
  // Left model: 4 bytes len (0x0)
  // Left tex: 4 bytes len (0x0)
  // Motion block: 4 bytes len (0x8) + -1 (terminator 4 bytes) + 4 bytes padding
  Bytes buffer;
  auto append32 = [&](uint32_t val) {
    buffer.push_back(uint8_t(val & 0xff));
    buffer.push_back(uint8_t((val >> 8) & 0xff));
    buffer.push_back(uint8_t((val >> 16) & 0xff));
    buffer.push_back(uint8_t((val >> 24) & 0xff));
  };
  // Right model: len 8
  append32(8);
  for (int i = 0; i < 8; ++i) buffer.push_back(uint8_t(0x10 + i));
  // Right tex: len 4
  append32(4);
  for (int i = 0; i < 4; ++i) buffer.push_back(uint8_t(0x20 + i));
  // Left model: len 0
  append32(0);
  // Left tex: len 0
  append32(0);
  // Motions: total 4 bytes containing terminator -1
  append32(4);
  append32(0xffffffffu);

  auto pkg = decode_weapon_package(buffer);
  ck(pkg.right_model.model.size() == 8, "Right model size matches");
  ck(pkg.right_model.textures.size() == 4, "Right textures size matches");
  ck(pkg.left_model.model.empty(), "Left model is empty");
  ck(pkg.left_model.textures.empty(), "Left textures is empty");
  ck(pkg.motions.empty(), "Motions list is empty");
}
void test_check_gun_atari() {
  // 1. Near range hit
  {
    ScenarioState s;
    s.enemies.resize(1);
    auto &z = s.enemies[0];
    z.flags = 1;
    z.id = 1; // Zombie
    z.health = 100;
    z.position = {0, 0, 10.0f};
    z.geometry.capsule_start = {0, 15.0f, 10.0f};
    z.geometry.capsule_end = {0, 0.0f, 10.0f};
    z.geometry.capsule_radius = 2.5f;

    GunAttackWork gap;
    gap.set_player_pos({0, 0, 0});
    gap.set_gun_pos({0, 10.0f, 0});
    gap.set_dir({0, 0, 1.0f});
    gap.wpn_no = 2; // Handgun
    gap.r = 0.5f;
    gap.l = 100.0f;

    auto res = check_gun_atari(gap, s);
    ck(res.hit, "Near hit detected");
    ck(res.enemy_index == 0, "Hit correct enemy index");
    ck(res.damage == 10, "Near handgun damage against zombie is 10");
    ck(z.health == 90, "Zombie health reduced to 90");
    ck(z.combo_flags & 0x10, "Near combo flag set");
    ck(z.flags & 4, "Hit damage flag set");
    ck(!(z.flags & 2), "Not yet defeated");
  }

  // 2. Mid range hit (using weapon 11: Near=70, Mid=30, Far=10)
  {
    ScenarioState s;
    s.enemies.resize(1);
    auto &z = s.enemies[0];
    z.flags = 1;
    z.id = 1;
    z.health = 100;
    z.position = {0, 0, 25.0f};
    z.geometry.capsule_start = {0, 15.0f, 25.0f};
    z.geometry.capsule_end = {0, 0.0f, 25.0f};
    z.geometry.capsule_radius = 2.5f;

    GunAttackWork gap;
    gap.set_player_pos({0, 0, 0});
    gap.set_gun_pos({0, 10.0f, 0});
    gap.set_dir({0, 0, 1.0f});
    gap.wpn_no = 11;
    gap.r = 0.5f;
    gap.l = 100.0f;

    auto res = check_gun_atari(gap, s);
    ck(res.hit, "Mid hit detected");
    ck(res.damage == 30, "Mid wpn 11 damage against zombie is 30");
    ck(z.health == 70, "Zombie health reduced to 70");
    ck(z.combo_flags & 0x20, "Mid combo flag set");
  }

  // 3. Far range hit (using weapon 11)
  {
    ScenarioState s;
    s.enemies.resize(1);
    auto &z = s.enemies[0];
    z.flags = 1;
    z.id = 1;
    z.health = 100;
    z.position = {0, 0, 50.0f};
    z.geometry.capsule_start = {0, 15.0f, 50.0f};
    z.geometry.capsule_end = {0, 0.0f, 50.0f};
    z.geometry.capsule_radius = 2.5f;

    GunAttackWork gap;
    gap.set_player_pos({0, 0, 0});
    gap.set_gun_pos({0, 10.0f, 0});
    gap.set_dir({0, 0, 1.0f});
    gap.wpn_no = 11;
    gap.r = 0.5f;
    gap.l = 100.0f;

    auto res = check_gun_atari(gap, s);
    ck(res.hit, "Far hit detected");
    ck(res.damage == 10, "Far wpn 11 damage against zombie is 10");
    ck(z.health == 90, "Zombie health reduced to 90");
    ck(z.combo_flags & 0x40, "Far combo flag set");
  }

  // 4. Defeat
  {
    ScenarioState s;
    s.enemies.resize(1);
    auto &z = s.enemies[0];
    z.flags = 1;
    z.id = 1;
    z.health = 10;
    z.position = {0, 0, 10.0f};
    z.geometry.capsule_start = {0, 15.0f, 10.0f};
    z.geometry.capsule_end = {0, 0.0f, 10.0f};
    z.geometry.capsule_radius = 2.5f;

    GunAttackWork gap;
    gap.set_player_pos({0, 0, 0});
    gap.set_gun_pos({0, 10.0f, 0});
    gap.set_dir({0, 0, 1.0f});
    gap.wpn_no = 2;
    gap.r = 0.5f;
    gap.l = 100.0f;

    auto res = check_gun_atari(gap, s);
    ck(res.hit, "Defeat hit detected");
    ck(z.health == 0, "Zombie health reduced to 0");
    ck(z.flags & 2, "Defeated flag set");
    ck(z.modes[0] == 4, "Zombie mode set to dead (4)");
  }

  // 5. Miss (behind shooter)
  {
    ScenarioState s;
    s.enemies.resize(1);
    auto &z = s.enemies[0];
    z.flags = 1;
    z.id = 1;
    z.health = 100;
    z.position = {0, 0, 10.0f};
    z.geometry.capsule_start = {0, 15.0f, 10.0f};
    z.geometry.capsule_end = {0, 0.0f, 10.0f};
    z.geometry.capsule_radius = 2.5f;

    GunAttackWork gap;
    gap.set_player_pos({0, 0, 0});
    gap.set_gun_pos({0, 10.0f, 0});
    gap.set_dir({0, 0, -1.0f}); // firing away
    gap.wpn_no = 2;
    gap.r = 0.5f;
    gap.l = 100.0f;

    auto res = check_gun_atari(gap, s);
    ck(!res.hit, "Miss correctly reported");
    ck(z.health == 100, "Health untouched");
  }

  // 6. Inactive / defeated enemy not hit
  {
    ScenarioState s;
    s.enemies.resize(2);
    s.enemies[0].flags = 3; // active (1) + dead (2)
    s.enemies[0].id = 1;
    s.enemies[0].position = {0, 0, 10.0f};
    s.enemies[1].flags = 1; // active alive
    s.enemies[1].id = 1;
    s.enemies[1].position = {0, 0, 20.0f};

    GunAttackWork gap;
    gap.set_player_pos({0, 0, 0});
    gap.set_gun_pos({0, 10.0f, 0});
    gap.set_dir({0, 0, 1.0f});
    gap.wpn_no = 2;
    gap.r = 0.5f;
    gap.l = 100.0f;

    auto res = check_gun_atari(gap, s);
    ck(res.hit, "Hit second enemy");
    ck(res.enemy_index == 1, "Skipped dead enemy and hit alive enemy");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2) {
      std::cerr << "usage: weapon-tests <upstream weapon.c>\n";
      return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    ck(bool(in), "Cannot open upstream weapon.c");
    std::stringstream buffer;
    buffer << in.rdbuf();
    tables(buffer.str());
    std::string player_path = argv[1];
    auto slash = player_path.find_last_of("/\\");
    ck(slash != std::string::npos, "Unexpected upstream path");
    player_path = player_path.substr(0, slash + 1) + "player.c";
    std::ifstream player_in(player_path, std::ios::binary);
    ck(bool(player_in), "Cannot open upstream player.c");
    std::stringstream player_buffer;
    player_buffer << player_in.rdbuf();
    weapon_table(player_buffer.str());
    set_weapon_flags();
    angles();
    bullets();
    test_decode_weapon_package();
    test_check_gun_atari();
    std::cout << "weapon tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
