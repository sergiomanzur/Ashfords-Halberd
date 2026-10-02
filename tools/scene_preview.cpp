#include "render.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
namespace fs = std::filesystem;
Bytes read(std::ifstream &file, uint64_t offset, size_t size) {
  if (size > 64 * 1024 * 1024)
    throw std::runtime_error("Preview read exceeds limit");
  Bytes b(size);
  file.seekg(std::streamoff(offset));
  if (!file.read(reinterpret_cast<char *>(b.data()), std::streamsize(size)))
    throw std::runtime_error("Preview read failed");
  return b;
}
void ppm(const fs::path &path, uint32_t width, uint32_t height,
         const Bytes &rgb) {
  std::ofstream f(path, std::ios::binary);
  f << "P6\n" << width << ' ' << height << "\n255\n";
  f.write(reinterpret_cast<const char *>(rgb.data()),
          std::streamsize(rgb.size()));
  f.flush();
  if (!f)
    throw std::runtime_error("Preview write failed");
}
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc != 4)
      throw std::runtime_error("Usage: AshfordScenePreview RDX_LNK.AFS "
                               "room-index new-output-directory");
    fs::path archive(argv[1]), destination(argv[3]);
    // A fresh directory avoids overwriting any existing assets or user files.
    if (fs::exists(destination))
      throw std::runtime_error("Preview output directory must not exist");
    std::wstring argument(argv[2]);
    size_t used = 0;
    auto index = std::stoul(argument, &used);
    if (used != argument.size())
      throw std::runtime_error("Invalid room index");
    std::ifstream f(archive, std::ios::binary);
    if (!f)
      throw std::runtime_error("Could not open room archive");
    auto header = read(f, 0, 8);
    auto count = uint32_t(header[4]) | uint32_t(header[5]) << 8 |
                 uint32_t(header[6]) << 16 | uint32_t(header[7]) << 24;
    if (count > 10000)
      throw std::runtime_error("Invalid archive count");
    auto entries =
        afs_index(read(f, 0, 8 + size_t(count) * 8), fs::file_size(archive));
    if (index >= entries.size())
      throw std::runtime_error("Room index outside archive");
    auto scene = decode_room_scene(
        expand(read(f, entries[index].offset, entries[index].size)));
    auto image = render_overview(scene, 960, 720);
    fs::create_directories(destination);
    ppm(destination / "overview.ppm", image.width, image.height, image.rgb);
    for (size_t i = 0; i < scene.textures.size(); ++i) {
      const auto &t = scene.textures[i];
      Bytes rgb(size_t(t.width) * t.height * 3);
      for (size_t p = 0; p < rgb.size() / 3; ++p)
        for (size_t c = 0; c < 3; ++c)
          rgb[p * 3 + c] = t.rgba[p * 4 + c];
      ppm(destination / ("texture-" + std::to_string(i) + ".ppm"), t.width,
          t.height, rgb);
    }
    size_t triangles = 0;
    for (const auto &mesh : scene.meshes)
      triangles += mesh.triangles.size();
    std::ofstream info(destination / "summary.txt");
    info << "Room index: " << index << "\nMeshes: " << scene.meshes.size()
         << "\nTriangles: " << triangles
         << "\nTextures: " << scene.textures.size()
         << "\nCamera: diagnostic orthographic overview\n";
    info.flush();
    if (!info)
      throw std::runtime_error("Summary write failed");
    std::cout << "Rendered " << triangles << " triangles with "
              << scene.textures.size() << " textures headlessly\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
