#include "audio.hpp"
#include "../../core/app.hpp"
#include "../../platform/sysctl.hpp"
#include "../../render/util.hpp"

BL::AudioSettings::AudioSettings(App &app): ListScreen(app)
{
    set_title("Audio");
    set_footer("Left/Right: adjust  A: select  B: back");
    add_item("Volume", "--%");
    add_item("Mute", "off");
    add_item("Output", "--");
    add_item("UI Sounds", app_.config().sound_enabled ? "on" : "off");
    add_item("UI Volume", std::to_string(app_.config().sound_volume));
}

void BL::AudioSettings::on_show()
{
    refresh();
}

void BL::AudioSettings::refresh()
{
    app_.jobs().run(ctl("audio", "volume/get"), [this](const JobResult &res) {
        if (!res.ok) return;
        for (const auto &line : split_lines(res.output)) {
            if (line.starts_with("volume="))
                volume_ = atoi(line.c_str() + 7);
            else if (line.starts_with("muted="))
                muted_ = atoi(line.c_str() + 6) != 0;
        }
        if (volume_ >= 0)
            set_value(item_volume, std::to_string(volume_) + "%");
        set_value(item_mute, muted_ ? "on" : "off");
        app_.mark_dirty();
    });
    app_.jobs().run(ctl("audio", "sinks"), [this](const JobResult &res) {
        if (!res.ok) return;
        sinks_.clear();
        sink_index_ = 0;
        int i = 0;
        for (const auto &line : split_lines(res.output)) {
            auto tab = line.find('\t');
            if (tab == std::string::npos) continue;
            std::string name = line.substr(tab + 1);
            sinks_.push_back(name);
            if (line.size() > tab + 2 && line.back() == '1')
                sink_index_ = i;
            i++;
        }
        if (!sinks_.empty())
            set_value(item_output, sinks_[sink_index_]);
        app_.mark_dirty();
    });
}

bool BL::AudioSettings::on_action(Action action)
{
    if (action == Action::Left || action == Action::Right) {
        if (selected() == item_volume) {
            int delta = (action == Action::Right) ? 5 : -5;
            int v = (volume_ < 0 ? 50 : volume_) + delta;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            volume_ = v;
            set_value(item_volume, std::to_string(v) + "%");
            app_.play_click();
            app_.jobs().run(ctl("audio", "volume/set", {std::to_string(v)}), nullptr);
            app_.mark_dirty();
            return true;
        }
        if (selected() == item_uivol) {
            int delta = (action == Action::Right) ? 1 : -1;
            int v = app_.config().sound_volume + delta;
            if (v < 0) v = 0;
            if (v > 10) v = 10;
            app_.config().sound_volume = v;
            set_value(item_uivol, std::to_string(v));
            app_.play_click();
            app_.mark_dirty();
            return true;
        }
    }
    return ListScreen::on_action(action);
}

void BL::AudioSettings::on_select(int index)
{
    switch (index) {
        case item_mute:
            app_.jobs().run(ctl("audio", "mute/toggle"), [this](const JobResult &r) {
                if (r.ok) {
                    muted_ = !muted_;
                    set_value(item_mute, muted_ ? "on" : "off");
                }
            });
            break;
        case item_output:
            if (sinks_.size() > 1) {
                sink_index_ = (sink_index_ + 1) % (int) sinks_.size();
                set_value(item_output, sinks_[sink_index_]);
                // Set the sink by name (htpc-ctl resolves)
                app_.jobs().run(ctl("audio", "sink/set", {sinks_[sink_index_]}), nullptr);
            }
            break;
        case item_sounds: {
            app_.config().sound_enabled = !app_.config().sound_enabled;
            set_value(item_sounds, app_.config().sound_enabled ? "on" : "off");
            break;
        }
        default:
            break;
    }
}
