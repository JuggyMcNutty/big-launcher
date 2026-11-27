#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>

#include "kit.hpp"
#include "../platform/sysctl.hpp"

namespace Foyer {
    class App;

    // Settings tab content (grid of category tiles). Returns the node to focus.
    ui::Node* build_settings_hub(App &app, ui::Engine &e, ui::Node &into);
    // Push a settings page by id: audio, display, network, bluetooth, system,
    // power, appearance, layout, launcher.
    void open_settings_page(App &app, const std::string &id);

    class AudioPage : public Page {
    public:
        explicit AudioPage(App &a): Page(a, "Audio", "Output device and volume") {}
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        kit::RowButton *vol_ = nullptr, *mute_ = nullptr, *ui_sounds_ = nullptr;
        ui::Node *outputs_ = nullptr;
        int volume_ = -1;
        bool muted_ = false;
        void refresh();
    };

    class DisplayPage : public Page {
    public:
        explicit DisplayPage(App &a): Page(a, "Display", "Resolution, panel info and scaling") {}
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        kit::RowButton *mode_ = nullptr, *render_ = nullptr, *scale_ = nullptr, *overscan_ = nullptr;
        ui::Node *panel_ = nullptr;   // "Panel" info rows (filled by refresh)
        DisplayReport report_;
        std::string current_ = "", render_cur_ = "native";
        void refresh();
        void pick_mode();
        void pick_render();
        void fill_panel();
    };

    class NetworkPage : public Page {
    public:
        explicit NetworkPage(App &a): Page(a, "Network", "Wired and wireless connections") {}
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        ui::Node *ifaces_ = nullptr, *nets_ = nullptr;
        bool has_wifi_ = false;
        std::string wifi_state_;
        void refresh();
        void scan();
    };

    class BluetoothPage : public Page {
    public:
        explicit BluetoothPage(App &a): Page(a, "Bluetooth", "Controllers, remotes and headphones") {}
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        kit::RowButton *power_ = nullptr;
        ui::Node *devs_ = nullptr;
        bool powered_ = false, available_ = false;
        void refresh();
        void list(const std::string &out);
    };

    class SystemPage : public Page {
    public:
        explicit SystemPage(App &a): Page(a, "System", "About this device") {}
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        ui::Node *info_ = nullptr;
        void refresh();
    };

    // Live system usage (CPU, memory, temperatures...), polled while open.
    class StatsPage : public Page {
    public:
        explicit StatsPage(App &a): Page(a, "System stats", "Live CPU, memory and thermal usage") {}
        void on_enter(ui::Engine &e) override;
        bool tick(ui::Engine &e, float dt) override;
        bool on_action(ui::Engine &e, Action a) override;
        static constexpr int kTopProcs = 8;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        // Job callbacks hold a weak_ptr to this: a sample that finishes after
        // the page was popped (Back mid-sample) is dropped, not run on a
        // freed page.
        std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
        ui::Text *cpu_ = nullptr, *load_ = nullptr, *procs_ = nullptr, *uptime_ = nullptr;
        ui::Text *mem_ = nullptr, *swap_ = nullptr, *dio_ = nullptr;
        ui::Text *gpu_ = nullptr, *net_total_ = nullptr;
        ui::Graph *cpu_g_ = nullptr, *mem_g_ = nullptr, *net_g_ = nullptr;
        // Variable-length groups: rows are rebuilt only when the count
        // changes, otherwise their value texts are updated in place.
        struct Group { ui::Node *box = nullptr; std::vector<ui::Text*> keys, vals; };
        Group cores_, mounts_, temps_, net_;
        // Process rows are focusable, so they are never rebuilt while the
        // page is up (that would destroy the focused row): fixed slots,
        // hidden when unused; the pid behind each slot is looked up at
        // activation time.
        struct ProcRow { kit::RowButton *b = nullptr; int pid = -1; std::string name; };
        ProcRow top_[kTopProcs];
        ui::Text *top_empty_ = nullptr;
        float poll_ = 0;              // seconds since the last sample
        bool in_flight_ = false;
        void refresh();
        void show(const StatsSnapshot &s);
        void show_procs(const std::vector<ProcStat> &p);
        void set_group(Group &g, const std::vector<std::pair<std::string, std::string>> &rows);
        void confirm_kill(int slot);
    };

    class PowerPage : public Page {
    public:
        explicit PowerPage(App &a): Page(a, "Power") {}
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    };

    class AppearancePage : public Page {
    public:
        explicit AppearancePage(App &a): Page(a, "Appearance", "Theme, colours, wallpaper and motion") {}
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    };

    class LauncherPage : public Page {
    public:
        explicit LauncherPage(App &a): Page(a, "Launcher", "Sounds, clock, screensaver and input") {}
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    };

    // 2.4G air mouse: device status, enable toggle, per-button remap with a
    // live highlight — press any button and its row lights up.
    class AirMousePage : public Page {
    public:
        explicit AirMousePage(App &a): Page(a, "Air mouse", "Wave pointer and button remap") {}
        bool tick(ui::Engine &e, float dt) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        ui::Text *status_ = nullptr;
        kit::RowButton *enable_ = nullptr;
        ui::Node *buttons_ = nullptr;      // rows container (new rows appended here)
        std::map<int, kit::RowButton*> rows_;
        std::map<int, std::string> shown_; // code -> current value text
        int pressed_code_ = -1;
        void refresh_status();
        std::string code_label(int code) const;
        kit::RowButton* add_row(int code);
        void cycle(int code, int dir);
    };
}
