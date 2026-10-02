#include "core.hpp"
#include "usa_inventory.hpp"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ashford {
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string trim(std::string value) {
    auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    return value.substr(start, value.find_last_not_of(" \t\r\n") - start + 1);
}
int bounded(const std::string& value, int fallback, int low, int high) {
    try { std::size_t used; int number = std::stoi(value, &used);
        return used == value.size() && number >= low && number <= high ? number : fallback;
    } catch (...) { return fallback; }
}
std::map<std::string, std::string> read_configuration(const fs::path& path, bool& healthy) {
    std::map<std::string, std::string> values;
    std::ifstream input(path); std::string section, line; healthy = bool(input);
    while (std::getline(input, line)) {
        line = trim(line); if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
        auto equals = line.find('=');
        if (equals == std::string::npos || section.empty()) { healthy = false; continue; }
        values[section + "." + trim(line.substr(0, equals))] = trim(line.substr(equals + 1));
    }
    auto valid_number = [&](const char* key, int low, int high) {
        auto found = values.find(key); return found == values.end() || bounded(found->second, -1, low, high) != -1;
    };
    healthy &= valid_number("Display.width", 640, 7680) && valid_number("Display.height", 480, 4320);
    auto mode = values.find("Display.mode");
    healthy &= mode == values.end() || mode->second == "windowed" || mode->second == "borderless" || mode->second == "fullscreen";
    auto vsync = values.find("Display.vsync");
    healthy &= vsync == values.end() || vsync->second == "0" || vsync->second == "1" || vsync->second == "true" || vsync->second == "false";
    return values;
}
bool safe_component(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') return false;
    if (name.find_first_of("/\\:< >\"|?*\t\r\n") != std::string::npos) return false;
    for (unsigned char c : name) if (c < 32 || c > 126) return false;
    std::string base = name.substr(0, name.find('.'));
    std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL") return false;
    if (base.size() == 4 && (base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") && base[3] >= '0' && base[3] <= '9') return false;
    return true;
}
bool safe_relative(const std::string& name) {
    std::istringstream stream(name); std::string component;
    if (name.empty() || name.front() == '/' || name.back() == '/') return false;
    while (std::getline(stream, component, '/')) if (!safe_component(component)) return false;
    return true;
}
std::uint32_t le32(const unsigned char* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
std::uint32_t both32(const unsigned char* p) {
    auto value = le32(p);
    auto big = std::uint32_t(p[4]) << 24 | std::uint32_t(p[5]) << 16 | std::uint32_t(p[6]) << 8 | p[7];
    check(value == big, "Inconsistent ISO record byte order."); return value;
}
std::vector<unsigned char> read_at(std::ifstream& stream, std::uint64_t offset, std::uint64_t size, std::uint64_t limit) {
    check(offset <= limit && size <= limit - offset && size <= 16 * 1024 * 1024, "Invalid or truncated ISO extent.");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    stream.clear(); stream.seekg(static_cast<std::streamoff>(offset));
    if (size) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    check(bool(stream), "Cannot read ISO data."); return bytes;
}
class Hash {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE handle = nullptr;
    std::vector<unsigned char> object;
public:
    Hash() {
        check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) >= 0, "Cannot initialize SHA-1.");
        DWORD size = 0, used = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof(size), &used, 0) < 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0); throw std::runtime_error("Cannot query SHA-1 provider.");
        }
        object.resize(size);
        if (BCryptCreateHash(algorithm, &handle, object.data(), size, nullptr, 0, 0) < 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0); throw std::runtime_error("Cannot create SHA-1 hash.");
        }
    }
    ~Hash() { if (handle) BCryptDestroyHash(handle); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    void add(unsigned char* data, ULONG size) { check(BCryptHashData(handle, data, size, 0) >= 0, "Hashing failed."); }
    std::string finish() {
        unsigned char result[20]; check(BCryptFinishHash(handle, result, 20, 0) >= 0, "Hashing failed.");
        const char* hex = "0123456789abcdef"; std::string text;
        for (unsigned char c : result) { text += hex[c >> 4]; text += hex[c & 15]; } return text;
    }
};
std::string hash_range(std::ifstream& input, std::uint64_t offset, std::uint64_t size) {
    input.clear(); input.seekg(static_cast<std::streamoff>(offset)); Hash hash;
    std::array<unsigned char, 65536> buffer;
    while (size) {
        auto amount = static_cast<ULONG>(std::min<std::uint64_t>(size, buffer.size()));
        input.read(reinterpret_cast<char*>(buffer.data()), amount);
        check(input.gcount() == amount, "Truncated executable data."); hash.add(buffer.data(), amount); size -= amount;
    }
    return hash.finish();
}
const DiscFile& find_file(const Disc& disc, const char* name) {
    auto found = std::find_if(disc.files.begin(), disc.files.end(), [&](const DiscFile& file) { return file.name == name; });
    check(found != disc.files.end(), "This disc is not CVX USA (SLUS-20184)."); return *found;
}
}

Settings load_settings(const fs::path& path) {
    Settings result; if (!fs::exists(path)) return result;
    bool healthy; auto values = read_configuration(path, healthy);
    if (!healthy) { std::error_code error; fs::copy_file(path, fs::path(path.wstring() + L".damaged"), fs::copy_options::overwrite_existing, error); }
    result.iso_path = values["GameData.iso_path"];
    result.width = bounded(values["Display.width"], 1120, 640, 7680);
    result.height = bounded(values["Display.height"], 720, 480, 4320);
    auto mode = values["Display.mode"];
    if (mode == "windowed" || mode == "borderless" || mode == "fullscreen") result.mode = mode;
    auto vsync = values["Display.vsync"];
    if (vsync == "0" || vsync == "false") result.vsync = false;
    return result;
}
void save_settings(const fs::path& path, const Settings& value) {
    check(value.iso_path.find_first_of("\r\n") == std::string::npos, "Invalid ISO path for configuration.");
    fs::create_directories(path.parent_path());
    fs::path temporary(path.wstring() + L".tmp"), backup(path.wstring() + L".bak");
    { std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      output << "; Ashford's Halberd - user settings\n[General]\nschema_version=1\n\n[GameData]\niso_path=" << value.iso_path
             << "\n\n[Display]\nwidth=" << value.width << "\nheight=" << value.height
             << "\nmode=" << value.mode << "\nvsync=" << (value.vsync ? 1 : 0) << "\n";
      output.flush(); check(bool(output), "Cannot write settings. Check available disk space and folder permissions."); }
    if (fs::exists(path)) {
        bool healthy; read_configuration(path, healthy);
        if (healthy) check(CopyFileW(path.c_str(), backup.c_str(), FALSE) != 0, "Cannot back up settings.");
        else check(CopyFileW(path.c_str(), fs::path(path.wstring() + L".damaged").c_str(), FALSE) != 0, "Cannot preserve damaged settings.");
    }
    check(MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
          "Cannot save settings. Check folder permissions.");
}
Disc read_disc(const fs::path& path) {
    Disc disc { fs::absolute(path), {} };
    std::ifstream input(path, std::ios::binary); check(bool(input), "Cannot open the selected ISO.");
    auto limit = fs::file_size(path); check(limit >= 18 * 2048, "Truncated ISO image.");
    std::vector<unsigned char> primary;
    for (unsigned sector = 16; sector < 64; ++sector) {
        auto bytes = read_at(input, sector * 2048, 2048, limit);
        check(std::string(reinterpret_cast<char*>(bytes.data() + 1), 5) == "CD001" && bytes[6] == 1,
              "Not a supported ISO9660 disc image.");
        if (bytes[0] == 1) { primary = std::move(bytes); break; }
        if (bytes[0] == 255) break;
    }
    check(!primary.empty(), "Missing ISO volume descriptor.");
    check(primary[128] == 0 && primary[129] == 8 && primary[130] == 8 && primary[131] == 0,
          "Unsupported ISO sector size.");
    auto volume_size = std::uint64_t(both32(primary.data() + 80)) * 2048;
    check(volume_size <= limit && volume_size >= 18 * 2048, "Truncated ISO volume.");
    limit = volume_size;
    check(primary[156] >= 34 && (primary[181] & 2), "Invalid ISO root directory.");
    std::set<std::uint32_t> visited; std::set<std::string> names;
    std::function<void(std::uint32_t, std::uint32_t, const std::string&, int)> walk;
    walk = [&](std::uint32_t extent, std::uint32_t size, const std::string& prefix, int depth) {
        check(depth <= 16 && visited.insert(extent).second, "Invalid ISO directory cycle or depth.");
        auto bytes = read_at(input, std::uint64_t(extent) * 2048, size, limit);
        for (std::size_t at = 0; at < bytes.size();) {
            auto length = bytes[at];
            if (!length) { at = (at / 2048 + 1) * 2048; continue; }
            check(length >= 34 && at + length <= bytes.size() && at % 2048 + length <= 2048, "Invalid ISO directory record.");
            const auto* record = bytes.data() + at;
            auto name_length = record[32];
            check(name_length > 0 && 33 + name_length <= length && record[1] == 0 && !(record[25] & 0x80),
                  "Unsupported ISO record format.");
            at += length;
            if (name_length == 1 && record[33] <= 1) continue;
            std::string name(reinterpret_cast<const char*>(record + 33), name_length);
            auto version = name.find(';'); if (version != std::string::npos) name.erase(version);
            if (!name.empty() && name.back() == '.') name.pop_back();
            check(safe_component(name), "Unsafe filename in ISO image.");
            auto full = prefix + name;
            check(names.insert(full).second && names.size() <= 10000, "Duplicate or excessive ISO entries.");
            auto file_extent = both32(record + 2), file_size = both32(record + 10);
            auto offset = std::uint64_t(file_extent) * 2048;
            check(offset <= limit && file_size <= limit - offset, "Invalid or truncated ISO extent.");
            if (record[25] & 2) walk(file_extent, file_size, full + "/", depth + 1);
            else disc.files.push_back({full, offset, file_size});
        }
    };
    walk(both32(primary.data() + 158), both32(primary.data() + 166), "", 0);
    return disc;
}
void validate_disc(const Disc& disc) {
    const auto& executable = find_file(disc, "SLUS_201.84");
    const auto& config = find_file(disc, "SYSTEM.CNF");
    std::ifstream input(disc.path, std::ios::binary);
    auto bytes = read_at(input, config.offset, config.size, fs::file_size(disc.path));
    std::string boot(bytes.begin(), bytes.end());
    std::istringstream lines(boot); std::string line, boot_target;
    unsigned directives = 0;
    while (std::getline(lines, line)) {
        line = trim(line); if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto equals = line.find('='); if (equals == std::string::npos) continue;
        auto key = trim(line.substr(0, equals));
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::toupper(c)); });
        if (key == "BOOT2") {
            ++directives; boot_target = trim(line.substr(equals + 1));
            std::transform(boot_target.begin(), boot_target.end(), boot_target.begin(), [](unsigned char c) { return char(std::toupper(c)); });
            std::replace(boot_target.begin(), boot_target.end(), '/', '\\');
        }
    }
    check(directives == 1 && boot_target == "CDROM0:\\SLUS_201.84;1", "Invalid BOOT2 target. Select the USA CVX disc (SLUS-20184).");
    check(hash_range(input, executable.offset, executable.size) == retail_sha1,
          "The executable does not match CVX USA. Select an unmodified USA ISO.");
    check(disc.files.size() == usa_inventory.size(), "Incomplete or modified USA game disc inventory.");
    for (const auto& expected : usa_inventory) {
        auto found = std::find_if(disc.files.begin(), disc.files.end(), [&](const DiscFile& file) { return file.name == expected.name; });
        check(found != disc.files.end() && found->size == expected.size, "A required USA game asset is missing or has the wrong size.");
    }
}
std::string sha1_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary); check(bool(input), "Cannot read file for validation.");
    return hash_range(input, 0, fs::file_size(path));
}
bool cache_valid(const fs::path& root) {
    try {
        std::ifstream input(root / "assets/manifest.txt"); std::string header, line;
        if (!std::getline(input, header) || header != "ASHFORD_CACHE_V1") return false;
        unsigned count = 0; std::set<std::string> names;
        while (std::getline(input, line)) {
            auto separator = line.find('\t'); if (separator == std::string::npos) return false;
            auto name = line.substr(0, separator); if (!safe_relative(name) || !names.insert(name).second || ++count > 10000) return false;
            auto text = line.substr(separator + 1); std::size_t used;
            auto size = std::stoull(text, &used); if (used != text.size()) return false;
            auto file = root / "assets" / fs::u8path(name);
            if (!fs::is_regular_file(file) || fs::file_size(file) != size) return false;
        }
        if (count != usa_inventory.size()) return false;
        for (const auto& expected : usa_inventory) {
            if (!names.count(expected.name) || fs::file_size(root / "assets" / fs::u8path(expected.name)) != expected.size) return false;
        }
        return sha1_file(root / "assets/SLUS_201.84") == retail_sha1;
    } catch (...) { return false; }
}
void recover_import(const fs::path& root) {
    auto live = root / "assets", previous = root / "assets.previous";
    if (!fs::exists(live) && fs::exists(previous)) fs::rename(previous, live);
}
void import_disc(const Disc& disc, const fs::path& root,
                 const std::function<void(float, const std::string&)>& progress) {
    validate_disc(disc); fs::create_directories(root); recover_import(root);
    auto stage = root / "assets.pending", live = root / "assets", old = root / "assets.previous";
    // All managed directories are fixed children of the user-data root.
    if (fs::exists(stage)) fs::remove_all(stage);
    fs::create_directories(stage);
    std::uint64_t total = 0, done = 0;
    for (const auto& file : disc.files) { check(safe_relative(file.name), "Unsafe import path."); total += file.size; }
    try {
        check(fs::space(root).available > total + 64 * 1024 * 1024, "Not enough space to import the disc. About 2 GB of free space is needed.");
        std::ifstream input(disc.path, std::ios::binary); std::vector<char> buffer(1024 * 1024);
        std::ofstream manifest(stage / "manifest.txt", std::ios::binary);
        manifest << "ASHFORD_CACHE_V1\n";
        for (const auto& file : disc.files) {
            auto target = stage / fs::u8path(file.name); fs::create_directories(target.parent_path());
            std::ofstream output(target, std::ios::binary); input.clear(); input.seekg(static_cast<std::streamoff>(file.offset));
            auto remaining = file.size;
            while (remaining) {
                auto amount = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, buffer.size()));
                input.read(buffer.data(), amount); check(input.gcount() == amount, "Disc read interrupted. The previous import is preserved.");
                output.write(buffer.data(), amount); check(bool(output), "Cannot write imported assets.");
                remaining -= amount; done += amount; progress(total ? float(double(done) / double(total)) : 1.0f, file.name);
            }
            output.flush(); check(bool(output), "Cannot finish imported file.");
            manifest << file.name << '\t' << file.size << '\n';
        }
        manifest.flush(); check(bool(manifest), "Cannot write import manifest."); manifest.close();
        check(sha1_file(stage / "SLUS_201.84") == retail_sha1, "Imported executable verification failed.");
        if (fs::exists(old)) fs::remove_all(old);
        if (fs::exists(live)) fs::rename(live, old);
        try { fs::rename(stage, live); }
        catch (...) { if (fs::exists(old) && !fs::exists(live)) fs::rename(old, live); throw; }
        std::error_code ignored; fs::remove_all(old, ignored);
    } catch (...) { std::error_code ignored; fs::remove_all(stage, ignored); throw; }
}
}
