#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <atomic>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include "model.hpp"
#include "../jobs/job_queue.hpp"
#include "../platform/process.hpp"
#include "../input/action.hpp"
#include "../render/sound.hpp"
#include "../ui/engine/engine.hpp"

#ifdef FOYER_EVDEV
#include "../input/evdev_global.hpp"
#include "../input/airmouse.hpp"
#endif
#ifdef FOYER_CEC
#include "../input/cec.hpp"
#endif

namespace Foyer {
    std::shared_ptr<spdlog::logger> init_logging_global();

    struct Options {
        std::string config_dir;          // -c dir (or legacy config.ini path -> its dir)
        std::string legacy_layout;       // -l layout.xml (import source)
        std::string legacy_config;       // config.ini (import source)
        bool overlay = false;            // --overlay (quick panel over a running app)
        bool windowed = false;           // --window
        int width = 0, height = 0;       // --size WxH (window / offscreen)
        std::string screenshot;          // --screenshot out.png: render, save, exit
        std::string open;                // --open :settings (initial command)
        std::string keys;                // --keys "right,down,accept" (screenshot scripting)
        bool debug = false;
    };

    // Live system status shown in the top bar.
    struct Status {
        std::string net_kind;    // "ethernet" | "wifi" | "offline"
        std::string net_text;
        int volume = -1;
        bool muted = false;
        bool bt = false;
    };

    // Owns services, the window, and the UI engine. Screens are ui::Layers
    // that get App& at construction.
    class App {
    public:
        explicit App(const Options &o);
        ~App();
        int run();

        // services
        ui::Engine& ui() { return *engine_; }
        JobQueue& jobs() { return *jobs_; }
        const Options& options() const { return opt_; }
        Settings& settings() { return settings_; }
        Layout& layout() { return layout_; }
        std::map<std::string, AppEntry>& apps() { return apps_; }
        const Status& status() const { return status_; }
        const std::string& theme_dir() const { return theme_dir_; }
        bool is_overlay() const { return opt_.overlay; }

        // persistence (write + apply)
        void save_settings();
        void save_layout();
        void save_app(const AppEntry &a);
        void reload_apps();
        void apply_appearance();        // re-load theme tokens + metrics (live)

        // air mouse button activity (for the remap page live highlight)
        struct AirActivity { int code; bool pressed; };
        void drain_air_activity(std::vector<AirActivity> &out);
        std::optional<std::string> airmouse_device_name() const;
        Action air_action(int code) const;

        // actions
        void execute(const std::string &command, const std::string &title = "");
        void launch(const AppEntry &a);
        void close_app();
        bool app_running() const { return app_proc_.alive(); }
        const std::string& running_title() const { return app_title_; }
        void quit() { quit_ = true; }
        void toast(const std::string &msg, bool error = false);
        void open_settings(const std::string &page = "");
        void open_quick_panel();
        void refresh_status();

        // helpers for screens
        std::string resolve_art(const std::string &p) const;   // data-dir relative -> abs
        std::string icon(const std::string &name) const;        // ui icon path
        void play_click();
        void play_select();

        static uint32_t evt_global();   // SDL_RegisterEvents (evdev/CEC -> main thread)
        static uint32_t evt_airmouse(); // SDL_RegisterEvents (air mouse -> main thread)

    private:
        Options opt_;
        Settings settings_;
        Layout layout_;
        std::map<std::string, AppEntry> apps_;
        std::string theme_dir_;
        Status status_;

        SDL_Window *window_ = nullptr;
        SDL_Renderer *renderer_ = nullptr;
        std::unique_ptr<ui::Engine> engine_;
        std::unique_ptr<JobQueue> jobs_;
        std::unique_ptr<Sound> sound_;
        std::thread sound_init_;
        std::atomic<bool> sound_ready_{false};
        Process app_proc_;
        Process overlay_proc_;
        std::string app_title_, app_command_, app_log_;
        Uint64 app_started_ = 0;
        bool quit_ = false;
        Uint64 last_input_ = 0, last_status_ = 0, last_frame_ = 0;
        bool screensaver_ = false;
        int frames_ = 0;
#ifdef FOYER_EVDEV
        std::unique_ptr<EvdevGlobal> evdev_;
        std::unique_ptr<Airmouse> airmouse_;
        std::vector<AirActivity> air_pending_;
        void on_airmouse(int code, bool pressed);
        Action resolve_air_action(int code) const;
#endif
#ifdef FOYER_CEC
        std::unique_ptr<Cec> cec_;
#endif
        void init_video();
        void load_data();
        void build_root();
        void handle(SDL_Event &e);
        void dispatch(Action a);
        void on_global(Action a);
        void poll_app();
        void report_exit(int code);
        void update_metrics();
        std::string legacy_import();
    };
}
