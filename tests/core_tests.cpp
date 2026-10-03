#include "core.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <windows.h>

using namespace ashford;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "expected test directory and fixture directory");
        // A failed run may leave an INI halfway through the persistence cases.
        // Each run gets its own diagnostic profile, independent of that state.
        fs::path root = fs::u8path(argv[1]) /
            ("run-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        fs::create_directories(root);
        auto ini = root / "settings.ini";
        auto defaults = load_settings(ini);
        require(defaults.width == 1120 && defaults.mode == "windowed", "missing INI defaults");
        defaults.iso_path = "C:/Games/CVX USA/日本語.iso";
        defaults.width = 1280; defaults.height = 720; defaults.vsync = false;
        defaults.viewer_room=87;defaults.viewer_cut=2;defaults.viewer_variant=1;
        save_settings(ini, defaults);
        auto restored = load_settings(ini);
        require(restored.iso_path == defaults.iso_path && restored.width == 1280 && !restored.vsync,
                "settings must survive restart, including Unicode paths");
        require(restored.viewer_room==87 && restored.viewer_cut==2 && restored.viewer_variant==1,"viewer selection persistence");
        restored.width = 1920; save_settings(ini, restored);
        require(fs::exists(ini.string() + ".bak"), "settings backup");
        auto locked_settings = restored;
        locked_settings.width = 1600;
        HANDLE settings_handle = CreateFileW(ini.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        require(settings_handle != INVALID_HANDLE_VALUE, "open settings sharing-lock fixture");
        std::thread release_settings([settings_handle] {
            Sleep(150);
            CloseHandle(settings_handle);
        });
        std::exception_ptr save_error;
        try { save_settings(ini, locked_settings); }
        catch (...) { save_error = std::current_exception(); }
        release_settings.join();
        if (save_error) std::rethrow_exception(save_error);
        require(load_settings(ini).width == 1600,
                "settings must persist after a short Windows sharing lock");
        require(load_settings(fs::path(ini.wstring() + L".bak")).width == 1920,
                "sharing retry preserves the previous settings backup");
        settings_handle = CreateFileW(ini.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        require(settings_handle != INVALID_HANDLE_VALUE, "open persistent settings lock");
        auto blocked_settings = locked_settings;
        blocked_settings.width = 1700;
        const auto lock_started = GetTickCount64();
        save_error = nullptr;
        try { save_settings(ini, blocked_settings); }
        catch (...) { save_error = std::current_exception(); }
        CloseHandle(settings_handle);
        const auto lock_elapsed = GetTickCount64() - lock_started;
        require(save_error && lock_elapsed >= 400 && lock_elapsed < 5000,
                "persistent sharing lock must fail within the bounded retry");
        require(load_settings(ini).width == 1600,
                "failed sharing retry preserves original settings");
        { std::ofstream file(ini); file << "[Display]\nwidth=oops\nheight=999999\nmode=invalid\nvsync=0\n[GameData]\niso_path=C:/good.iso\n[Viewer]\nroom=-1\ncut=999999\nvariant=7\n"; }
        auto partial = load_settings(ini);
        require(partial.width == 1120 && partial.height == 720 && partial.mode == "windowed" &&
                !partial.vsync && partial.iso_path == "C:/good.iso", "invalid keys must preserve valid keys");
        require(partial.viewer_room==0 && partial.viewer_cut==0 && partial.viewer_variant==0,"invalid viewer settings must default safely");
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
        auto sharing_recovery = root / "sharing-recovery";
        fs::create_directories(sharing_recovery / "assets.previous");
        { std::ofstream marker(sharing_recovery / "assets.previous/marker.txt"); marker << "recover after sharing lock"; }
        HANDLE recovery_handle = CreateFileW((sharing_recovery / "assets.previous").c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        require(recovery_handle != INVALID_HANDLE_VALUE, "open asset directory sharing-lock fixture");
        std::thread release_recovery([recovery_handle] {
            Sleep(150);
            CloseHandle(recovery_handle);
        });
        std::exception_ptr recovery_error;
        try { recover_import(sharing_recovery); }
        catch (...) { recovery_error = std::current_exception(); }
        release_recovery.join();
        require(!recovery_error, "asset recovery must survive a short Windows directory sharing lock");
        require(content(sharing_recovery / "assets/marker.txt") == "recover after sharing lock" &&
                !fs::exists(sharing_recovery / "assets.previous"), "sharing retry must preserve restored asset data");
        auto blocked_recovery = root / "blocked-recovery";
        fs::create_directories(blocked_recovery / "assets.previous");
        { std::ofstream marker(blocked_recovery / "assets.previous/marker.txt"); marker << "preserve blocked cache"; }
        recovery_handle = CreateFileW((blocked_recovery / "assets.previous").c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        require(recovery_handle != INVALID_HANDLE_VALUE, "open persistent asset directory lock");
        const auto recovery_started = GetTickCount64();
        recovery_error = nullptr;
        try { recover_import(blocked_recovery); }
        catch (...) { recovery_error = std::current_exception(); }
        const auto recovery_elapsed = GetTickCount64() - recovery_started;
        CloseHandle(recovery_handle);
        require(recovery_error && recovery_elapsed >= 400 && recovery_elapsed < 5000,
                "persistent asset sharing lock must fail within bounded retry");
        require(!fs::exists(blocked_recovery / "assets") &&
                content(blocked_recovery / "assets.previous/marker.txt") == "preserve blocked cache",
                "blocked recovery must preserve previous assets");
        recover_import(blocked_recovery);
        require(content(blocked_recovery / "assets/marker.txt") == "preserve blocked cache",
                "asset recovery must remain possible after persistent lock releases");
        auto live_recovery = root / "live-recovery";
        fs::create_directories(live_recovery / "assets");
        fs::create_directories(live_recovery / "assets.previous");
        { std::ofstream marker(live_recovery / "assets/marker.txt"); marker << "current cache"; }
        { std::ofstream marker(live_recovery / "assets.previous/marker.txt"); marker << "previous cache"; }
        recover_import(live_recovery);
        require(content(live_recovery / "assets/marker.txt") == "current cache" &&
                content(live_recovery / "assets.previous/marker.txt") == "previous cache",
                "recovery must not replace an existing live cache");
        std::cout << "Settings and malformed-disc tests passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
