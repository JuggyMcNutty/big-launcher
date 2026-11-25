#pragma once

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
        explicit DisplayPage(App &a): Page(a, "Display", "Resolution, scaling and TV safe area") {}
        void on_enter(ui::Engine &e) override;
    protected:
        void fill(ui::Engine &e, ui::Node &body) override;
    private:
        kit::RowButton *mode_ = nullptr, *render_ = nullptr, *scale_ = nullptr, *overscan_ = nullptr;
        std::vector<std::string> modes_;   // "WxH" (first = preferred)
        std::string preferred_, current_ = "", render_cur_ = "native";
        std::string connector_;
        void refresh();
        void pick_mode();
        void pick_render();
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
}
