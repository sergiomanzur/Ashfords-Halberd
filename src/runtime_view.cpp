#include "item_use_area.hpp"
#include "runtime_view.hpp"
#include "camera.hpp"
#include "effect_assets.hpp"
#include "inventory_engine.hpp"
#include "item_pickup.hpp"
#include "ninja_matrix.hpp"
#include "render.hpp"
#include "room.hpp"
#include "save_state.hpp"
#include "weapon.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace ashford {
namespace {
Bytes archive_entry(const fs::path &path, size_t index, bool compressed) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("Could not open native runtime archive");
  auto read = [&](uint64_t offset, size_t size) {
    if (size > 64 * 1024 * 1024)
      throw std::runtime_error("Runtime archive extent exceeds limit");
    Bytes bytes(size);
    file.seekg(std::streamoff(offset));
    if (!file.read(reinterpret_cast<char *>(bytes.data()),
                   std::streamsize(size)))
      throw std::runtime_error("Could not read native runtime archive");
    return bytes;
  };
  auto header = read(0, 8);
  size_t count = uint32_t(header[4]) | uint32_t(header[5]) << 8 |
                 uint32_t(header[6]) << 16 | uint32_t(header[7]) << 24;
  if (count > 10000)
    throw std::runtime_error("Invalid native runtime archive count");
  auto entries = afs_index(read(0, 8 + count * 8), fs::file_size(path));
  if (index >= entries.size())
    throw std::runtime_error("Missing native runtime archive entry");
  auto entry = entries[index];
  auto bytes = read(entry.offset, entry.size);
  return compressed ? expand(bytes) : bytes;
}
} // namespace

RuntimeView::RuntimeView(SDL_Renderer *renderer, const fs::path &rooms,
                         const fs::path &system, int effects_volume)
    : renderer_(renderer),
      rooms_path_(rooms),
      system_path_(system),
      sound_root_(rooms.parent_path()) {
  player_ = decode_player_package(archive_entry(system, 10, false));
  translations_ = decode_player_translation_tables(player_.auxiliary);
  set_effects_volume(effects_volume);
  ScenarioState initial;
  initial.effect_assets =
      decode_global_effect_assets(archive_entry(system, 1, false));
  initial.player_health = 160;
  // Experimental room-1 slice loadout, using original item IDs and bank
  // ownership. This is not the original room-0 introduction/startup path.
  InventoryEngine::set_extra_slot(initial, InventoryEngine::make_slot(55, 1));
  InventoryEngine::set_slot(initial, 0, InventoryEngine::make_slot(9, 15));
  InventoryEngine::set_slot(initial, 1, InventoryEngine::make_slot(12, 30));
  InventoryEngine::set_slot(initial, 2, InventoryEngine::make_slot(8, 0));
  InventoryEngine::set_slot(initial, 3, InventoryEngine::make_slot(21, 1));
  InventoryEngine::set_slot(initial, 4, InventoryEngine::make_slot(20, 1));
  std::string message;
  if (!InventoryEngine::equip_weapon(initial, 0, message))
    throw std::runtime_error("Could not initialize slice inventory: " + message);
  runtime_ = load_room_runtime(
      archive_entry(rooms, 1, true), 0, 1, 0, std::move(initial), player_,
      nullptr, [&](uint8_t stage, uint8_t room, int32_t sound_case) {
        return read_room_sound_assets(sound_root_ / "MULTSPQ1.AFS",
                                      sound_root_ / "MULTSPQ1.IDX", stage, room,
                                      sound_case);
      });
  audio_ = std::make_unique<RoomAudioPlayback>(runtime_.sound_assets);
  audio_->apply(runtime_.scenario.background_audio.commands);
  runtime_.scenario.background_audio.commands.clear();
  tick(0);
  texture_.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24,
                                   SDL_TEXTUREACCESS_STREAMING, 320, 240));
  if (!texture_)
    throw std::runtime_error(SDL_GetError());
  set_audio_running(true);
}

void RuntimeView::tick(uint16_t pad) {
  try {
    uint16_t pressed = uint16_t(pad & ~previous_pad_);
    previous_pad_ = pad;

    aim_mode_ = (pad & 0x10) != 0;

    if (aim_mode_ && ((pressed & 0x100) || (pressed & 0x200))) {
      // The ammunition owner is the selected carried word. Gun indices are
      // WeaponSet indices; never pass an inventory item ID to damage tables.
      const auto slot = InventoryEngine::equipped_slot(runtime_.scenario);
      const int32_t weapon = runtime_.scenario.equipped_weapon;
      const auto item_id = slot ? InventoryEngine::get_item_id(
          InventoryEngine::get_slot(runtime_.scenario, *slot)) : 0;
      if (slot && weapon > 2 && item_id != 55 &&
          InventoryEngine::consume_equipped_ammo(runtime_.scenario)) {

        GunAttackWork gap;
        const auto &p = runtime_.world.player;
        gap.set_player_pos(p.position);
        auto sincos = ninja_sin_cos(p.yaw);
        Vec3 dir{-sincos[0], 0.0f, -sincos[1]};
        gap.set_dir(dir);
        Vec3 gun_pos = p.position;
        gun_pos.y += 12.0f;
        gun_pos.x -= sincos[0] * 1.0f;
        gun_pos.z -= sincos[1] * 1.0f;
        gap.set_gun_pos(gun_pos);
        gap.wpn_no = weapon;
        gap.r = 0.5f;
        gap.l = 200.0f;
        gap.rmax = 200.0f;

        auto res = check_gun_atari(gap, runtime_.scenario);
        last_shot_hit_ = res.hit;
        last_damage_dealt_ = res.damage;

        if (res.hit && res.enemy_index >= 0) {
          auto &enemy = runtime_.scenario.enemies[res.enemy_index];
          if (enemy.health <= 0) {
            size_t progress = runtime_.scenario.room == 1 ? 0 : 33;
            size_t word = (progress & 0x3ff) >> 5;
            uint32_t mask = 0x80000000u >> (progress & 31);
            runtime_.scenario.flags[3][word] |= mask;
          }
        }
      } else {
        last_shot_hit_ = false;
        last_damage_dealt_ = 0;
      }
    }

    uint16_t sim_pad = aim_mode_ ? (pad & ~0x020fu) : pad;
    tick_room_runtime(runtime_, player_, translations_, sim_pad);

    if (runtime_.scenario.flags[10][0] & 0x10) {
      std::string message;
      accept_selected_item(runtime_.scenario, message);
    }

    if (runtime_.scenario.door && (runtime_.scenario.flags[10][0] & 1)) {
      auto door = *runtime_.scenario.door;
      size_t dest_idx = room_archive_index(door.stage, door.room,
                                           runtime_.scenario.room_case);
      auto dest_data = archive_entry(rooms_path_, dest_idx, true);
      transition_room_runtime(
          runtime_, dest_data, player_,
          [&](uint8_t stage, uint8_t room, int32_t sound_case) {
            return read_room_sound_assets(sound_root_ / "MULTSPQ1.AFS",
                                          sound_root_ / "MULTSPQ1.IDX", stage,
                                          room, sound_case);
          });
      audio_ = std::make_unique<RoomAudioPlayback>(runtime_.sound_assets);
      audio_->apply(runtime_.scenario.background_audio.commands);
      runtime_.scenario.background_audio.commands.clear();
      tick_room_runtime(runtime_, player_, translations_, 0);
    }

    audio_->apply(runtime_.scenario.background_audio.commands);
    runtime_.scenario.background_audio.commands.clear();
    const auto frames = audio_->tick(effects_volume_);
    if (audio_running_)
      output_.queue(frames);
    ++ticks_;
  } catch (...) {
    set_audio_running(false);
    audio_->stop();
    throw;
  }
}

void RuntimeView::set_audio_running(bool running) {
  output_.pause(!running);
  audio_running_ = running;
}

void RuntimeView::set_effects_volume(int volume) {
  if (volume < 0 || volume > 100)
    throw std::runtime_error("Invalid effects volume");
  effects_volume_ = volume;
}

void RuntimeView::draw(const SDL_Rect &destination) {
  if (!runtime_.world.cut)
    throw std::runtime_error("Native runtime has no active original camera");
  auto scene = compose_room_runtime(runtime_, runtime_.scenario.drawing_flags,
                                    runtime_.camera_flags);
  auto image = render_camera(
      scene, resolve_camera(runtime_.cuts.at(*runtime_.world.cut), 0), 320, 240,
      &ambient_material_, &runtime_.scenario.fog.active.enabled,
      &light_material_);
  if (scene.ending_light_work)
    runtime_.scenario.lights = std::move(scene.ending_light_work);
  if (scene.ending_lighting)
    runtime_.scenario.drawing_lighting = scene.ending_lighting;
  if (SDL_UpdateTexture(texture_.get(), nullptr, image.rgb.data(), 320 * 3) <
          0 ||
      SDL_RenderCopy(renderer_, texture_.get(), nullptr, &destination) < 0)
    throw std::runtime_error(SDL_GetError());
}

void RuntimeView::save(const fs::path &path) {
  save_game_to_file(path, runtime_);
}

void RuntimeView::load(const fs::path &path) {
  ScenarioState loaded_scenario;
  PlayerWorldState loaded_world;
  load_game_from_file(path, loaded_scenario, loaded_world);
  const auto saved_items = loaded_scenario.inventory;
  const auto saved_owner = loaded_scenario.player_id;
  const auto saved_mode = loaded_scenario.game_mode;
  const auto saved_health = loaded_scenario.player_health;
  const auto saved_spray_count = loaded_scenario.spray_use_count;
  const auto saved_weapon = loaded_scenario.equipped_weapon;
  const auto saved_player_flags = loaded_scenario.player_state_flags;

  size_t room_idx = room_archive_index(loaded_scenario.stage, loaded_scenario.room,
                                       loaded_scenario.room_case);
  auto room_data = archive_entry(rooms_path_, room_idx, true);

  loaded_scenario.flags[10][0] &= ~0x800000u;
  if (runtime_.scenario.effect_assets) {
    loaded_scenario.effect_assets = runtime_.scenario.effect_assets;
  } else {
    loaded_scenario.effect_assets =
        decode_global_effect_assets(archive_entry(system_path_, 1, false));
  }

  auto replacement = load_room_runtime(
      room_data, loaded_scenario.stage, loaded_scenario.room, 0,
      std::move(loaded_scenario), player_, nullptr,
      [&](uint8_t stage, uint8_t room, int32_t sound_case) {
        return read_room_sound_assets(sound_root_ / "MULTSPQ1.AFS",
                                      sound_root_ / "MULTSPQ1.IDX", stage, room,
                                      sound_case);
      });

  // Room entry scripts are resource reconstruction, not an inventory reset.
  // Restore the saved item owner and recovery state after those scripts run.
  replacement.scenario.inventory = saved_items;
  replacement.scenario.player_id = saved_owner;
  replacement.scenario.game_mode = saved_mode;
  replacement.scenario.player_health = saved_health;
  replacement.scenario.spray_use_count = saved_spray_count;
  replacement.scenario.equipped_weapon = saved_weapon;
  replacement.scenario.player_state_flags = saved_player_flags;

  replacement.world.player.position = loaded_world.player.position;
  replacement.world.player.yaw = loaded_world.player.yaw;
  replacement.world.player.motion = loaded_world.player.motion;
  replacement.world.player.frame = loaded_world.player.frame;
  replacement.world.floor = loaded_world.floor;
  replacement.world.player.visual_anchor = loaded_world.player.position;
  replacement.world.front_anchor = loaded_world.player.position;
  replacement.world.cut = find_camera_cut(
      replacement.cuts, replacement.world.player.position.x,
      replacement.world.player.position.z, replacement.world.floor);

  replacement.player_pose = sample_player_locomotion_pose(
      player_, replacement.world.player, {},
      &replacement.scenario.motion_angle_scratch);
  replacement.player_scene = pose_actor(
      player_.models.at(0), replacement.player_pose,
      replacement.world.player.position, replacement.world.player.yaw);

  synchronize_runtime_player(replacement, player_);
  update_inventory_use_area(replacement);

  auto replacement_audio = std::make_unique<RoomAudioPlayback>(replacement.sound_assets);
  replacement_audio->apply(replacement.scenario.background_audio.commands);
  replacement.scenario.background_audio.commands.clear();
  runtime_ = std::move(replacement);
  audio_ = std::move(replacement_audio);
  previous_pad_ = 0;
  aim_mode_ = false;
  last_shot_hit_ = false;
  last_damage_dealt_ = 0;
}

} // namespace ashford
