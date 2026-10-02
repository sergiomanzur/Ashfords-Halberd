#include "actor.hpp"
#include "render.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace ashford;
Bytes read(std::ifstream &f, uint64_t offset, size_t size) {
  if (size > 64 * 1024 * 1024)
    throw std::runtime_error("Player archive extent exceeds limit");
  Bytes b(size);
  f.seekg(std::streamoff(offset));
  if (!f.read(reinterpret_cast<char *>(b.data()), std::streamsize(size)))
    throw std::runtime_error("Cannot read player archive");
  return b;
}
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc != 3 && argc != 4 && argc != 6)
      throw std::runtime_error(
          "Usage: AshfordPlayerInspect SYSTEM.AFS "
          "entry-index [--pose-check | new-preview-directory [RDX_LNK.AFS "
          "room-index]]");
    const std::filesystem::path path(argv[1]);
    std::ifstream f(path, std::ios::binary);
    if (!f)
      throw std::runtime_error("Cannot open player archive");
    auto h = read(f, 0, 8);
    uint32_t count = uint32_t(h[4]) | uint32_t(h[5]) << 8 |
                     uint32_t(h[6]) << 16 | uint32_t(h[7]) << 24;
    if (count > 10000)
      throw std::runtime_error("Invalid player archive count");
    auto entries = afs_index(read(f, 0, 8 + size_t(count) * 8),
                             std::filesystem::file_size(path));
    size_t index = std::stoul(argv[2]);
    if (index >= entries.size())
      throw std::runtime_error("Player entry outside archive");
    auto entry = entries[index];
    auto package = decode_player_package(read(f, entry.offset, entry.size));
    if (argc >= 4) {
      bool pose_only = argc == 4 && std::wstring(argv[3]) == L"--pose-check";
      const std::filesystem::path destination(argv[3]);
      if (!pose_only && std::filesystem::exists(destination))
        throw std::runtime_error("Player preview directory must not exist");
      if (package.models.empty())
        throw std::runtime_error("No player model to preview");
      Scene room_scene;
      std::vector<RoomCut> cuts;
      if (argc == 6) {
        const std::filesystem::path room_path(argv[4]);
        std::ifstream rf(room_path, std::ios::binary);
        if (!rf)
          throw std::runtime_error("Cannot open room archive");
        auto header = read(rf, 0, 8);
        size_t count = uint32_t(header[4]) | uint32_t(header[5]) << 8 |
                       uint32_t(header[6]) << 16 | uint32_t(header[7]) << 24;
        if (count > 10000)
          throw std::runtime_error("Invalid room archive count");
        auto entries = afs_index(read(rf, 0, 8 + count * 8),
                                 std::filesystem::file_size(room_path));
        size_t room = std::stoul(argv[5]);
        if (room >= entries.size())
          throw std::runtime_error("Room index outside archive");
        auto entry = entries[room];
        auto bytes = expand(read(rf, entry.offset, entry.size));
        room_scene = decode_room_scene(bytes);
        cuts = decode_room_cuts(bytes);
        if (cuts.empty())
          throw std::runtime_error("Room has no authored camera");
      }
      if (!pose_only)
        std::filesystem::create_directories(destination);
      size_t views = 0;
      for (size_t motion = 0; motion < package.motions.size(); ++motion)
        if (package.motions[motion]) {
          const auto &m = *package.motions[motion];
          // Validate first, quarter and half poses for every populated motion.
          for (uint32_t frame : {0u, m.frames / 4, m.frames / 2}) {
            auto scene = pose_actor(package.models[0],
                                    sample_motion(m, int64_t(frame) * 65536));
            if (motion < 4 && !pose_only) {
              auto image = render_overview(scene, 640, 480);
              std::ofstream output(destination /
                                       ("motion-" + std::to_string(motion) +
                                        "-" + std::to_string(frame) + ".ppm"),
                                   std::ios::binary);
              output << "P6\n"
                     << image.width << ' ' << image.height << "\n255\n";
              output.write(reinterpret_cast<const char *>(image.rgb.data()),
                           std::streamsize(image.rgb.size()));
              if (!output)
                throw std::runtime_error("Cannot write player preview");
              if (!cuts.empty()) {
                // Diagnostic placement, not the original player spawn/event
                // state.
                auto position = cuts[0].origin;
                position.x += cuts[0].size.x * 0.5f;
                position.z += cuts[0].size.z * 0.5f;
                auto actor = pose_actor(
                    package.models[0], sample_motion(m, int64_t(frame) * 65536),
                    position);
                auto combined = room_scene;
                size_t offset = combined.textures.size();
                if (offset + actor.textures.size() > 65535)
                  throw std::runtime_error("Combined texture limit exceeded");
                combined.textures.insert(combined.textures.end(),
                                         actor.textures.begin(),
                                         actor.textures.end());
                for (auto &mesh : actor.meshes) {
                  for (auto &triangle : mesh.triangles)
                    if (triangle.texture != 0xffff)
                      triangle.texture = uint16_t(triangle.texture + offset);
                  combined.meshes.push_back(std::move(mesh));
                }
                auto room_image = render_camera(
                    combined, resolve_camera(cuts[0], 0), 640, 480);
                std::ofstream room_output(
                    destination / ("room-motion-" + std::to_string(motion) +
                                   "-" + std::to_string(frame) + ".ppm"),
                    std::ios::binary);
                room_output << "P6\n"
                            << room_image.width << ' ' << room_image.height
                            << "\n255\n";
                room_output.write(
                    reinterpret_cast<const char *>(room_image.rgb.data()),
                    std::streamsize(room_image.rgb.size()));
                if (!room_output)
                  throw std::runtime_error(
                      "Cannot write combined room preview");
              }
            }
            ++views;
          }
        }
      std::cout << "validated_actor_poses=" << views << '\n';
    }
    size_t present = 0, frames = 0, tracks = 0;
    for (const auto &m : package.motions)
      if (m) {
        ++present;
        frames += m->frames;
        tracks += m->tracks.size();
        // Traverse every frame and half-frame; this tests runtime sampling too.
        for (uint32_t frame = 0; frame < m->frames; ++frame) {
          sample_motion(*m, int64_t(frame) * 65536);
          sample_motion(*m, int64_t(frame) * 65536 + 32768, true);
        }
      }
    std::cout << "entry=" << index << " models=" << package.models.size()
              << " slots=" << package.motions.size() << " present=" << present
              << " frames=" << frames << " tracks=" << tracks << '\n';
    for (size_t i = 0; i < package.models.size(); ++i) {
      const auto &m = package.models[i];
      if (!m.skin.empty()) {
        size_t count = uint16_t(m.model.at(6)) | uint16_t(m.model.at(7)) << 8;
        auto skin = decode_skin(m.skin, count);
        size_t vertices = 0;
        for (const auto &node : skin)
          vertices += node.vertices.size();
        std::cout << "skin_model=" << i << " objects=" << count
                  << " vertices=" << vertices << '\n';
      }
      std::cout << "model=" << i << " bytes=" << m.model.size()
                << " skin=" << m.skin.size()
                << " textures=" << m.textures.size() << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
