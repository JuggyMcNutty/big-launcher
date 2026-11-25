#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#define TOML_EXCEPTIONS 0
#include "../external/toml.hpp"

#include "model.hpp"
#include "config.hpp"
#include "layout_model.hpp"
#include "../render/logger.hpp"
#include "../render/util.hpp"

namespace fs = std::filesystem;

namespace Foyer {
    // ---- helpers -------------------------------------------------------------------
    bool parse_color(const std::string &s, SDL_Color &out)
    {
        std::string h = s;
        if (h.empty()) return false;
        if (h[0] == '#') h = h.substr(1);
        if (h.size() != 6 && h.size() != 8) return false;
        for (char c : h) if (!isxdigit((unsigned char) c)) return false;
        unsigned v = (unsigned) std::stoul(h, nullptr, 16);
        if (h.size() == 6) out = {(Uint8) (v >> 16), (Uint8) (v >> 8), (Uint8) v, 0xFF};
        else out = {(Uint8) (v >> 24), (Uint8) (v >> 16), (Uint8) (v >> 8), (Uint8) v};
        return true;
    }

    std::string color_hex(SDL_Color c)
    {
        char buf[16];
        if (c.a == 0xFF) snprintf(buf, sizeof buf, "#%02X%02X%02X", c.r, c.g, c.b);
        else snprintf(buf, sizeof buf, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
        return buf;
    }

    std::string slug(const std::string &s)
    {
        std::string out;
        for (char c : s) {
            if (isalnum((unsigned char) c)) out += (char) tolower((unsigned char) c);
            else if (!out.empty() && out.back() != '-') out += '-';
        }
        while (!out.empty() && out.back() == '-') out.pop_back();
        return out.empty() ? "item" : out;
    }

    static std::string shell_quote(const std::string &s)
    {
        if (!s.empty() && s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._/-=:@%+,") == std::string::npos)
            return s;
        std::string o = "'";
        for (char c : s) { if (c == '\'') o += "'\\''"; else o += c; }
        return o + "'";
    }

    std::string AppEntry::launch_command() const
    {
        std::string c;
        for (auto &[k, v] : env) c += k + "=" + shell_quote(v) + " ";
        c += command;
        for (auto &a : args) c += " " + shell_quote(a);
        return c;
    }

    std::string AppEntry::resolve_art(const std::string &p) const
    {
        if (p.empty() || p[0] == '/' || p[0] == '#') return p;
        if (!source.empty()) {
            fs::path cand = fs::path(source).parent_path() / p;
            std::error_code ec;
            if (fs::exists(cand, ec)) return cand.string();
        }
        std::string d = Paths::find_data(p);
        return d.empty() ? p : d;
    }

    bool write_file_atomic(const std::string &path, const std::string &content)
    {
        std::error_code ec;
        fs::path target(path);
        if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
        fs::path tmp = target; tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) { Foyer::logger::error("Cannot write {}", tmp.string()); return false; }
            f << content;
            if (!f.good()) return false;
        }
        if (fs::exists(target, ec)) {
            fs::path bak = target; bak += ".bak";
            fs::copy_file(target, bak, fs::copy_options::overwrite_existing, ec);
        }
        fs::rename(tmp, target, ec);
        if (ec) { Foyer::logger::error("Cannot replace {}: {}", path, ec.message()); return false; }
        return true;
    }

    // ---- Paths ---------------------------------------------------------------------
    static std::string g_config_dir = "/etc/foyer";

    void Paths::init(const std::string &d) { if (!d.empty()) g_config_dir = d; }
    const std::string& Paths::config_dir() { return g_config_dir; }

    std::vector<std::string> Paths::data_dirs()
    {
        std::vector<std::string> v;
        v.push_back(g_config_dir);
        if (getenv("FOYER_ISOLATED")) return v;   // tests: config dir only
        if (const char *e = getenv("FOYER_DATA_DIR"); e && *e) v.push_back(e);
        if (const char *x = executable_dir(); x && *x) {
            v.push_back((fs::path(x) / "data").string());
            v.push_back((fs::path(x) / ".." / "data").lexically_normal().string());
            v.push_back((fs::path(x) / "assets").string());
            v.push_back(x);
        }
        v.push_back("/usr/share/foyer");
        v.push_back("/usr/local/share/foyer");
        return v;
    }

    std::string Paths::find_data(const std::string &rel)
    {
        if (rel.empty()) return "";
        std::error_code ec;
        if (rel[0] == '/') return fs::exists(rel, ec) ? rel : "";
        for (auto &d : data_dirs()) {
            fs::path p = fs::path(d) / rel;
            if (fs::exists(p, ec)) return p.string();
        }
        return "";
    }

    std::vector<std::string> Paths::theme_dirs()
    {
        std::vector<std::string> out;
        std::error_code ec;
        for (auto &d : data_dirs()) {
            fs::path p = fs::path(d) / "themes";
            if (fs::is_directory(p, ec)) out.push_back(p.string());
        }
        return out;
    }

    // ---- toml helpers --------------------------------------------------------------
    template <typename T> static void get(const toml::table &t, const char *k, T &out)
    {
        if (auto v = t[k].value<T>()) out = *v;
    }
    static void get_f(const toml::table &t, const char *k, float &out)
    {
        if (auto v = t[k].value<double>()) out = (float) *v;
    }
    static void get_i(const toml::table &t, const char *k, int &out)
    {
        if (auto v = t[k].value<int64_t>()) out = (int) *v;
    }

    static std::optional<toml::table> parse_file(const std::string &file, LoadReport *rep)
    {
        std::error_code ec;
        if (!fs::exists(file, ec)) return std::nullopt;
        auto res = toml::parse_file(file);
        if (!res) {
            std::ostringstream os;
            os << file << ":" << res.error().source().begin.line << ": " << res.error().description();
            Foyer::logger::warn("TOML error: {}", os.str());
            if (rep) rep->warnings.push_back(os.str());
            return std::nullopt;
        }
        return std::move(res).table();
    }

    static std::string to_toml(const toml::table &t, const std::string &header)
    {
        std::ostringstream os;
        os << header << t << "\n";
        return os.str();
    }

    static TileSize parse_size(const std::string &s, TileSize def)
    {
        if (s == "small" || s == "s") return TileSize::Small;
        if (s == "large" || s == "l" || s == "hero") return TileSize::Large;
        if (s == "medium" || s == "m") return TileSize::Medium;
        return def;
    }
    static const char* size_name(TileSize s)
    {
        switch (s) { case TileSize::Small: return "small"; case TileSize::Large: return "large"; default: return "medium"; }
    }

    // ---- settings ------------------------------------------------------------------
    Settings load_settings(const std::string &file, LoadReport *rep)
    {
        Settings s;
        auto tp = parse_file(file, rep);
        if (!tp) return s;
        const toml::table &t = *tp;
        if (auto a = t["appearance"].as_table()) {
            get(*a, "theme", s.theme);
            get(*a, "accent", s.accent);
            get(*a, "wallpaper", s.wallpaper);
            get_i(*a, "ui_scale", s.ui_scale);
            get_i(*a, "overscan", s.overscan);
            get(*a, "reduce_motion", s.reduce_motion);
            get_f(*a, "motion_speed", s.motion_speed);
        }
        if (auto a = t["sound"].as_table()) {
            get(*a, "enabled", s.sound);
            get_i(*a, "volume", s.sound_volume);
        }
        if (auto a = t["clock"].as_table()) {
            get(*a, "enabled", s.clock);
            get(*a, "format", s.clock_format);
        }
        if (auto a = t["screensaver"].as_table()) {
            get(*a, "enabled", s.screensaver);
            get_i(*a, "idle_seconds", s.screensaver_idle_s);
            get_i(*a, "dim_percent", s.screensaver_dim);
        }
        if (auto a = t["input"].as_table()) {
            get(*a, "gamepad", s.gamepad);
            get(*a, "gamepad_mappings", s.gamepad_mappings);
            get(*a, "mouse_select", s.mouse_select);
        }
        if (auto a = t["launcher"].as_table()) {
            get(*a, "autostart", s.autostart);
            int b = (int) s.asset_budget_mb;
            get_i(*a, "asset_budget_mb", b);
            s.asset_budget_mb = (size_t) std::clamp(b, 16, 2048);
        }
        if (auto hk = t["hotkeys"].as_array()) {
            for (auto &n : *hk)
                if (auto h = n.as_table()) {
                    KeyBinding k;
                    get(*h, "key", k.key);
                    get(*h, "command", k.command);
                    if (!k.key.empty() && !k.command.empty()) s.hotkeys.push_back(k);
                }
        }
        s.ui_scale = std::clamp(s.ui_scale, 50, 250);
        s.overscan = std::clamp(s.overscan, 0, 20);
        s.sound_volume = std::clamp(s.sound_volume, 0, 10);
        s.screensaver_dim = std::clamp(s.screensaver_dim, 0, 100);
        return s;
    }

    bool save_settings(const std::string &file, const Settings &s)
    {
        toml::table t;
        t.insert("appearance", toml::table{
            {"theme", s.theme}, {"accent", s.accent}, {"wallpaper", s.wallpaper},
            {"ui_scale", s.ui_scale}, {"overscan", s.overscan},
            {"reduce_motion", s.reduce_motion}, {"motion_speed", (double) s.motion_speed}});
        t.insert("sound", toml::table{{"enabled", s.sound}, {"volume", s.sound_volume}});
        t.insert("clock", toml::table{{"enabled", s.clock}, {"format", s.clock_format}});
        t.insert("screensaver", toml::table{{"enabled", s.screensaver}, {"idle_seconds", s.screensaver_idle_s},
                                            {"dim_percent", s.screensaver_dim}});
        t.insert("input", toml::table{{"gamepad", s.gamepad}, {"gamepad_mappings", s.gamepad_mappings},
                                      {"mouse_select", s.mouse_select}});
        t.insert("launcher", toml::table{{"autostart", s.autostart}, {"asset_budget_mb", (int64_t) s.asset_budget_mb}});
        toml::array hk;
        for (auto &h : s.hotkeys) hk.push_back(toml::table{{"key", h.key}, {"command", h.command}});
        if (!hk.empty()) t.insert("hotkeys", std::move(hk));
        return write_file_atomic(file, to_toml(t, "# Foyer settings (written by the Settings screens; safe to hand-edit)\n\n"));
    }

    // ---- layout --------------------------------------------------------------------
    static Tile parse_tile(const toml::table &t, TileSize def)
    {
        Tile tile;
        get(t, "app", tile.app);
        get(t, "title", tile.title);
        get(t, "command", tile.command);
        get(t, "icon", tile.icon);
        get(t, "background", tile.background);
        std::string sz; get(t, "size", sz);
        tile.size = parse_size(sz, def);
        return tile;
    }

    Layout load_layout(const std::string &file, LoadReport *rep)
    {
        Layout l;
        auto tp = parse_file(file, rep);
        if (!tp) return l;
        if (auto tabs = (*tp)["tab"].as_array()) {
            for (auto &tn : *tabs) {
                auto tt = tn.as_table();
                if (!tt) continue;
                Tab tab;
                get(*tt, "title", tab.title);
                get(*tt, "id", tab.id);
                get(*tt, "kind", tab.kind);
                if (tab.id.empty()) tab.id = slug(tab.title);
                if (auto rows = (*tt)["row"].as_array()) {
                    for (auto &rn : *rows) {
                        auto rt = rn.as_table();
                        if (!rt) continue;
                        Row row;
                        get(*rt, "title", row.title);
                        std::string sz; get(*rt, "size", sz);
                        row.size = parse_size(sz, TileSize::Medium);
                        if (auto tiles = (*rt)["tile"].as_array())
                            for (auto &x : *tiles)
                                if (auto xt = x.as_table()) row.tiles.push_back(parse_tile(*xt, row.size));
                        // shorthand: apps = ["jellyfin", "youtube"]
                        if (auto apps = (*rt)["apps"].as_array())
                            for (auto &a : *apps)
                                if (auto s = a.value<std::string>()) { Tile x; x.app = *s; x.size = row.size; row.tiles.push_back(x); }
                        tab.rows.push_back(std::move(row));
                    }
                }
                l.tabs.push_back(std::move(tab));
            }
        }
        if (auto pw = (*tp)["power"].as_array())
            for (auto &p : *pw)
                if (auto pt = p.as_table()) {
                    std::string title, cmd;
                    get(*pt, "title", title); get(*pt, "command", cmd);
                    if (!title.empty() && !cmd.empty()) l.power.push_back({title, cmd});
                }
        return l;
    }

    bool save_layout(const std::string &file, const Layout &l)
    {
        toml::table t;
        toml::array tabs;
        for (auto &tab : l.tabs) {
            toml::table tt{{"id", tab.id}, {"title", tab.title}};
            if (tab.kind != "rows") tt.insert("kind", tab.kind);
            toml::array rows;
            for (auto &row : tab.rows) {
                toml::table rt{{"title", row.title}};
                if (row.size != TileSize::Medium) rt.insert("size", size_name(row.size));
                toml::array tiles;
                for (auto &x : row.tiles) {
                    toml::table xt;
                    if (!x.app.empty()) xt.insert("app", x.app);
                    if (!x.title.empty()) xt.insert("title", x.title);
                    if (!x.command.empty()) xt.insert("command", x.command);
                    if (!x.icon.empty()) xt.insert("icon", x.icon);
                    if (!x.background.empty()) xt.insert("background", x.background);
                    if (x.size != row.size) xt.insert("size", size_name(x.size));
                    tiles.push_back(std::move(xt));
                }
                rt.insert("tile", std::move(tiles));
                rows.push_back(std::move(rt));
            }
            if (!rows.empty()) tt.insert("row", std::move(rows));
            tabs.push_back(std::move(tt));
        }
        t.insert("tab", std::move(tabs));
        toml::array pw;
        for (auto &[title, cmd] : l.power) pw.push_back(toml::table{{"title", title}, {"command", cmd}});
        if (!pw.empty()) t.insert("power", std::move(pw));
        return write_file_atomic(file, to_toml(t,
            "# Foyer layout: tabs -> rows -> tiles. Tiles reference apps in apps.d/ by id,\n"
            "# or define title/command/icon inline. Edited by Settings > Layout.\n\n"));
    }

    // ---- apps ----------------------------------------------------------------------
    static AppEntry parse_app(const toml::table &t, const std::string &id, const std::string &src)
    {
        AppEntry a;
        a.id = id; a.source = src;
        get(t, "id", a.id);
        get(t, "title", a.title);
        get(t, "subtitle", a.subtitle);
        get(t, "command", a.command);
        get(t, "icon", a.icon);
        get(t, "tile", a.tile);
        get(t, "hero", a.hero);
        get(t, "background", a.background);
        if (auto args = t["args"].as_array())
            for (auto &x : *args) if (auto s = x.value<std::string>()) a.args.push_back(*s);
        if (auto env = t["env"].as_table())
            for (auto &[k, v] : *env) if (auto s = v.value<std::string>()) a.env[std::string(k.str())] = *s;
        if (a.title.empty()) a.title = a.id;
        return a;
    }

    std::map<std::string, AppEntry> load_apps(const std::vector<std::string> &dirs, LoadReport *rep)
    {
        // Later dirs are lower priority: iterate in reverse so earlier dirs override.
        std::map<std::string, AppEntry> out;
        std::error_code ec;
        for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) {
            if (!fs::is_directory(*it, ec)) continue;
            std::vector<fs::path> files;
            for (auto &de : fs::directory_iterator(*it, ec))
                if (de.path().extension() == ".toml") files.push_back(de.path());
            std::sort(files.begin(), files.end());
            for (auto &f : files) {
                auto tp = parse_file(f.string(), rep);
                if (!tp) continue;
                AppEntry a = parse_app(*tp, f.stem().string(), f.string());
                if (a.command.empty()) {
                    if (rep) rep->warnings.push_back(f.string() + ": no command");
                    continue;
                }
                if ((*tp)["hidden"].value_or(false)) { out.erase(a.id); continue; }
                out[a.id] = std::move(a);
            }
        }
        return out;
    }

    bool save_app(const std::string &dir, const AppEntry &a)
    {
        toml::table t{{"title", a.title}, {"command", a.command}};
        if (!a.subtitle.empty()) t.insert("subtitle", a.subtitle);
        if (!a.args.empty()) { toml::array arr; for (auto &x : a.args) arr.push_back(x); t.insert("args", std::move(arr)); }
        if (!a.env.empty()) { toml::table e; for (auto &[k, v] : a.env) e.insert(k, v); t.insert("env", std::move(e)); }
        if (!a.icon.empty()) t.insert("icon", a.icon);
        if (!a.tile.empty()) t.insert("tile", a.tile);
        if (!a.hero.empty()) t.insert("hero", a.hero);
        if (!a.background.empty()) t.insert("background", a.background);
        return write_file_atomic((fs::path(dir) / (a.id + ".toml")).string(),
                                 to_toml(t, "# Foyer app (edited by Settings > Layout > Edit app)\n\n"));
    }

    // ---- themes --------------------------------------------------------------------
    std::vector<ThemeInfo> list_themes()
    {
        std::map<std::string, ThemeInfo> m;
        std::error_code ec;
        auto dirs = Paths::theme_dirs();
        for (auto it = dirs.rbegin(); it != dirs.rend(); ++it)
            for (auto &de : fs::directory_iterator(*it, ec)) {
                fs::path tf = de.path() / "theme.toml";
                if (!fs::exists(tf, ec)) continue;
                ThemeInfo ti;
                ti.name = de.path().filename().string();
                ti.dir = de.path().string();
                ti.title = ti.name;
                if (auto tp = parse_file(tf.string(), nullptr)) {
                    if (auto meta = (*tp)["meta"].as_table()) {
                        get(*meta, "title", ti.title);
                        get(*meta, "author", ti.author);
                    }
                }
                m[ti.name] = ti;
            }
        std::vector<ThemeInfo> v;
        for (auto &[k, t] : m) v.push_back(t);
        return v;
    }

    static void apply_theme_table(const toml::table &t, ui::Tokens &k, const std::string &dir)
    {
        auto col = [&](const toml::table &tab, const char *key, SDL_Color &out) {
            if (auto s = tab[key].value<std::string>()) parse_color(*s, out);
        };
        if (auto c = t["colors"].as_table()) {
            col(*c, "bg", k.bg); col(*c, "surface", k.surface); col(*c, "surface_hi", k.surface_hi);
            col(*c, "accent", k.accent); col(*c, "on_accent", k.on_accent);
            col(*c, "text", k.text); col(*c, "text_dim", k.text_dim); col(*c, "danger", k.danger);
            col(*c, "focus_ring", k.focus_ring); col(*c, "shadow", k.shadow); col(*c, "scrim", k.scrim);
        }
        if (auto f = t["fonts"].as_table()) {
            auto font = [&](const char *key, std::string &out) {
                if (auto s = (*f)[key].value<std::string>()) {
                    fs::path p = fs::path(dir) / *s;
                    std::error_code ec;
                    out = fs::exists(p, ec) ? p.string() : *s;   // theme-local font or a bundled name
                }
            };
            font("body", k.font); font("display", k.font_display);
        }
        if (auto ty = t["type"].as_table()) {
            get_f(*ty, "xs", k.text_xs); get_f(*ty, "sm", k.text_sm); get_f(*ty, "md", k.text_md);
            get_f(*ty, "lg", k.text_lg); get_f(*ty, "xl", k.text_xl); get_f(*ty, "hero", k.text_hero);
        }
        if (auto sh = t["shape"].as_table()) {
            get_f(*sh, "radius_sm", k.radius_sm); get_f(*sh, "radius_md", k.radius_md); get_f(*sh, "radius_lg", k.radius_lg);
            get_f(*sh, "focus_ring_width", k.focus_ring_width); get_f(*sh, "focus_scale", k.focus_scale);
        }
        if (auto l = t["layout"].as_table()) {
            get_f(*l, "page_margin", k.page_margin); get_f(*l, "tile_width", k.tile_w);
            get_f(*l, "tile_aspect", k.tile_aspect); get_f(*l, "tile_gap", k.tile_gap);
        }
        if (auto m = t["motion"].as_table()) get_f(*m, "speed", k.motion_speed);
        if (auto w = t["wallpaper"].as_table()) {
            get(*w, "mode", k.wallpaper_mode);
            if (auto s = (*w)["image"].value<std::string>()) {
                fs::path p = fs::path(dir) / *s;
                std::error_code ec;
                k.wallpaper_image = fs::exists(p, ec) ? p.string() : *s;
            }
            get_f(*w, "dim", k.wallpaper_dim);
            get_f(*w, "blur", k.wallpaper_blur);
        }
    }

    ui::Tokens load_theme(const std::string &name, const std::string &bp, const Settings &s,
                          std::string *theme_dir, LoadReport *rep)
    {
        ui::Tokens k;
        std::string dir;
        for (auto &d : Paths::theme_dirs()) {
            fs::path p = fs::path(d) / name / "theme.toml";
            std::error_code ec;
            if (fs::exists(p, ec)) { dir = (fs::path(d) / name).string(); break; }
        }
        if (dir.empty() && name != "default") {
            if (rep) rep->warnings.push_back("theme '" + name + "' not found; using default");
            return load_theme("default", bp, s, theme_dir, rep);
        }
        if (!dir.empty()) {
            if (auto tp = parse_file(dir + "/theme.toml", rep)) {
                // inherit = "default"
                if (auto parent = (*tp)["meta"]["inherit"].value<std::string>(); parent && *parent != name) {
                    Settings plain; plain.accent.clear(); plain.wallpaper.clear();
                    k = load_theme(*parent, bp, plain, nullptr, rep);
                }
                apply_theme_table(*tp, k, dir);
                if (auto bps = (*tp)["breakpoint"].as_table())
                    if (auto o = (*bps)[bp].as_table()) apply_theme_table(*o, k, dir);
            }
        }
        if (theme_dir) *theme_dir = dir;
        // user overrides
        SDL_Color acc;
        if (!s.accent.empty() && parse_color(s.accent, acc)) k.accent = acc;
        if (!s.wallpaper.empty()) {
            SDL_Color c;
            if (s.wallpaper == "focused-art") k.wallpaper_mode = "focused-art";
            else if (parse_color(s.wallpaper, c)) { k.wallpaper_mode = "color"; k.bg = c; }
            else { k.wallpaper_mode = "image"; k.wallpaper_image = s.wallpaper; }
        }
        k.reduce_motion = s.reduce_motion;
        k.motion_speed *= s.motion_speed;
        return k;
    }

    // ---- legacy import -------------------------------------------------------------
    static std::string app_id_from_command(const std::string &cmd, const std::string &title)
    {
        // foyer-app <id> / htpc-app <id> / htpc-<id> / foyer-<id>
        std::istringstream is(cmd);
        std::string a, b;
        is >> a >> b;
        std::string base = a.substr(a.rfind('/') == std::string::npos ? 0 : a.rfind('/') + 1);
        if ((base == "foyer-app" || base == "htpc-app") && !b.empty()) return b == "red" ? "youtube" : b;
        if (base.rfind("htpc-", 0) == 0 && base.size() > 5) { std::string id = base.substr(5); return id == "red" ? "youtube" : id; }
        return slug(title);
    }

    ImportResult import_legacy(const std::string &layout_xml, const std::string &config_ini, const std::string &asset_dir)
    {
        ImportResult r;
        std::error_code ec;
        if (!config_ini.empty() && fs::exists(config_ini, ec)) {
            Config c;
            try { c.parse(config_ini); } catch (...) {}
            r.settings.sound = c.sound_enabled;
            r.settings.sound_volume = std::clamp(c.sound_volume, 0, 10);
            r.settings.clock = c.clock_enabled;
            r.settings.clock_format = c.clock_format;
            r.settings.screensaver = c.screensaver_enabled;
            r.settings.screensaver_idle_s = (int) (c.screensaver_idle_time / 1000);
            r.settings.screensaver_dim = (int) (c.screensaver_intensity * 100 / 255);
            r.settings.gamepad = c.gamepad_enabled;
            r.settings.gamepad_mappings = c.gamepad_mappings_file;
            r.settings.mouse_select = c.mouse_select;
            r.settings.ui_scale = c.ui_scale;
            r.settings.overscan = c.overscan;
            r.settings.autostart = c.autostart_command;
            for (auto &h : c.hotkeys)
                if (h.command != ":quit") r.settings.hotkeys.push_back({SDL_GetKeyName(h.keycode), h.command});
        }
        if (layout_xml.empty() || !fs::exists(layout_xml, ec)) return r;
        LayoutModel lm;
        try { lm.parse(layout_xml); } catch (...) { return r; }

        Tab home; home.id = "home"; home.title = "Home";
        Tab settings_tab; settings_tab.id = "settings"; settings_tab.title = "Settings"; settings_tab.kind = "settings";
        auto art = [&](const std::string &p) {
            if (p.empty() || p[0] == '/' || asset_dir.empty()) return p;
            // legacy paths were relative to the launcher dir ("assets/icons/x.svg")
            fs::path cand = fs::path(asset_dir) / p;
            return fs::exists(cand, ec) ? cand.string() : p;
        };
        for (auto &m : lm.menus) {
            bool is_settings = slug(m.title) == "settings";
            Row row; row.title = m.title;
            for (auto &e : m.entries) {
                if (e.command.starts_with(":")) {
                    if (is_settings) continue;   // built into the Settings tab now
                    Tile t; t.title = e.title; t.command = e.command; t.icon = art(e.card.icon); t.background = e.card.background;
                    row.tiles.push_back(t);
                    continue;
                }
                std::string id = app_id_from_command(e.command, e.title);
                AppEntry a;
                a.id = id; a.title = e.title;
                // normalise legacy wrappers to foyer-app
                std::string cmd = e.command;
                std::istringstream is(cmd); std::string first, second; is >> first >> second;
                std::string base = first.substr(first.rfind('/') == std::string::npos ? 0 : first.rfind('/') + 1);
                if (base.rfind("htpc-", 0) == 0 && base != "htpc-app" && base != "htpc-ctl") cmd = "foyer-app " + id;
                else if (base == "htpc-app") cmd = "foyer-app " + id;
                a.command = cmd;
                a.icon = art(e.card.icon);
                a.tile = art(e.card.custom);
                a.background = e.card.background;
                r.apps[id] = a;
                Tile t; t.app = id;
                row.tiles.push_back(t);
            }
            if (!row.tiles.empty() && !is_settings) home.rows.push_back(std::move(row));
        }
        for (auto &sc : lm.sidebar_commands) r.layout.power.push_back({sc.title, sc.command});
        r.layout.tabs.push_back(std::move(home));
        Tab lib; lib.id = "apps"; lib.title = "Apps"; lib.kind = "library";
        r.layout.tabs.push_back(std::move(lib));
        r.layout.tabs.push_back(std::move(settings_tab));
        r.ok = true;
        return r;
    }

    bool maybe_import(const std::string &cfg, const std::string &legacy_layout, const std::string &legacy_config, std::string *msg)
    {
        std::error_code ec;
        fs::path lt = fs::path(cfg) / "layout.toml";
        if (fs::exists(lt, ec)) return false;
        if (legacy_layout.empty() || !fs::exists(legacy_layout, ec)) return false;
        std::string assets = fs::path(legacy_layout).parent_path().string();
        if (const char *x = executable_dir(); x && *x) assets = x;
        ImportResult r = import_legacy(legacy_layout, legacy_config, assets);
        if (!r.ok) return false;
        fs::path apps = fs::path(cfg) / "apps.d";
        fs::create_directories(apps, ec);
        for (auto &[id, a] : r.apps) {
            // don't clobber user app definitions, and prefer packaged ones
            // (they carry proper artwork) when the command matches
            if (fs::exists(apps / (id + ".toml"), ec)) continue;
            bool shipped = false;
            for (auto &d : Paths::data_dirs()) {
                if (d == cfg) continue;
                if (fs::exists(fs::path(d) / "apps.d" / (id + ".toml"), ec)) { shipped = true; break; }
            }
            if (shipped && a.command == "foyer-app " + id) continue;
            AppEntry fixed = a;
            // legacy relative art that no longer resolves -> drop (tile shows the title)
            for (std::string *p : {&fixed.icon, &fixed.tile}) {
                if (!p->empty() && (*p)[0] != '/' && !fs::exists(*p, ec) && Paths::find_data(*p).empty()) p->clear();
            }
            save_app(apps.string(), fixed);
        }
        bool ok = save_layout(lt.string(), r.layout);
        fs::path st = fs::path(cfg) / "settings.toml";
        if (!fs::exists(st, ec)) ok &= save_settings(st.string(), r.settings);
        if (msg) *msg = "Imported " + std::to_string(r.apps.size()) + " apps from " + legacy_layout;
        Foyer::logger::info("Imported legacy layout {} ({} apps)", legacy_layout, r.apps.size());
        return ok;
    }
}
