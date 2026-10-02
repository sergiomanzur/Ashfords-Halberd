#include "actor.hpp"
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
    if (argc != 3)
      throw std::runtime_error(
          "Usage: AshfordPlayerInspect SYSTEM.AFS entry-index");
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
