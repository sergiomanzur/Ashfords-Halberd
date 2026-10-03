#include "actor.hpp"
#include "motion.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace ashford;
void ck(bool v) {
  if (!v)
    throw std::runtime_error("Motion check failed");
}
template <class F> void bad(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  ck(failed);
}
void put(Bytes &b, size_t p, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (i * 8));
}
void half(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
void flt(Bytes &b, size_t p, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, 4);
  put(b, p, bits);
}
Bytes fixture() {
  Bytes b(80);
  b[0] = 'M';
  b[1] = 'T';
  b[2] = 'N';
  b[3] = 0x80;
  half(b, 4, 16);
  half(b, 6, 1);
  put(b, 8, 0);
  put(b, 12, 60);
  put(b, 16, 12);
  put(b, 20, 2);
  half(b, 24, 3);
  half(b, 26, 2);
  put(b, 28, 20);
  put(b, 32, 44);
  flt(b, 36, 2);
  flt(b, 40, 4);
  flt(b, 44, 6);
  flt(b, 48, 6);
  flt(b, 52, 8);
  flt(b, 56, 10);
  half(b, 60, 65530);
  half(b, 62, 0);
  half(b, 64, 0);
  half(b, 66, 6);
  half(b, 68, 100);
  half(b, 70, 0);
  half(b, 76, 0x1234);
  half(b, 78, 0x5678);
  return b;
}
int main() {
  try {
    auto b = fixture();
    auto m = decode_motion(b);
    // Remapping must preserve source order and mirror only X position/YZ angle.
    Motion channels;
    channels.flags = 0x10;
    channels.frames = 1;
    channels.tracks.resize(2);
    channels.tracks[0].positions = {{2, 4, 6}};
    channels.tracks[0].angles = {{{100, 200, 300}}};
    channels.tracks[1].positions = {{8, 10, 12}};
    channels.tracks[1].angles = {{{400, 500, 600}}};
    std::vector<JointPose> joints(5);
    for (auto &joint : joints) {
      joint.position = Vec3{20, 30, 40};
      joint.angles = std::array<uint16_t, 3>{10, 20, 30};
    }
    MotionApplicationOptions mapping;
    mapping.mode = 2;
    mapping.joint_base = 2;
    mapping.mirror_map = {1, 0, -1};
    auto applied = apply_motion_pose(joints, channels, 0, mapping);
    ck(applied.pose[0].position->x == 20 && applied.pose[4].position->x == 20);
    ck(applied.pose[3].position->x == -2 && applied.pose[3].position->y == 4 &&
       *applied.pose[3].angles ==
           std::array<uint16_t, 3>{100, uint16_t(-200), uint16_t(-300)});
    ck(applied.pose[2].position->x == -8 &&
       (*applied.pose[2].angles)[0] == 400);
    ck(joints[2].position->x == 20 && channels.tracks[0].positions[0].x == 2);
    auto interpolated = channels;
    interpolated.frames = 2;
    for (auto &track : interpolated.tracks) {
      track.positions.push_back({6, 8, 10});
      track.angles.push_back({200, 600, 1000});
    }
    mapping.mode = 2 | 8; // Force Slow despite the motion's Normal flag.
    applied = apply_motion_pose(joints, interpolated, 32768, mapping);
    ck(applied.pose[3].position->x == -4 &&
       *applied.pose[3].angles == std::array<uint16_t, 3>{150, uint16_t(-400), uint16_t(-650)});
    mapping = {};
    mapping.joint_flags = {4, 2, 0};
    applied = apply_motion_pose(joints, channels, 0, mapping);
    // Skip joint zero without consuming a track; joint one preserves angles.
    ck(applied.pose[0].position->x == 20 && applied.pose[1].position->x == 2 &&
       *applied.pose[1].angles == *joints[1].angles &&
       applied.pose[2].position->x == 8);
    mapping.blend_rate = .5f;
    applied = apply_motion_pose(joints, channels, 0, mapping);
    // NormalHokan consumes track zero even though destination zero is skipped.
    ck(applied.pose[1].position->x == 14 && applied.pose[2].position->x == 20);
    channels.flags = 0x20;
    applied = apply_motion_pose(joints, channels, 0, mapping);
    ck(applied.pose[1].position->x == 11 && applied.pose[2].position->x == 14);
    channels.flags = 0;
    mapping.blend_rate.reset();
    applied = apply_motion_pose(joints, channels, 0, mapping);
    // Fast root translation precedes the flag-4 test; later translations
    // ignored.
    ck(applied.pose[0].position->x == 2 && applied.pose[1].position->x == 20 &&
       (*applied.pose[2].angles)[0] == 400);
    mapping.blend_rate = .5f;
    applied = apply_motion_pose(joints, channels, 0, mapping);
    ck(applied.pose[0].position->x == 11);
    mapping.mode = 0x100;
    ck(apply_motion_pose(joints, channels, 0, mapping).pose[0].position->x ==
       2);
    mapping = {};
    auto absent_channels = channels;
    absent_channels.tracks[1].angles.clear();
    for (uint8_t format : {uint8_t(0x10), uint8_t(0x20)}) {
      absent_channels.flags = format;
      applied = apply_motion_pose(joints, absent_channels, 0, mapping);
      ck(*applied.pose[1].angles == std::array<uint16_t, 3>{0, 20, 30});
    }
    absent_channels.flags = 0;
    ck(*apply_motion_pose(joints, absent_channels, 0).pose[1].angles ==
       std::array<uint16_t, 3>{});
    absent_channels.flags = 0x20;
    mapping.blend_rate = 1.f;
    applied = apply_motion_pose(joints, absent_channels, 0, mapping);
    ck(*applied.pose[1].angles == std::array<uint16_t, 3>{0, 200, 300} &&
       applied.angle_scratch == std::array<uint16_t, 3>{0, 200, 300});
    // SlowHokan scratch survives separate calls and skips angle-disabled work.
    Motion missing;
    missing.frames = 1;
    missing.flags = 0x20;
    missing.tracks.resize(1);
    applied =
        apply_motion_pose(joints, missing, 0, mapping, applied.angle_scratch);
    ck(*applied.pose[0].angles == std::array<uint16_t, 3>{0, 200, 300});
    mapping.joint_flags = {2};
    applied = apply_motion_pose(joints, channels, 0, mapping, {7, 8, 9});
    ck(*applied.pose[0].angles == *joints[0].angles &&
       applied.angle_scratch == std::array<uint16_t, 3>{400, 500, 600});
    mapping.joint_flags = {};
    missing.flags = 0x10;
    applied = apply_motion_pose(joints, missing, 0, mapping, {7, 8, 9});
    ck(applied.angle_scratch == std::array<uint16_t, 3>{});
    // Work flag 8 affects only FastHokan's alternate Euler interpolation.
    mapping.blend_rate = .35f;
    mapping.joint_flags = {8};
    channels.flags = 0;
    channels.tracks[0].angles[0] = {8192, 8192, 8192};
    auto alternate = apply_motion_pose(joints, channels, 0, mapping);
    mapping.joint_flags.clear();
    ck(alternate.pose[0].angles !=
       apply_motion_pose(joints, channels, 0, mapping).pose[0].angles);
    for (uint8_t format : {uint8_t(0x10), uint8_t(0x20)}) {
      channels.flags = format;
      mapping.joint_flags = {8};
      auto flagged = apply_motion_pose(joints, channels, 0, mapping);
      mapping.joint_flags.clear();
      ck(flagged.pose[0].angles ==
         apply_motion_pose(joints, channels, 0, mapping).pose[0].angles);
    }
    mapping = {};
    mapping.mode = 2;
    mapping.mirror_map = {0, -1};
    bad([&] { apply_motion_pose(joints, channels, 0, mapping); });
    mapping.mirror_map = {0, 99};
    bad([&] { apply_motion_pose(joints, channels, 0, mapping); });
    mapping = {};
    mapping.joint_base = 5;
    bad([&] { apply_motion_pose(joints, channels, 0, mapping); });
    mapping = {};
    mapping.joint_flags = {4, 4, 4, 4, 4};
    bad([&] { apply_motion_pose(joints, channels, 0, mapping); });
    mapping = {};
    mapping.blend_rate = NAN;
    bad([&] { apply_motion_pose(joints, channels, 0, mapping); });
    bad([&] { apply_motion_pose(joints, channels, 65536); });
    auto invalid_channels = channels;
    invalid_channels.tracks[0].positions[0].x = INFINITY;
    bad([&] { apply_motion_pose(joints, invalid_channels, 0); });
    auto missing_fast_translation = channels;
    missing_fast_translation.flags = 0;
    missing_fast_translation.tracks[0].positions.clear();
    bad([&] { apply_motion_pose(joints, missing_fast_translation, 0); });
    ck(joints[0].position->x == 20 && (*joints[0].angles)[0] == 10);
    std::vector<JointPose> angle_start(1), angle_end(1);
    angle_start[0].angles = std::array<uint16_t, 3>{};
    angle_end[0].angles = std::array<uint16_t, 3>{8192, 8192, 8192};
    auto ordinary = blend_motion_pose(angle_start, angle_end, .5f);
    auto shoulder = blend_motion_pose(angle_start, angle_end, .5f, false, {8});
    ck(shoulder[0].angles != ordinary[0].angles);
    auto endpoint = blend_motion_pose(angle_start, angle_end, 1.f, false, {8});
    for (size_t axis = 0; axis < 3; ++axis)
      ck(std::abs(int((*endpoint[0].angles)[axis]) - 8192) < 8);
    bad([&] { blend_motion_pose(angle_start, angle_end, .5f, false, {4}); });
    bad([&] { blend_motion_pose(angle_start, angle_end, .5f, false, {8, 8}); });
    angle_start[0].angles = std::array<uint16_t, 3>{12000, 64000, 3000};
    angle_end[0].angles = std::array<uint16_t, 3>{50000, 9000, 62000};
    shoulder = blend_motion_pose(angle_start, angle_end, .35f, false, {8});
    // Independent evaluation of the source's conversion equations and
    // quarter-wave lookup; tolerate a few integer units of libm rounding.
    std::array<int, 3> expected_shoulder{3856, 2201, 1158};
    for (size_t axis = 0; axis < 3; ++axis)
      ck(std::abs(int((*shoulder[0].angles)[axis]) - expected_shoulder[axis]) <=
         2);
    std::vector<JointPose> current(1), target(1);
    current[0].position = Vec3{2, 4, 6};
    target[0].position = Vec3{10, 12, 14};
    current[0].angles = std::array<uint16_t, 3>{65530, 0, 0};
    target[0].angles = std::array<uint16_t, 3>{6, 2000, 0};
    auto blended = blend_motion_pose(current, target, 0.5f);
    ck(blended[0].position->x == 6 && (*blended[0].angles)[0] == 0 &&
       (*blended[0].angles)[1] == 1000);
    ck(blend_motion_pose(current, target, 0.5f, true)[0].position->x == 10);
    current[0].angles = std::array<uint16_t, 3>{32768, 32768, 32768};
    target[0].angles = std::array<uint16_t, 3>{0, 0, 0};
    ck(*blend_motion_pose(current, target, 0.25f)[0].angles ==
       std::array<uint16_t, 3>{0, 0, 0});
    bad([&] { blend_motion_pose(current, target, NAN); });
    bad([&] { blend_motion_pose({}, target, 0.5f); });
    int32_t counter = 0;
    uint32_t direction = 0;
    ck(advance_motion_frame(3, 65536, 0, counter, direction) == 0 &&
       counter == 65536);
    counter = 2 * 65536 + 32768;
    ck(advance_motion_frame(3, 65536, 0, counter, direction) == 1 &&
       counter == 32768);
    counter = 0;
    ck(advance_motion_frame(3, -65536, 0, counter, direction) == -1 &&
       counter == 2 * 65536);
    counter = 0;
    ck(advance_motion_frame(3, 65536, 1, counter, direction) == 0 &&
       counter == 65536 && direction == 0);
    ck(advance_motion_frame(3, 65536, 1, counter, direction) == 0 &&
       counter == 2 * 65536 && direction == 0xffffffff);
    advance_motion_frame(3, 65536, 1, counter, direction);
    ck(counter == 65536 && direction == 0xffffffff);
    advance_motion_frame(3, 65536, 1, counter, direction);
    ck(counter == 0 && direction == 0);
    counter = 2 * 65536;
    advance_motion_frame(3, 32768, 1, counter, direction);
    ck(counter == 2 * 65536 + 32768 && direction == 0xffffffff);
    bad([&] { advance_motion_frame(0, 65536, 0, counter, direction); });
    bad([&] { advance_motion_frame(65536, 65536, 0, counter, direction); });
    counter = 0;
    direction = 0;
    ck(advance_motion_frame(1, 65536, 0, counter, direction) == 1 &&
       counter == 0);
    counter = 0;
    ck(advance_motion_frame(3, -32768, 0, counter, direction) == -1 &&
       counter == 2 * 65536 + 32768);
    // Source uses a signed extent after its 16-bit shift, including wrap.
    counter = 0;
    ck(advance_motion_frame(65535, 0, 0, counter, direction) == 1 &&
       counter == 65536);
    counter = 0;
    ck(advance_motion_frame(3, 7 * 65536, 0, counter, direction) == 1 &&
       counter == 4 * 65536);
    counter = 2147483647;
    ck(advance_motion_frame(3, 1, 0, counter, direction) == -1 &&
       counter == -2147287040);
    counter = 65536;
    direction = 7;
    ck(advance_motion_frame(3, 65536, 1, counter, direction) == 0 &&
       counter == 0 && direction == 0xfffffff8);
    counter = 0;
    direction = 0;
    ck(advance_motion_frame(1, 65536, 1, counter, direction) == 0 &&
       counter == 65536 && direction == 0xffffffff);
    Bytes bank(8);
    put(bank, 0, 0);
    put(bank, 4, uint32_t(b.size()));
    bank.insert(bank.end(), b.begin(), b.end());
    bank.resize(bank.size() + 4);
    put(bank, bank.size() - 4, 0xffffffff);
    auto decoded_bank = decode_motion_bank(bank, 0, bank.size());
    ck(decoded_bank.size() == 2 && !decoded_bank[0] &&
       decoded_bank[1]->frames == 2);
    bad([&] { decode_motion_bank(bank, 0, bank.size() - 1); });
    bad([&] { decode_motion_bank(bank, 0, bank.size(), 3); });
    bad([&] { decode_motion_bank(bank, 9, 8); });
    bad([&] { decode_motion_bank(bank, 0, bank.size() + 1); });
    Bytes too_many(514 * 4);
    put(too_many, 513 * 4, 0xffffffff);
    bad([&] { decode_motion_bank(too_many, 0, too_many.size()); });
    Bytes max_slots(513 * 4);
    put(max_slots, 512 * 4, 0xffffffff);
    ck(decode_motion_bank(max_slots, 0, max_slots.size()).size() == 512);
    auto padded_bank = bank;
    padded_bank.insert(padded_bank.begin(), 8, 0xff);
    padded_bank.insert(padded_bank.end(), 8, 0xff);
    ck(decode_motion_bank(padded_bank, 8, padded_bank.size() - 8).size() == 2);
    auto two_motions = bank;
    two_motions.resize(two_motions.size() - 4);
    auto second_record = two_motions.size();
    two_motions.resize(second_record + 4);
    put(two_motions, second_record, uint32_t(b.size()));
    two_motions.insert(two_motions.end(), b.begin(), b.end());
    two_motions.resize(two_motions.size() + 4);
    put(two_motions, two_motions.size() - 4, 0xffffffff);
    bad([&] { decode_motion_bank(two_motions, 0, two_motions.size(), 7); });
    ck(decode_motion_bank(two_motions, 0, two_motions.size(), 8).size() == 3);
    Bytes room(1300);
    flt(room, 0, 10);
    put(room, 16, 128);
    put(room, 24, 1100);
    put(room, 1100, 1120);
    put(room, 1104, 0);
    put(room, 1108, 0);
    std::copy(bank.begin(), bank.end(), room.begin() + 1120);
    auto room_bank = decode_room_motion_bank(room);
    ck(room_bank.size() == 512 && !room_bank[0] && room_bank[1]->frames == 2 &&
       !room_bank[511]);
    put(room, 28, uint32_t(1120 + bank.size() - 1));
    bad([&] { decode_room_motion_bank(room); });
    put(room, 28, 0);
    put(room, 1100, 1104);
    bad([&] { decode_room_motion_bank(room); });
    put(room, 1100, 0);
    ck(decode_room_motion_bank(room).size() == 512);
    Bytes populated_room(1600);
    flt(populated_room, 0, 10);
    put(populated_room, 16, 128);
    put(populated_room, 128 + 2 * 4, 1440);
    put(populated_room, 128 + 0x80 + 2 * 4, 2);
    put(populated_room, 24, 1200);
    put(populated_room, 1200, 1250);
    put(populated_room, 1204, 1260);
    put(populated_room, 1208, 1300);
    put(populated_room, 1212, 1396);
    put(populated_room, 1216, 1400);
    std::copy(bank.begin(), bank.end(), populated_room.begin() + 1300);
    auto populated_bank = decode_room_motion_bank(populated_room);
    ck(populated_bank[1]->frames == 2 && !populated_bank[0]);
    auto enemy_room = populated_room;
    put(enemy_room, 1440, 1);
    put(enemy_room, 1476, 1);
    put(enemy_room, 1440 + 4, 67);
    put(enemy_room, 1476 + 4, 67);
    put(enemy_room, 1200, 1300);
    put(enemy_room, 1204, 0);
    auto enemy_banks = decode_room_enemy_motion_banks(enemy_room);
    ck(enemy_banks.size() == 2 && enemy_banks[0] == enemy_banks[1] &&
       enemy_banks[0]->size() == 512 && (*enemy_banks[0])[1]->frames == 2);
    // Sharing follows runtime enemy IDs and load order.
    put(enemy_room, 1476 + 4, 68);
    enemy_banks = decode_room_enemy_motion_banks(enemy_room);
    ck(enemy_banks[0] != enemy_banks[1] && !(*enemy_banks[1])[1]);
    put(enemy_room, 1200, 0);
    put(enemy_room, 1476 + 4, 67);
    enemy_banks = decode_room_enemy_motion_banks(enemy_room);
    ck(enemy_banks[0] == enemy_banks[1] && !(*enemy_banks[0])[1]);
    put(enemy_room, 24, 1200);
    put(enemy_room, 1200, 1300);
    put(enemy_room, 1204, 0);
    put(enemy_room, 1440, 0);
    put(enemy_room, 1440 + 4, 68);
    enemy_banks = decode_room_enemy_motion_banks(enemy_room);
    ck((*enemy_banks[0])[1] && !(*enemy_banks[1])[1] &&
       enemy_banks[0] != enemy_banks[1]);
    // The inactive authored entry's ID never enters the runtime cache.
    put(enemy_room, 1440 + 4, 128);
    ck(decode_room_enemy_motion_banks(enemy_room).size() == 2);
    put(enemy_room, 1440, 1);
    put(enemy_room, 1440 + 4, 67);
    put(enemy_room, 1200, 0);
    put(enemy_room, 1204, 1300);
    enemy_banks = decode_room_enemy_motion_banks(enemy_room);
    ck(enemy_banks[0] != enemy_banks[1] && !(*enemy_banks[0])[1] &&
       (*enemy_banks[1])[1]->frames == 2);
    bad([&] { decode_room_enemy_motion_banks(enemy_room, 3); });
    put(enemy_room, 1200, 1300);
    enemy_banks = decode_room_enemy_motion_banks(enemy_room);
    ck(enemy_banks[0] != enemy_banks[1] && (*enemy_banks[0])[1] &&
       (*enemy_banks[1])[1]);
    bad([&] { decode_room_enemy_motion_banks(enemy_room, 7); });
    ck(decode_room_enemy_motion_banks(enemy_room, 8).size() == 2);
    put(enemy_room, 1204, 1208);
    bad([&] { decode_room_enemy_motion_banks(enemy_room); });
    put(enemy_room, 1204, 1300);
    put(enemy_room, 1476 + 4, 128);
    bad([&] { decode_room_enemy_motion_banks(enemy_room); });
    put(enemy_room, 1476 + 4, 67);
    put(enemy_room, 24, 0);
    enemy_banks = decode_room_enemy_motion_banks(enemy_room);
    ck(enemy_banks[0] == enemy_banks[1] && !(*enemy_banks[0])[1]);
    put(populated_room, 1212, uint32_t(1300 + bank.size() - 1));
    bad([&] { decode_room_motion_bank(populated_room); });
    ck(m.frames == 2 && m.tracks.size() == 1 && m.flags == 0x80);
    auto first = sample_motion(m, 0);
    ck(first[0].position->x == 2 && (*first[0].angles)[0] == 65530);
    auto middle = sample_motion(m, 32768, true);
    ck(middle[0].position->x == 4 && (*middle[0].angles)[0] == 0 &&
       (*middle[0].angles)[1] == 50);
    auto wrap = sample_motion(m, 98304, true);
    ck(wrap[0].position->x == 4 && (*wrap[0].angles)[0] == 0);
    ck(sample_motion(m, 131072)[0].position->x == 2);
    ck((*sample_motion(m, 8192, true)[0].angles)[0] == 65532);
    bad([&] { decode_motion(b, 3); });
    ck(m.attributes.at(0) == 0x1234 && m.attributes.at(1) == 0x5678);
    bad([&] { sample_motion(m, -1); });
    bad([&] { decode_motion(Bytes(15)); });
    auto corrupt = b;
    put(corrupt, 28, 0xfffffffe);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    put(corrupt, 20, 0);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    put(corrupt, 36, 0x7f800000);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    corrupt.resize(70);
    bad([&] { decode_motion(corrupt); });
    corrupt = b;
    put(corrupt, 28, 0xffffffff);
    put(corrupt, 32, 0xffffffff);
    auto absent = sample_motion(decode_motion(corrupt), 0);
    ck(!absent[0].position && !absent[0].angles);
    // A package with a skin record, one textureless model and three motion
    // slots.
    Bytes pack(4);
    auto word = [&](uint32_t v) {
      auto p = pack.size();
      pack.resize(p + 4);
      put(pack, p, v);
    };
    auto append = [&](const Bytes &data) {
      pack.insert(pack.end(), data.begin(), data.end());
    };
    word(4);
    append(Bytes{'S', 'K', 'I', 'N'});
    word(16);
    Bytes mdl(16);
    mdl[0] = 'M';
    mdl[1] = 'D';
    mdl[2] = 'L';
    mdl[3] = 128;
    half(mdl, 4, 16);
    put(mdl, 8, 0xffffffff);
    put(mdl, 12, 0xffffffff);
    append(mdl);
    word(0xffffffff);
    put(pack, 0, uint32_t(pack.size() - 4));
    auto motion_size = pack.size();
    word(0);
    auto motion_start = pack.size();
    word(uint32_t(b.size()));
    append(b);
    word(0);
    word(0);
    word(0xffffffff);
    put(pack, motion_size, uint32_t(pack.size() - motion_start));
    word(0);
    auto package = decode_player_package(pack);
    ck(package.models.size() == 1 && package.models[0].skin.size() == 4);
    ck(package.motions.size() == 3 && package.motions[0] &&
       !package.motions[1] && !package.motions[2]);
    ck(package.models[0].textures.empty() && package.auxiliary.empty());
    bad([&] { decode_player_package(pack, 3); });
    auto repeated = pack;
    Bytes repeat_record(84);
    put(repeat_record, 0, 80);
    std::copy(b.begin(), b.end(), repeat_record.begin() + 4);
    repeated.insert(repeated.begin() + 124, repeat_record.begin(),
                    repeat_record.end());
    put(repeated, 36, 180);
    ck(decode_player_package(repeated, 8).motions.size() == 4);
    bad([&] { decode_player_package(repeated, 7); });
    auto broken = pack;
    put(broken, 0, 0xfffffffe);
    bad([&] { decode_player_package(broken); });
    broken = pack;
    put(broken, motion_size - 4, 0);
    bad([&] { decode_player_package(broken); });
    broken = pack;
    broken.pop_back();
    bad([&] { decode_player_package(broken); });
    // Texture payload alignment is relative to the original package start.
    auto aligned = pack;
    put(aligned, 24, 0); // Model header's texture offset.
    auto tex_header = aligned.size();
    aligned.resize(tex_header + 4);
    put(aligned, tex_header, 0x80000004);
    aligned.resize((aligned.size() + 31) & ~size_t(31), 0xcd);
    aligned.insert(aligned.end(), {1, 2, 3, 4});
    ck(decode_player_package(aligned).models[0].textures ==
       Bytes({1, 2, 3, 4}));
    aligned.pop_back();
    bad([&] { decode_player_package(aligned); });
    auto multiple = pack;
    Bytes second(20);
    put(second, 0, 16);
    std::copy(mdl.begin(), mdl.end(), second.begin() + 4);
    multiple.insert(multiple.begin() + 32, second.begin(), second.end());
    put(multiple, 0, 52);
    put(multiple, 24, 0);
    put(multiple, 44, 0);
    auto append_word = [&](uint32_t value) {
      auto p = multiple.size();
      multiple.resize(p + 4);
      put(multiple, p, value);
    };
    append_word(4);
    multiple.insert(multiple.end(), {4, 3, 2, 1});
    append_word(0x80000004);
    multiple.resize((multiple.size() + 31) & ~size_t(31), 0xcd);
    multiple.insert(multiple.end(), {1, 2, 3, 4});
    auto linked = decode_player_package(multiple);
    ck(linked.models.size() == 2 &&
       linked.models[0].textures == Bytes({4, 3, 2, 1}) &&
       linked.models[1].textures == Bytes({1, 2, 3, 4}));
    Bytes skin(40);
    skin[0] = 'S';
    skin[1] = 'K';
    skin[2] = 'I';
    skin[3] = 'N';
    put(skin, 4, 3);
    put(skin, 8, 0);
    put(skin, 12, 3);
    put(skin, 16, 0x01090000);
    put(skin, 20, 0x01040000);
    put(skin, 24, 0x01000000);
    put(skin, 28, 1);
    put(skin, 32, 0);
    put(skin, 36, 0xffffffff);
    auto binding = decode_skin(skin, 2);
    ck(binding.size() == 2 && binding[0].vertices.size() == 3);
    ck(binding[0].vertices[0].joint == 1 &&
       binding[0].vertices[0].rotation_weight == 1);
    ck(binding[0].vertices[1].rotation_weight == 0.5f &&
       binding[0].vertices[2].rotation_weight == 0.1f);
    auto invalid = skin;
    put(invalid, 16, 0x02090000);
    bad([&] { decode_skin(invalid, 2); });
    invalid = skin;
    put(invalid, 16, 0x010a0000);
    bad([&] { decode_skin(invalid, 2); });
    invalid = skin;
    put(invalid, 32, 1);
    bad([&] { decode_skin(invalid, 2); });
    invalid = skin;
    put(invalid, 12, 0xffffffff);
    bad([&] { decode_skin(invalid, 2); });
    // Bounded deterministic mutations must either decode safely or reject.
    uint32_t rng = 0x8172;
    for (int i = 0; i < 600; ++i) {
      auto mutated = b;
      rng = rng * 1664525 + 1013904223;
      mutated[rng % mutated.size()] ^= uint8_t(rng >> 24);
      try {
        auto decoded = decode_motion(mutated);
        sample_motion(decoded, 32768, true);
      } catch (const std::runtime_error &) {
      }
      mutated = pack;
      rng = rng * 1664525 + 1013904223;
      mutated[rng % mutated.size()] ^= uint8_t(rng >> 24);
      try {
        decode_player_package(mutated);
      } catch (const std::runtime_error &) {
      }
    }
    std::cout << "Motion checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
