#include "core.hpp"
#include "text.hpp"
#include "viewer.hpp"
#include "runtime_view.hpp"
#include <array>
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
int run_app(const fs::path& root, bool smoke,bool scene_smoke=false,const fs::path& archive_override={},bool runtime_smoke=false,const fs::path& system_override={},bool options_smoke=false) {
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
    bool ready = cache_valid(root)||scene_smoke||runtime_smoke, running = true, first_launch = !ready && !smoke;
    int page = options_smoke?1:runtime_smoke?4:scene_smoke?3:0, selected = options_smoke?3:0, frames = 0;
    if(options_smoke){SDL_Event enter{};enter.type=SDL_KEYDOWN;enter.key.keysym.sym=SDLK_RETURN;SDL_PushEvent(&enter);}
    std::unique_ptr<RuntimeView> native;
    std::array<bool,SDL_NUM_SCANCODES> runtime_keys{};
    uint64_t runtime_clock=SDL_GetPerformanceCounter();
    double runtime_accumulator=0;
    size_t runtime_movement=0;
    bool runtime_focused=true;
    size_t runtime_focus_blocked=0;
    auto open_native=[&]{
        native=std::make_unique<RuntimeView>(renderer.get(),archive_override.empty()?root/"assets/RDX_LNK.AFS":archive_override,system_override.empty()?root/"assets/SYSTEM.AFS":system_override,settings.effects_volume);
        runtime_keys.fill(false);runtime_focused=true;runtime_clock=SDL_GetPerformanceCounter();runtime_accumulator=0;frames=0;runtime_movement=0;
    };
    if(runtime_smoke)open_native();
    std::unique_ptr<RoomViewer> viewer;
    auto open_viewer=[&]{
        viewer=std::make_unique<RoomViewer>(renderer.get(),archive_override.empty()?root/"assets/RDX_LNK.AFS":archive_override);
        auto next=settings;viewer->select(next);save_settings(settings_path,next);settings=next;
    };
    if(scene_smoke)open_viewer();
    auto trace_viewer=[&]{if(scene_smoke)std::ofstream(root/"scene-navigation.tsv",std::ios::app)<<settings.viewer_room<<'\t'<<settings.viewer_cut<<'\t'<<settings.viewer_variant<<'\n';};
    if(scene_smoke){std::ofstream(root/"scene-navigation.tsv")<<"room\tcut\tvariant\n";trace_viewer();}
    std::string notice;
    std::unique_ptr<ImportJob> job;
    std::vector<SDL_GameController*> controllers;
    auto attach = [&](int index) { if (SDL_IsGameController(index)) {auto id=SDL_JoystickGetDeviceInstanceID(index);for(auto* existing:controllers)if(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(existing))==id)return; auto* pad = SDL_GameControllerOpen(index); if (pad) controllers.push_back(pad); } };
    for (int i = 0; i < SDL_NumJoysticks(); ++i) attach(i);
    struct VirtualPad {
        int index=-1;SDL_Joystick* joystick=nullptr;
        ~VirtualPad(){if(joystick)SDL_JoystickClose(joystick);if(index>=0)SDL_JoystickDetachVirtual(index);}
    } virtual_pad;
    if(runtime_smoke){
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        virtual_pad.index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
        if(virtual_pad.index<0)throw std::runtime_error(SDL_GetError());
        virtual_pad.joystick=SDL_JoystickOpen(virtual_pad.index);
        if(!virtual_pad.joystick || !SDL_IsGameController(virtual_pad.index))throw std::runtime_error("Could not create headless controller");
        attach(virtual_pad.index);
    }
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
        const bool display_changed=next.width!=previous.width || next.height!=previous.height || next.mode!=previous.mode || next.vsync!=previous.vsync;
        try { if(display_changed)apply_display(window.get(), renderer.get(), next); save_settings(settings_path, next); settings = next;if(native)native->set_effects_volume(next.effects_volume); notice = "Settings saved."; }
        catch (...) { if(display_changed)try { apply_display(window.get(), renderer.get(), previous); } catch (...) {} throw; }
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
                if (selected == 1) { if(!ready){notice="Import your ISO in Game data first.";return;}open_viewer();page=3;notice.clear(); }
                else if (selected == 2) { page = 1; selected = 0; notice.clear(); }
                else if (selected == 3) { page = 2; selected = 0; notice.clear(); }
                else if (selected == 4) {if(!ready){notice="Import your ISO in Game data first.";return;}open_native();page=4;notice.clear();}
                else if (selected == 5) running = false;
            } else if (page == 1) {
                auto next = settings;
                if (selected == 0) {
                    const int widths[] = {1120, 1280, 1600, 1920}; const int heights[] = {720, 720, 900, 1080};
                    int i = 0; while (i < 4 && widths[i] != next.width) ++i; i = i == 4 ? 0 : (i + 1) % 4;
                    next.width = widths[i]; next.height = heights[i]; persist(next);
                } else if (selected == 1) { next.mode = next.mode == "windowed" ? "borderless" : next.mode == "borderless" ? "fullscreen" : "windowed"; persist(next); }
                else if (selected == 2) { next.vsync = !next.vsync; persist(next); }
                else if (selected == 3) { next.effects_volume=((next.effects_volume/10+1)%11)*10;persist(next); }
                else if (selected == 4) { page = 2; selected = 0; notice.clear(); }
                else { page = 0; selected = 2; notice.clear(); }
            } else {
                if (selected == 0) choose();
                else { page = 0; selected = 3; notice.clear(); }
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
                if(page==4){
                    if(back){native->set_audio_running(false);page=0;selected=4;runtime_keys.fill(false);notice.clear();continue;}
                    if(event.type==SDL_WINDOWEVENT && (event.window.event==SDL_WINDOWEVENT_FOCUS_LOST || event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)){runtime_focused=event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED;native->set_audio_running(runtime_focused);runtime_keys.fill(false);}
                    if(runtime_focused && (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP)){auto scancode=event.key.keysym.scancode;if(scancode>=0 && scancode<SDL_NUM_SCANCODES)runtime_keys[size_t(scancode)]=event.type==SDL_KEYDOWN;}
                    continue;
                }
                if(page==3){
                    if(back){page=0;selected=1;notice.clear();continue;}
                    bool left=event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_LEFT;
                    bool right=event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_RIGHT;
                    if(event.type==SDL_CONTROLLERBUTTONDOWN){left|=event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT;right|=event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT;}
                    if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT && event.button.y>=660){
                        if(event.button.x<320)down=true;
                        else if(event.button.x<610)right=true;
                        else if(event.button.x<840)confirm=true;
                        else {page=0;selected=1;continue;}
                    }
                    if(up||down||left||right||confirm){
                        auto next=settings;
                        if(up||down){next.viewer_room=(next.viewer_room+int(viewer->room_count())+(down?1:-1))%int(viewer->room_count());next.viewer_cut=0;}
                        if((left||right)&&viewer->cut_count())next.viewer_cut=(next.viewer_cut+int(viewer->cut_count())+(right?1:-1))%int(viewer->cut_count());
                        if(confirm)next.viewer_variant=1-next.viewer_variant;
                        try{viewer->select(next);save_settings(settings_path,next);settings=next;trace_viewer();}
                        catch(const std::exception& error){if(scene_smoke)throw;try{viewer->select(settings);}catch(...){}notice=error.what();}
                    }
                    continue;
                }
                int count = page == 0 ? 6 : page == 1 ? 6 : 2;
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
                if (confirm) { try { activate(); } catch (const std::exception& error) { if(smoke)throw;message(window.get(), error.what()); } }
            }
        }
        if (!running) break;
        auto* r = renderer.get();
        SDL_SetRenderDrawColor(r, dark.r, dark.g, dark.b, 255); SDL_RenderClear(r);
        if(page==4){
            uint16_t pad=0;
            if(runtime_keys[SDL_SCANCODE_W]||runtime_keys[SDL_SCANCODE_UP])pad|=1;
            if(runtime_keys[SDL_SCANCODE_S]||runtime_keys[SDL_SCANCODE_DOWN])pad|=2;
            if(runtime_keys[SDL_SCANCODE_A]||runtime_keys[SDL_SCANCODE_LEFT])pad|=4;
            if(runtime_keys[SDL_SCANCODE_D]||runtime_keys[SDL_SCANCODE_RIGHT])pad|=8;
            if(runtime_keys[SDL_SCANCODE_LSHIFT]||runtime_keys[SDL_SCANCODE_RSHIFT])pad|=0x400;
            for(auto* controller:controllers){
                auto x=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX),y=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY);
                if(y< -8000 || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_UP))pad|=1;
                if(y>8000 || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_DOWN))pad|=2;
                if(x< -8000 || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_LEFT))pad|=4;
                if(x>8000 || SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_RIGHT))pad|=8;
                if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_X))pad|=0x400;
            }
            if(!runtime_focused){pad=0;++runtime_focus_blocked;}
            auto now=SDL_GetPerformanceCounter();
            runtime_accumulator+=std::min(.25,double(now-runtime_clock)/SDL_GetPerformanceFrequency());runtime_clock=now;
            size_t steps=runtime_smoke?1:std::min(size_t(8),size_t(runtime_accumulator*60));
            try {
                for(size_t step=0;step<steps;++step){auto before=native->state().world.player.position;native->tick(pad);if(runtime_smoke && !runtime_focused && (native->state().scenario.flags[6][0]&1) && native->state().world.player.previous_pad)throw std::runtime_error("Controller input continued after focus loss");auto after=native->state().world.player.position;if(before.x!=after.x || before.z!=after.z)++runtime_movement;}
                if(!runtime_smoke)runtime_accumulator-=double(steps)/60;
                SDL_Rect image{160,55,800,600};native->draw(image);
            }catch(const std::exception& error){native->set_audio_running(false);if(runtime_smoke)throw;page=0;selected=4;notice=error.what();continue;}
            text.draw("CORRIDOR PREVIEW",68,18,16,gold);
            text.draw("WASD/Arrows or left stick: Move   SHIFT/X: Run   ESC/B: Back",68,673,15,white);
            text.draw("Movement preview / combat, interactions and saves are in development",170,631,13,gold);
            SDL_RenderPresent(r);++frames;
            if(runtime_smoke && (frames==180 || frames==240)){
                if(frames==180 && SDL_JoystickSetVirtualAxis(virtual_pad.joystick,SDL_CONTROLLER_AXIS_LEFTY,-32768)<0)throw std::runtime_error(SDL_GetError());
                SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=frames==180?SDLK_w:SDLK_ESCAPE;e.key.keysym.scancode=frames==180?SDL_SCANCODE_W:SDL_SCANCODE_ESCAPE;SDL_PushEvent(&e);
            }
            if(runtime_smoke && (frames==200 || frames==210)){SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=frames==200?SDL_WINDOWEVENT_FOCUS_LOST:SDL_WINDOWEVENT_FOCUS_GAINED;SDL_PushEvent(&e);}
            SDL_Delay(runtime_smoke?0:1);continue;
        }
        if(page==3){
            SDL_Rect image{160,55,800,600};viewer->draw(image);
            text.draw("ROOM VIEWER  /  DEVELOPMENT BUILD",68,18,16,gold);
            std::string label="Room "+std::to_string(settings.viewer_room+1)+" / "+std::to_string(viewer->room_count())+"  Camera "+std::to_string(settings.viewer_cut+1)+"  View "+std::to_string(settings.viewer_variant+1);
            text.draw(label,640,18,15,white);
            text.draw("UP/DOWN: Room",68,673,15,white);text.draw("LEFT/RIGHT: Camera",330,673,15,white);
            text.draw("ENTER/A: Variant",620,673,15,white);text.draw("ESC/B: Back",870,673,15,white);
            if(!notice.empty())text.draw(notice,170,600,15,gold,false,false,770);
            text.draw(viewer->authored()?"Original camera snapshot / gameplay is not implemented":"Overview / this selection has no original camera",170,631,13,gold);
            if(scene_smoke && ++frames==8){
                int w=0,h=0;SDL_GetRendererOutputSize(r,&w,&h);std::vector<uint8_t> pixels(size_t(w)*h*3);
                if(SDL_RenderReadPixels(r,nullptr,SDL_PIXELFORMAT_RGB24,pixels.data(),w*3)<0)throw std::runtime_error(SDL_GetError());
                size_t red=0;for(size_t i=0;i<pixels.size();i+=3)if(pixels[i]>80 && pixels[i+1]<10 && pixels[i+2]<10)++red;
                save_settings(settings_path,settings);auto saved=load_settings(settings_path);
                if(saved.viewer_room!=settings.viewer_room || saved.viewer_cut!=settings.viewer_cut || saved.viewer_variant!=settings.viewer_variant)throw std::runtime_error("Viewer selection persistence failed.");
                std::ofstream(root/"scene-smoke.txt")<<"Native scene displayed; selection persisted.\nred_pixels="<<red<<"\n";
            }
            SDL_RenderPresent(r);
            if(scene_smoke && frames>=1 && frames<=8){
                const SDL_Keycode steps[]={SDLK_RETURN,SDLK_RIGHT,SDLK_RIGHT,SDLK_DOWN,SDLK_DOWN,SDLK_UP,SDLK_UP,SDLK_ESCAPE};
                SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=steps[frames-1];SDL_PushEvent(&e);
            }
            SDL_Delay(8);continue;
        }
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
        if (page == 0) labels = {"New game", "Room viewer", "Options", "Game data", "Corridor preview", "Quit"};
        else if (page == 1) labels = {
            "Resolution     " + std::to_string(settings.width) + " x " + std::to_string(settings.height),
            "Window mode     " + settings.mode,
            std::string("Vertical sync     ") + (settings.vsync ? "On" : "Off"),
            "Effects volume     " + std::to_string(settings.effects_volume) + "%", "Game data", "Back"};
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
            text.draw("Development build 0.2\nRoom viewer available. Gameplay is in development.", 683, 471, 17, muted, false, false, 352);
        }
        if (!notice.empty()) text.draw(notice, 72, 620, 16, gold, false, false, 950);
        fill(r, 68, 641, 967, 1, {58, 64, 64, 255});
        text.draw("ARROWS / D-PAD   Navigate       ENTER / A   Select       ESC / B   Back", 72, 667, 13, muted);
        text.draw("NATIVE WINDOWS BUILD", 837, 667, 12, muted);
        SDL_RenderPresent(r);
        if (first_launch && !job) { first_launch = false; try { choose(); } catch (const std::exception& error) { message(window.get(), error.what()); running = false; } }
        if(runtime_smoke){if(frames!=240 || !runtime_movement || runtime_focus_blocked!=10 || native->audio_queue_frames()!=0)throw std::runtime_error("Native runtime input/audio smoke failed");save_settings(settings_path,settings);std::ofstream(root/"runtime-smoke.txt")<<"ticks="<<native->ticks()<<"\nrendered_frames="<<frames<<"\nmovement_ticks="<<runtime_movement<<"\nfocus_blocked_ticks="<<runtime_focus_blocked<<"\nmenu_return=1\naudio_frames="<<native->audio_frames()<<"\naudio_submitted_frames="<<native->audio_submitted_frames()<<"\naudio_queue_frames="<<native->audio_queue_frames()<<"\neffects_volume="<<settings.effects_volume<<"\n";running=false;}
        else if(scene_smoke){std::ofstream(root/"scene-menu-return.txt")<<"Returned to main menu.\n";running=false;}
        else if (smoke && ++frames == 3) {
            save_settings(settings_path, settings);
            auto persisted = load_settings(settings_path);
            if (persisted.width != settings.width) throw std::runtime_error("Menu settings persistence failed.");
            std::ofstream(root / "smoke.txt") << "Native menu rendered; INI persisted.\n";
            if(options_smoke){if(persisted.effects_volume!=settings.effects_volume || page!=0)throw std::runtime_error("Effects Options action/menu return failed");std::ofstream(root/"audio-options-smoke.txt")<<"effects_volume="<<settings.effects_volume<<"\nmenu_return=1\n";}
            running = false;
        }
        if(options_smoke && frames==1){SDL_Event back{};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;SDL_PushEvent(&back);}
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
    bool command = false, smoke = false,scene_smoke=false,runtime_smoke=false,options_smoke=false;
    fs::path archive_override,system_override;
    try {
        root = user_root();
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == L"--data-dir") {
                if (++i >= args.size()) throw std::runtime_error("Missing --data-dir argument.");
                root = fs::absolute(fs::path(args[i]));
            }
            if (args[i] == L"--smoke-test") smoke = true;
            if(args[i]==L"--audio-options-smoke-test"){options_smoke=true;smoke=true;command=true;}
            if(args[i]==L"--scene-smoke-test"){scene_smoke=true;smoke=true;command=true;}
            if(args[i]==L"--runtime-smoke-test"){runtime_smoke=true;smoke=true;command=true;}
            if(args[i]==L"--system"){if(++i>=args.size())throw std::runtime_error("Missing system archive argument.");system_override=fs::absolute(fs::path(args[i]));}
            if(args[i]==L"--archive"){if(++i>=args.size())throw std::runtime_error("Missing archive argument.");archive_override=fs::absolute(fs::path(args[i]));}
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
        return run_app(root, smoke,scene_smoke,archive_override,runtime_smoke,system_override,options_smoke);
    } catch (const std::exception& error) {
        if (!root.empty()) { std::ofstream log(root / "last-error.txt"); log << error.what() << '\n'; }
        if (!command && !smoke) MessageBoxW(nullptr, wide(error.what()).c_str(), L"Ashford's Halberd", MB_OK | MB_ICONERROR);
        std::cerr << error.what() << '\n'; return 1;
    }
}
