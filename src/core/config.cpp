#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <map>

#include "config.hpp"
#include "../render/util.hpp"
#include "../render/logger.hpp"

namespace BL {
    static std::string trim(const std::string &s)
    {
        size_t b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return "";
        size_t e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }

    static std::string color_to_string(const SDL_Color &c)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
        return buf;
    }

    // Managed (section,key) pairs that map to typed fields
    static const std::vector<std::pair<std::string,std::string>> &managed_keys()
    {
        static const std::vector<std::pair<std::string,std::string>> keys = {
            {"Settings","SidebarHighlightColor"},
            {"Settings","SidebarTextColor"},
            {"Settings","SidebarTextSelectedColor"},
            {"Settings","MenuHighlightColor"},
            {"Settings","BackgroundImage"},
            {"Settings","MouseSelect"},
            {"Settings","Debug"},
            {"Sound","Enabled"},
            {"Sound","Volume"},
            {"Clock","Enabled"},
            {"Clock","Format"},
            {"Screensaver","Enabled"},
            {"Screensaver","IdleTime"},
            {"Screensaver","Intensity"},
            {"Gamepad","Enabled"},
            {"Gamepad","MappingsFile"},
            {"Launcher","AutostartCmd"},
            {"Launcher","UIScale"},
            {"Launcher","Overscan"},
        };
        return keys;
    }

}

void BL::Config::parse(const std::string &file)
{
    this->path = file;
    BL::logger::debug("Parsing config file '{}'", file);
    std::ifstream in(file);
    if (!in)
        BL::logger::critical("Could not open config file '{}'", file);

    lines.clear();
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t.front() == '#' || t.front() == ';') {
            lines.push_back({section, "", "", false, line});
            continue;
        }
        if (t.front() == '[' && t.back() == ']') {
            section = t.substr(1, t.size() - 2);
            lines.push_back({section, "", "", false, line});
            continue;
        }
        size_t eq = t.find('=');
        if (eq == std::string::npos) {
            lines.push_back({section, "", "", false, line});
            continue;
        }
        std::string key = trim(t.substr(0, eq));
        std::string value = trim(t.substr(eq + 1));
        lines.push_back({section, key, value, true, line});
    }
    load_typed();
    BL::logger::debug("Successfully parsed config file");
}

void BL::Config::load_typed()
{
    for (const Line &l : lines) {
        if (l.kv)
            typed_set(l.section, l.key, l.value);
    }
}

void BL::Config::typed_set(const std::string &section, const std::string &key, const std::string &value)
{
    auto bool_of = [](const std::string &v){ return v == "true" || v == "True" || v == "1"; };
    auto int_of = [](const std::string &v, int def)->int{
        try { return std::stoi(v); } catch (...) { return def; }
    };

    if (section == "Settings") {
        if (key == "SidebarHighlightColor")  hex_to_color(value, sidebar_highlight_color);
        else if (key == "SidebarTextColor")  hex_to_color(value, sidebar_text_color);
        else if (key == "SidebarTextSelectedColor") hex_to_color(value, sidebar_text_color_hl);
        else if (key == "MenuHighlightColor") hex_to_color(value, menu_highlight_color);
        else if (key == "BackgroundImage")   background_image_path = value;
        else if (key == "MouseSelect")       mouse_select = bool_of(value);
        else if (key == "Debug")             debug = bool_of(value);
    }
    else if (section == "Sound") {
        if (key == "Enabled") sound_enabled = bool_of(value);
        else if (key == "Volume") sound_volume = int_of(value, sound_volume);
    }
    else if (section == "Clock") {
        if (key == "Enabled") clock_enabled = bool_of(value);
        else if (key == "Format") clock_format = value;
    }
    else if (section == "Screensaver") {
        if (key == "Enabled") screensaver_enabled = bool_of(value);
        else if (key == "IdleTime") {
            int s = int_of(value, (int)(screensaver_idle_time / 1000));
            screensaver_idle_time = (Uint32)(s * 1000);
        }
        else if (key == "Intensity") {
            // percent
            std::string v = value;
            if (!v.empty() && v.back() == '%') v.pop_back();
            int p = int_of(v, (int)screensaver_intensity);
            screensaver_intensity = (Uint8)(p * 255 / 100);
        }
    }
    else if (section == "Gamepad") {
        if (key == "Enabled") gamepad_enabled = bool_of(value);
        else if (key == "MappingsFile") gamepad_mappings_file = value;
    }
    else if (section == "Hotkeys") {
        // value: #XXXX;command
        if (key.rfind("Hotkey", 0) == 0) {
            std::string v = value;
            if (v.front() == '#') {
                size_t semi = v.find(';');
                if (semi != std::string::npos) {
                    SDL_Keycode kc = (SDL_Keycode) strtol(v.c_str() + 1, nullptr, 16);
                    if (kc)
                        hotkeys.push_back({kc, v.substr(semi + 1)});
                }
            }
        }
    }
    else if (section == "Launcher") {
        if (key == "AutostartCmd") autostart_command = value;
        else if (key == "UIScale") ui_scale = int_of(value, ui_scale);
        else if (key == "Overscan") overscan = int_of(value, overscan);
    }
}

std::optional<std::string> BL::Config::typed_value(const std::string &section, const std::string &key) const
{
    auto bool_str = [](bool b){ return b ? "true" : "false"; };
    if (section == "Settings") {
        if (key == "SidebarHighlightColor")  return color_to_string(sidebar_highlight_color);
        if (key == "SidebarTextColor")       return color_to_string(sidebar_text_color);
        if (key == "SidebarTextSelectedColor") return color_to_string(sidebar_text_color_hl);
        if (key == "MenuHighlightColor")     return color_to_string(menu_highlight_color);
        if (key == "BackgroundImage")        return background_image_path;
        if (key == "MouseSelect")            return bool_str(mouse_select);
        if (key == "Debug")                  return bool_str(debug);
    }
    else if (section == "Sound") {
        if (key == "Enabled") return bool_str(sound_enabled);
        if (key == "Volume")  return std::to_string(sound_volume);
    }
    else if (section == "Clock") {
        if (key == "Enabled") return bool_str(clock_enabled);
        if (key == "Format")  return clock_format;
    }
    else if (section == "Screensaver") {
        if (key == "Enabled") return bool_str(screensaver_enabled);
        if (key == "IdleTime") return std::to_string(screensaver_idle_time / 1000);
        if (key == "Intensity") return std::to_string(screensaver_intensity * 100 / 255) + "%";
    }
    else if (section == "Gamepad") {
        if (key == "Enabled") return bool_str(gamepad_enabled);
        if (key == "MappingsFile") return gamepad_mappings_file;
    }
    else if (section == "Launcher") {
        if (key == "AutostartCmd") return autostart_command;
        if (key == "UIScale")  return std::to_string(ui_scale);
        if (key == "Overscan") return std::to_string(overscan);
    }
    return std::nullopt;
}

void BL::Config::set(const std::string &section, const std::string &key, const std::string &value)
{
    // Update or append the raw line
    for (Line &l : lines) {
        if (l.kv && l.section == section && l.key == key) {
            l.value = value;
            l.original = key + "=" + value;
            break;
        }
    }
    bool found = false;
    for (const Line &l : lines)
        if (l.kv && l.section == section && l.key == key) { found = true; break; }
    if (!found) {
        // Ensure the section exists
        bool have_section = false;
        for (const Line &l : lines)
            if (!l.kv && trim(l.original) == "[" + section + "]") { have_section = true; break; }
        if (!have_section)
            lines.push_back({section, "", "", false, "\n[" + section + "]"});
        lines.push_back({section, key, value, true, key + "=" + value});
    }
    // Update typed field if known
    typed_set(section, key, value);
}

std::optional<std::string> BL::Config::get(const std::string &section, const std::string &key) const
{
    for (const Line &l : lines)
        if (l.kv && l.section == section && l.key == key)
            return l.value;
    return std::nullopt;
}

void BL::Config::write(const std::string &file) const
{
    // Work on a copy of the lines
    std::vector<Line> out = lines;

    // 1. Sync managed keys into existing lines
    for (Line &l : out) {
        if (!l.kv) continue;
        if (l.section == "Hotkeys") continue;  // handled below
        if (auto v = typed_value(l.section, l.key)) {
            l.value = *v;
            l.original = l.key + "=" + *v;
        }
    }

    // 2. Handle hotkeys: remove existing HotkeyN lines, re-add from typed vector
    size_t hotkey_section_idx = std::string::npos;
    for (size_t i = 0; i < out.size(); i++)
        if (!out[i].kv && trim(out[i].original) == "[Hotkeys]") { hotkey_section_idx = i; break; }
    out.erase(std::remove_if(out.begin(), out.end(),
        [](const Line &l){ return l.kv && l.section == "Hotkeys"; }), out.end());
    if (hotkey_section_idx != std::string::npos) {
        // recompute index after erase
        hotkey_section_idx = std::string::npos;
        for (size_t i = 0; i < out.size(); i++)
            if (!out[i].kv && trim(out[i].original) == "[Hotkeys]") { hotkey_section_idx = i; break; }
    }
    if (!hotkeys.empty()) {
        if (hotkey_section_idx == std::string::npos) {
            out.push_back({"Hotkeys", "", "", false, "\n[Hotkeys]"});
            hotkey_section_idx = out.size() - 1;
        }
        std::vector<Line> hk;
        for (size_t i = 0; i < hotkeys.size(); i++) {
            char key[32];
            snprintf(key, sizeof(key), "Hotkey%zu", i + 1);
            std::string val = "#" + std::to_string((unsigned) hotkeys[i].keycode) + ";" + hotkeys[i].command;
            // convert keycode to hex
            char hex[16];
            snprintf(hex, sizeof(hex), "#%X;%s", (unsigned) hotkeys[i].keycode, hotkeys[i].command.c_str());
            val = hex;
            hk.push_back({"Hotkeys", key, val, true, std::string(key) + "=" + val});
        }
        out.insert(out.begin() + (int) hotkey_section_idx + 1, hk.begin(), hk.end());
    }

    // 3. Append managed keys that don't exist yet
    for (const auto &sk : managed_keys()) {
        bool present = false;
        for (const Line &l : out)
            if (l.kv && l.section == sk.first && l.key == sk.second) { present = true; break; }
        if (present) continue;
        auto v = typed_value(sk.first, sk.second);
        if (!v) continue;
        bool have_section = false;
        size_t section_idx = std::string::npos;
        for (size_t i = 0; i < out.size(); i++)
            if (!out[i].kv && trim(out[i].original) == "[" + sk.first + "]") { have_section = true; section_idx = i; break; }
        if (!have_section) {
            out.push_back({sk.first, "", "", false, "\n[" + sk.first + "]"});
            section_idx = out.size() - 1;
        }
        out.insert(out.begin() + (int) section_idx + 1,
                   Line{sk.first, sk.second, *v, true, sk.second + "=" + *v});
    }

    // 4. Serialize + write atomically with backup
    std::ostringstream ss;
    for (const Line &l : out)
        ss << l.original << "\n";

    std::filesystem::path target(file);
    std::filesystem::path bak = target; bak += ".bak";
    std::filesystem::path tmp = target; tmp += ".tmp";

    std::error_code ec;
    if (std::filesystem::exists(target, ec))
        std::filesystem::copy_file(target, bak, std::filesystem::copy_options::overwrite_existing, ec);
    {
        std::ofstream out_file(tmp, std::ios::trunc);
        out_file << ss.str();
    }
    std::filesystem::rename(tmp, target, ec);
    if (ec)
        BL::logger::error("Failed to write config '{}': {}", file, ec.message());
    else
        BL::logger::debug("Wrote config file '{}'", file);
}
