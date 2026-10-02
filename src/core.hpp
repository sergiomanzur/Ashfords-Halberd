#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace ashford {
namespace fs = std::filesystem;
inline constexpr const char* retail_sha1 = "f6c26ed664c2de72627fea5323b0d6a931601e13";
struct Settings {
    std::string iso_path;
    int width = 1120, height = 720;
    std::string mode = "windowed";
    bool vsync = true;
};
Settings load_settings(const fs::path& path);
void save_settings(const fs::path& path, const Settings& settings);
struct DiscFile { std::string name; std::uint64_t offset, size; };
struct Disc { fs::path path; std::vector<DiscFile> files; };
Disc read_disc(const fs::path& path);
void validate_disc(const Disc& disc);
void import_disc(const Disc& disc, const fs::path& root,
                 const std::function<void(float, const std::string&)>& progress);
bool cache_valid(const fs::path& root);
void recover_import(const fs::path& root);
std::string sha1_file(const fs::path& path);
}
