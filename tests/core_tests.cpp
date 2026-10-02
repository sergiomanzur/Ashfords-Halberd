#include "core.hpp"
#include <fstream>
#include <iostream>

using namespace ashford;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "expected test directory and fixture directory");
        fs::path root = fs::u8path(argv[1]);
        fs::create_directories(root);
        auto ini = root / "settings.ini";
        auto defaults = load_settings(ini);
        require(defaults.width == 1120 && defaults.mode == "windowed", "missing INI defaults");
        defaults.iso_path = "C:/Games/CVX USA/日本語.iso";
        defaults.width = 1280; defaults.height = 720; defaults.vsync = false;
        save_settings(ini, defaults);
        auto restored = load_settings(ini);
        require(restored.iso_path == defaults.iso_path && restored.width == 1280 && !restored.vsync,
                "settings must survive restart, including Unicode paths");
        restored.width = 1920; save_settings(ini, restored);
        require(fs::exists(ini.string() + ".bak"), "settings backup");
        { std::ofstream file(ini); file << "[Display]\nwidth=oops\nheight=999999\nmode=invalid\nvsync=0\n[GameData]\niso_path=C:/good.iso\n"; }
        auto partial = load_settings(ini);
        require(partial.width == 1120 && partial.height == 720 && partial.mode == "windowed" &&
                !partial.vsync && partial.iso_path == "C:/good.iso", "invalid keys must preserve valid keys");
        { std::ofstream file(ini, std::ios::app); file << "broken section [\n"; }
        load_settings(ini);
        require(fs::exists(ini.wstring() + L".damaged"), "damaged INI preserved");
        auto backup_path = fs::path(ini.wstring() + L".bak");
        auto content = [](const fs::path& file) { std::ifstream input(file); return std::string(std::istreambuf_iterator<char>(input), {}); };
        auto good_backup = content(backup_path);
        save_settings(ini, partial);
        require(content(backup_path) == good_backup, "corrupt INI must not replace the last-known-good backup");
        auto fixture = fs::u8path(argv[2]);
        auto fake = read_disc(fixture / "wrong_hash.iso");
        require(fake.files.size() == 3, "ISO file inventory");
        rejects([&] { validate_disc(fake); }, "wrong executable hash must be rejected");
        rejects([&] { validate_disc(read_disc(fixture / "wrong_region.iso")); }, "wrong region");
        try { validate_disc(read_disc(fixture / "wrong_boot.iso")); throw std::runtime_error("wrong boot accepted"); }
        catch (const std::exception& error) { require(std::string(error.what()).find("BOOT2") != std::string::npos,
            "validate boot directive before hash, not an identifier in a comment"); }
        rejects([&] { read_disc(fixture / "truncated.iso"); }, "truncated ISO");
        rejects([&] { read_disc(fixture / "unsafe.iso"); }, "unsafe names");
        rejects([&] { read_disc(fixture / "extent.iso"); }, "out of bounds extent");
        rejects([&] { read_disc(fixture / "cycle.iso"); }, "directory cycle");
        require(!cache_valid(root / "absent"), "missing cache must not be accepted");
        auto recovery = root / "interrupted";
        fs::create_directories(recovery / "assets.previous");
        { std::ofstream marker(recovery / "assets.previous/marker.txt"); marker << "last known good"; }
        recover_import(recovery);
        require(fs::exists(recovery / "assets/marker.txt"), "interrupted cache swap must restore previous data");
        std::cout << "Settings and malformed-disc tests passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
