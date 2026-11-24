#pragma once

#include <string>
#include <memory>

#include <SDL3/SDL.h>

#include <memory>
#include <spdlog/spdlog.h>

#include "config.hpp"
#include "layout_model.hpp"
#include "../render/renderer_sdl.hpp"
#include "../render/theme.hpp"
#include "../render/sound.hpp"
#include "../ui/screen.hpp"
#include "../ui/anim.hpp"
#include "../jobs/job_queue.hpp"
#include "../platform/process.hpp"
#include "../input/action.hpp"

#ifdef BL_EVDEV
#include "../input/evdev_global.hpp"
#endif
#ifdef BL_CEC
#include "../input/cec.hpp"
#endif

namespace BL {
    // Initialize logging (file + console). Returns the default logger.
    std::shared_ptr<spdlog::logger> init_logging_global();

    // Central application object. Owns all services, runs the main loop, maps
    // input to actions and dispatches them to the top screen. No globals:
    // screens receive App& and use it to access services.
    class App {
    public:
        App(std::string config_path, std::string layout_path);
        ~App();

        int run();

        // Services (for screens)
        Renderer& renderer() { return *renderer_; }
        Theme& theme() { return theme_; }
        Config& config() { return config_; }
        LayoutModel& layout() { return layout_; }
        JobQueue& jobs() { return *jobs_; }
        Anim& anim() { return anim_; }
        int screen_w() const { return screen_w_; }
        int screen_h() const { return screen_h_; }
        Sound* sound() { return sound_.get(); }
        ScreenStack& screens() { return screens_; }

        // Command execution (from tiles / hotkeys / quick menu)
        void execute_command(const std::string &command);

        // Feedback
        void play_click();
        void play_select();
        void show_toast(const std::string &msg);

        // Persistence
        void save_config() { config_.write(config_path_); }
        void save_layout() { layout_.write(layout_path_); }
        const std::string& config_path() const { return config_path_; }
        const std::string& layout_path() const { return layout_path_; }

        // App (launched application) lifecycle
        bool app_running() const { return app_proc_.alive(); }
        void close_app();            // SIGTERM the running app
        void restart_ui();           // :quit — restart the launcher

        // Global (non-grabbing) input while an app is focused
        void on_global_action(Action action);

        // Rendering
        void mark_dirty() { dirty_ = true; }

    private:
        void init_display();
        void create_window();
        void handle_event(SDL_Event &event);
        void dispatch_action(Action action);
        void handle_key(SDL_Event &event);
        void handle_gamepad(SDL_Event &event);
        void wake_screensaver();
        void update_app_state();
        void draw();
        void build_home();

        // Services
        std::unique_ptr<RendererSDL> renderer_;
        Theme theme_;
        Config config_;
        LayoutModel layout_;
        std::string config_path_;
        std::string layout_path_;
        std::unique_ptr<JobQueue> jobs_;
        std::unique_ptr<Sound> sound_;
        ScreenStack screens_;
        Anim anim_;
        Process app_proc_;

        SDL_Window *window_ = nullptr;
        const SDL_DisplayMode *dm_ = nullptr;
        int screen_w_ = 0, screen_h_ = 0;
        bool letterbox_ = false;
        bool quit_ = false;
        bool dirty_ = true;
        bool screensaver_active_ = false;

        Uint64 last_input_ = 0;
        Uint64 last_clock_tick_ = 0;
        std::string last_clock_string_;

        // Toast
        std::string toast_msg_;
        Uint64 toast_until_ = 0;

        // Global input
#ifdef BL_EVDEV
        std::unique_ptr<EvdevGlobal> evdev_;
#endif
#ifdef BL_CEC
        std::unique_ptr<Cec> cec_;
#endif

        void apply_theme();
    };
}
