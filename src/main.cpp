#include "core.hpp"
#include "text.hpp"
#include <SDL.h>
#include <SDL_syswm.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

using namespace ashford;
namespace {
constexpr SDL_Color gold {202, 174, 117, 255}, white {236, 234, 225, 255};
constexpr SDL_Color muted {143, 154, 162, 255}, dark {13, 18, 23, 255};
fs::path user_root() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &folder)))
        throw std::runtime_error("Cannot locate your Windows user-data folder.");
    fs::path path(folder); CoTaskMemFree(folder); return path / "AshfordsHalberd";
}
struct Instance {
    HANDLE handle = nullptr;
    explicit Instance(const fs::path& root) {
        auto name = L"Local\\AshfordsHalberd-" + std::to_wstring(std::hash<std::wstring>{}(fs::absolute(root).wstring()));
        handle = CreateMutexW(nullptr, FALSE, name.c_str());
        if (!handle) throw std::runtime_error("Cannot create the application instance guard.");
        if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(handle); handle = nullptr;
            throw std::runtime_error("Ashford's Halberd is already running for this user."); }
    }
    ~Instance() { if (handle) CloseHandle(handle); }
};
fs::path pick_iso(SDL_Window* window) {
    SDL_SysWMinfo info {}; SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) throw std::runtime_error(SDL_GetError());
    std::vector<wchar_t> filename(32768, L'\0');
    OPENFILENAMEW dialog {}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = info.info.win.window;
    dialog.lpstrFilter = L"CVX USA disc image (*.iso)\0*.iso\0\0";
    dialog.lpstrFile = filename.data(); dialog.nMaxFile = DWORD(filename.size());
    dialog.lpstrTitle = L"Select your Resident Evil: Code Veronica X USA ISO";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) {
        if (CommDlgExtendedError()) throw std::runtime_error("The Windows ISO file picker could not open.");
        return {};
    }
    return fs::path(filename.data());
}
void message(SDL_Window* window, const std::string& text) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ashford's Halberd", text.c_str(), window);
}
struct ImportJob {
    std::thread worker;
    std::mutex lock;
    std::atomic<bool> finished {false}, cancelled {false};
    std::atomic<float> progress {0};
    std::string status = "Checking your disc...", error;
    fs::path iso;
    ~ImportJob() { cancelled = true; if (worker.joinable()) worker.join(); }
    void start(fs::path selection, fs::path root) {
        iso = std::move(selection);
        worker = std::thread([this, root] {
            try {
                auto disc = read_disc(iso);
                import_disc(disc, root, [this](float value, const std::string& name) {
                    if (cancelled) throw std::runtime_error("Import cancelled. Your previous data is preserved.");
                    progress = value;
                    std::lock_guard<std::mutex> guard(lock); status = name;
                });
            } catch (const std::exception& exception) { error = exception.what(); }
            finished.store(true, std::memory_order_release);
        });
    }
    std::string description() { std::lock_guard<std::mutex> guard(lock); return status; }
};
void fill(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rectangle {x, y, w, h}; SDL_RenderFillRect(renderer, &rectangle);
}
void apply_display(SDL_Window* window, SDL_Renderer* renderer, const Settings& settings) {
    if (SDL_SetWindowFullscreen(window, 0) < 0) throw std::runtime_error(SDL_GetError());
    SDL_SetWindowSize(window, settings.width, settings.height);
    if (settings.mode == "borderless") {
        if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP) < 0) throw std::runtime_error(SDL_GetError());
    } else if (settings.mode == "fullscreen") {
        SDL_DisplayMode requested {}; requested.w = settings.width; requested.h = settings.height;
        requested.format = SDL_PIXELFORMAT_RGB888;
        SDL_DisplayMode available {};
        auto display = SDL_GetWindowDisplayIndex(window);
        if (!SDL_GetClosestDisplayMode(display, &requested, &available) || SDL_SetWindowDisplayMode(window, &available) < 0 ||
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) < 0) throw std::runtime_error("This fullscreen mode is not supported by the display.");
    }
    if (SDL_RenderSetVSync(renderer, settings.vsync ? 1 : 0) < 0)
        throw std::runtime_error("This renderer cannot change vertical sync.");
}
int run_app(const fs::path& root, bool smoke) {
    Instance instance(root);
    recover_import(root);
    auto settings_path = root / "AshfordsHalberd.ini";
    auto settings = load_settings(settings_path);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0) throw std::runtime_error(SDL_GetError());
    struct Quit { ~Quit() { SDL_Quit(); } } quit;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("Ashford's Halberd", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         settings.width, settings.height,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (smoke ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN)), SDL_DestroyWindow);
    if (!window) throw std::runtime_error(SDL_GetError());
    SDL_SetWindowMinimumSize(window.get(), 640, 480);
    unsigned flags = SDL_RENDERER_ACCELERATED | (settings.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateRenderer(window.get(), -1, flags), SDL_DestroyRenderer);
    if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
    if (!renderer) throw std::runtime_error(SDL_GetError());
    SDL_RenderSetLogicalSize(renderer.get(), 1120, 720);
    Text text(renderer.get());
    bool ready = cache_valid(root), running = true, first_launch = !ready && !smoke;
    int page = 0, selected = 0, frames = 0;
    std::string notice;
    std::unique_ptr<ImportJob> job;
    std::vector<SDL_GameController*> controllers;
    auto attach = [&](int index) { if (SDL_IsGameController(index)) { auto* pad = SDL_GameControllerOpen(index); if (pad) controllers.push_back(pad); } };
    for (int i = 0; i < SDL_NumJoysticks(); ++i) attach(i);
    struct Pads { std::vector<SDL_GameController*>& pads; ~Pads() { for (auto* pad : pads) SDL_GameControllerClose(pad); } } pads {controllers};
    if (!smoke) {
        try { apply_display(window.get(), renderer.get(), settings); }
        catch (const std::exception& error) { settings.mode = "windowed"; notice = error.what(); SDL_SetWindowFullscreen(window.get(), 0); }
    }
    auto choose = [&] {
        auto selection = pick_iso(window.get());
        if (selection.empty()) { if (!ready) running = false; return; }
        job = std::make_unique<ImportJob>(); job->start(selection, root); notice.clear();
    };
    auto persist = [&](Settings next) {
        auto previous = settings;
        try { apply_display(window.get(), renderer.get(), next); save_settings(settings_path, next); settings = next; notice = "Settings saved."; }
        catch (...) { try { apply_display(window.get(), renderer.get(), previous); } catch (...) {} throw; }
    };
    while (running) {
        if (job && job->finished.load(std::memory_order_acquire)) {
            job->worker.join();
            if (job->error.empty()) {
                settings.iso_path = job->iso.u8string(); ready = cache_valid(root);
                try { save_settings(settings_path, settings); notice = "Your USA game data is ready."; }
                catch (const std::exception& error) { message(window.get(), error.what()); notice = "Data imported; settings could not be saved."; }
            } else { ready = cache_valid(root); message(window.get(), job->error); first_launch = !ready; }
            job.reset();
        }
        auto activate = [&] {
            if (job) return;
            if (page == 0) {
                if (selected == 0) notice = "The playable room slice is the next development milestone.";
                if (selected == 1) { page = 1; selected = 0; notice.clear(); }
                else if (selected == 2) { page = 2; selected = 0; notice.clear(); }
                else if (selected == 3) running = false;
            } else if (page == 1) {
                auto next = settings;
                if (selected == 0) {
                    const int widths[] = {1120, 1280, 1600, 1920}; const int heights[] = {720, 720, 900, 1080};
                    int i = 0; while (i < 4 && widths[i] != next.width) ++i; i = i == 4 ? 0 : (i + 1) % 4;
                    next.width = widths[i]; next.height = heights[i]; persist(next);
                } else if (selected == 1) { next.mode = next.mode == "windowed" ? "borderless" : next.mode == "borderless" ? "fullscreen" : "windowed"; persist(next); }
                else if (selected == 2) { next.vsync = !next.vsync; persist(next); }
                else if (selected == 3) { page = 2; selected = 0; notice.clear(); }
                else { page = 0; selected = 1; notice.clear(); }
            } else {
                if (selected == 0) choose();
                else { page = 0; selected = 2; notice.clear(); }
            }
        };
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = false; if (job) job->cancelled = true; }
            if (event.type == SDL_CONTROLLERDEVICEADDED) attach(event.cdevice.which);
            if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                for (auto it = controllers.begin(); it != controllers.end();) {
                    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*it)) == event.cdevice.which) { SDL_GameControllerClose(*it); it = controllers.erase(it); } else ++it;
                }
            }
            bool up = event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w);
            bool down = event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s);
            bool confirm = event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE);
            bool back = event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE;
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                up |= event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP;
                down |= event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN;
                confirm |= event.cbutton.button == SDL_CONTROLLER_BUTTON_A;
                back |= event.cbutton.button == SDL_CONTROLLER_BUTTON_B;
            }
            if (!job) {
                int count = page == 0 ? 4 : page == 1 ? 5 : 2;
                if (up) selected = (selected + count - 1) % count;
                if (down) selected = (selected + 1) % count;
                if (back) { if (page) { page = 0; selected = 0; notice.clear(); } else running = false; }
                if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN) {
                    int rawx = event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x;
                    int rawy = event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y;
                    // SDL_RenderSetLogicalSize has already mapped mouse events to logical coordinates.
                    float x = float(rawx), y = float(rawy);
                    int item = int(y - 285) / 55;
                    if (x >= 70 && x <= 580 && y >= 285 && item >= 0 && item < count) {
                        selected = item; confirm |= event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT;
                    }
                }
                if (confirm) { try { activate(); } catch (const std::exception& error) { message(window.get(), error.what()); } }
            }
        }
        if (!running) break;
        auto* r = renderer.get();
        SDL_SetRenderDrawColor(r, dark.r, dark.g, dark.b, 255); SDL_RenderClear(r);
        for (int y = 0; y < 720; ++y) {
            SDL_SetRenderDrawColor(r, Uint8(13 + y / 160), Uint8(18 + y / 120), Uint8(23 + y / 90), 255);
            SDL_RenderDrawLine(r, 0, y, 1120, y);
        }
        fill(r, 68, 64, 35, 2, gold);
        text.draw("ASHFORD FAMILY", 116, 52, 15, gold, false, true);
        text.draw("Ashford's", 68, 102, 60, white, true);
        text.draw("Halberd", 68, 163, 60, white, true);
        text.draw("CODE VERONICA X  /  WINDOWS", 72, 243, 14, muted, false, true);
        std::vector<std::string> labels;
        if (page == 0) labels = {"New game", "Options", "Game data", "Quit"};
        else if (page == 1) labels = {
            "Resolution     " + std::to_string(settings.width) + " x " + std::to_string(settings.height),
            "Window mode     " + settings.mode,
            std::string("Vertical sync     ") + (settings.vsync ? "On" : "Off"), "Game data", "Back"};
        else labels = {"Choose / change ISO", "Back"};
        for (int i = 0; i < int(labels.size()); ++i) {
            int y = 285 + i * 55;
            if (i == selected && !job) { fill(r, 68, y, 512, 45, {36, 43, 46, 255}); fill(r, 68, y, 3, 45, gold); }
            bool disabled = page == 0 && i == 0;
            text.draw(labels[i], 88, y + 9, 21, disabled ? muted : i == selected ? gold : white);
            if (disabled) text.draw("IN DEVELOPMENT", 370, y + 14, 12, muted);
        }
        fill(r, 637, 116, 1, 450, {58, 64, 64, 255});
        text.draw(job ? "PREPARING YOUR GAME" : page == 1 ? "OPTIONS" : page == 2 ? "YOUR GAME DATA" : "WELCOME", 683, 125, 14, gold, false, true);
        if (job) {
            text.draw("Importing your disc", 683, 169, 29, white, true);
            text.draw("This only happens once. Keep your ISO connected until the import is finished.", 683, 223, 18, muted, false, false, 352);
            fill(r, 683, 352, 352, 3, {58, 64, 64, 255});
            fill(r, 683, 352, int(352 * job->progress.load()), 3, gold);
            text.draw(std::to_string(int(job->progress.load() * 100)) + "%", 683, 378, 28, white);
            text.draw(job->description(), 683, 433, 15, muted, false, false, 352);
        } else {
            text.draw(ready ? "Your disc. Your game." : "Bring your own disc.", 683, 169, 29, white, true);
            text.draw(ready ? "Your Code Veronica X USA assets are imported and verified. You can keep the original ISO wherever you prefer."
                            : "Choose your original Code Veronica X USA ISO to prepare this installation.",
                      683, 225, 18, muted, false, false, 352);
            fill(r, 683, 347, 352, 1, {58, 64, 64, 255});
            text.draw(ready ? "GAME DATA READY" : "ISO REQUIRED", 683, 372, 14, ready ? gold : muted, false, true);
            text.draw("SLUS-20184  /  USA", 683, 401, 17, white);
            text.draw("Development build 0.1\nPlayable rooms are the next milestone.", 683, 471, 17, muted, false, false, 352);
        }
        if (!notice.empty()) text.draw(notice, 72, 590, 16, gold, false, false, 950);
        fill(r, 68, 641, 967, 1, {58, 64, 64, 255});
        text.draw("ARROWS / D-PAD   Navigate       ENTER / A   Select       ESC / B   Back", 72, 667, 13, muted);
        text.draw("NATIVE WINDOWS BUILD", 837, 667, 12, muted);
        SDL_RenderPresent(r);
        if (first_launch && !job) { first_launch = false; try { choose(); } catch (const std::exception& error) { message(window.get(), error.what()); running = false; } }
        if (smoke && ++frames == 3) {
            save_settings(settings_path, settings);
            auto persisted = load_settings(settings_path);
            if (persisted.width != settings.width) throw std::runtime_error("Menu settings persistence failed.");
            std::ofstream(root / "smoke.txt") << "Native menu rendered; INI persisted.\n";
            running = false;
        }
        SDL_Delay(8);
    }
    return 0;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    int argc = 0; LPWSTR* raw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!raw) return 1;
    std::vector<std::wstring> args(raw, raw + argc); LocalFree(raw);
    fs::path root;
    bool command = false, smoke = false;
    try {
        root = user_root();
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == L"--data-dir") {
                if (++i >= args.size()) throw std::runtime_error("Missing --data-dir argument.");
                root = fs::absolute(fs::path(args[i]));
            }
            if (args[i] == L"--smoke-test") smoke = true;
            if (args[i] == L"--inspect-iso" || args[i] == L"--import-iso" || args[i] == L"--check-cache") command = true;
        }
        fs::create_directories(root);
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == L"--inspect-iso" || args[i] == L"--import-iso") {
                bool import = args[i] == L"--import-iso";
                if (++i >= args.size()) throw std::runtime_error("Missing ISO argument.");
                Instance instance(root);
                auto disc = read_disc(fs::path(args[i])); validate_disc(disc);
                if (import) {
                    import_disc(disc, root, [](float, const std::string&) {});
                    auto settings = load_settings(root / "AshfordsHalberd.ini");
                    settings.iso_path = disc.path.u8string(); save_settings(root / "AshfordsHalberd.ini", settings);
                }
                std::ofstream report(root / (import ? "import.txt" : "inspection.txt"));
                report << "Verified CVX USA\n" << retail_sha1 << '\n' << disc.files.size() << " files\n";
                for (const auto& file : disc.files) report << file.name << '\t' << file.size << '\n';
                return 0;
            }
            if (args[i] == L"--check-cache") { recover_import(root); return cache_valid(root) ? 0 : 1; }
        }
        return run_app(root, smoke);
    } catch (const std::exception& error) {
        if (!root.empty()) { std::ofstream log(root / "last-error.txt"); log << error.what() << '\n'; }
        if (!command && !smoke) MessageBoxW(nullptr, wide(error.what()).c_str(), L"Ashford's Halberd", MB_OK | MB_ICONERROR);
        std::cerr << error.what() << '\n'; return 1;
    }
}
