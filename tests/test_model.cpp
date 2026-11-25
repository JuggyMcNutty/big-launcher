// TOML model: settings/layout/apps/theme round-trips + legacy importer.
#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "../src/core/model.hpp"

using namespace Foyer;
namespace fs = std::filesystem;

namespace {
    struct TmpDir {
        fs::path p;
        TmpDir() { p = fs::temp_directory_path() / ("foyer_model_" + std::to_string(getpid()) + "_" + std::to_string(rand())); fs::create_directories(p); }
        ~TmpDir() { std::error_code ec; fs::remove_all(p, ec); }
        std::string operator/(const std::string &s) const { return (p / s).string(); }
    };
    void write(const std::string &path, const std::string &s) { fs::create_directories(fs::path(path).parent_path()); std::ofstream(path) << s; }
}

TEST_SUITE("Model") {

TEST_CASE("settings round-trip and clamps") {
    TmpDir d;
    Settings s;
    s.theme = "light"; s.accent = "#FF8800"; s.ui_scale = 130; s.overscan = 3;
    s.reduce_motion = true; s.hotkeys.push_back({"F1", ":settings"});
    REQUIRE(save_settings(d / "settings.toml", s));
    Settings r = load_settings(d / "settings.toml");
    CHECK(r.theme == "light");
    CHECK(r.accent == "#FF8800");
    CHECK(r.ui_scale == 130);
    CHECK(r.overscan == 3);
    CHECK(r.reduce_motion);
    REQUIRE(r.hotkeys.size() == 1);
    CHECK(r.hotkeys[0].command == ":settings");
    write(d / "bad.toml", "[appearance]\nui_scale = 9000\noverscan = -4\n");
    Settings c = load_settings(d / "bad.toml");
    CHECK(c.ui_scale == 250);
    CHECK(c.overscan == 0);
    // second save leaves a .bak
    REQUIRE(save_settings(d / "settings.toml", s));
    CHECK(fs::exists(d / "settings.toml.bak"));
}

TEST_CASE("syntax errors are reported, defaults kept") {
    TmpDir d;
    write(d / "s.toml", "[appearance\ntheme = ");
    LoadReport rep;
    Settings s = load_settings(d / "s.toml", &rep);
    CHECK(s.theme == "default");
    CHECK(rep.warnings.size() == 1);
}

TEST_CASE("layout round-trip incl. shorthand apps and sizes") {
    TmpDir d;
    write(d / "layout.toml",
        "[[tab]]\ntitle = \"Home\"\n"
        "  [[tab.row]]\n  title = \"Watch\"\n  size = \"large\"\n  apps = [\"jellyfin\", \"youtube\"]\n"
        "  [[tab.row]]\n  title = \"More\"\n"
        "    [[tab.row.tile]]\n    title = \"Kodi-free\"\n    command = \"mpv\"\n    size = \"small\"\n"
        "[[tab]]\ntitle = \"Settings\"\nkind = \"settings\"\n"
        "[[power]]\ntitle = \"Reboot\"\ncommand = \":restart\"\n");
    Layout l = load_layout(d / "layout.toml");
    REQUIRE(l.tabs.size() == 2);
    CHECK(l.tabs[0].id == "home");
    REQUIRE(l.tabs[0].rows.size() == 2);
    CHECK(l.tabs[0].rows[0].size == TileSize::Large);
    REQUIRE(l.tabs[0].rows[0].tiles.size() == 2);
    CHECK(l.tabs[0].rows[0].tiles[1].app == "youtube");
    CHECK(l.tabs[0].rows[0].tiles[1].size == TileSize::Large);
    CHECK(l.tabs[0].rows[1].tiles[0].command == "mpv");
    CHECK(l.tabs[0].rows[1].tiles[0].size == TileSize::Small);
    CHECK(l.tabs[1].kind == "settings");
    REQUIRE(l.power.size() == 1);

    REQUIRE(save_layout(d / "out.toml", l));
    Layout r = load_layout(d / "out.toml");
    REQUIRE(r.tabs.size() == 2);
    CHECK(r.tabs[0].rows[0].tiles.size() == 2);
    CHECK(r.tabs[0].rows[0].size == TileSize::Large);
    CHECK(r.tabs[0].rows[1].tiles[0].title == "Kodi-free");
    CHECK(r.tabs[0].rows[1].tiles[0].size == TileSize::Small);
    CHECK(r.tabs[1].kind == "settings");
    CHECK(r.power[0].second == ":restart");
}

TEST_CASE("apps.d: override order, args/env quoting, hidden, art resolution") {
    TmpDir sys, user;
    write(sys / "apps.d/jellyfin.toml", "title = \"Jellyfin\"\ncommand = \"foyer-app jellyfin\"\nicon = \"jf.svg\"\n");
    write(sys / "apps.d/jf.svg", "<svg/>");
    write(sys / "apps.d/twitch.toml", "title = \"Twitch\"\ncommand = \"foyer-app twitch\"\n");
    write(user / "apps.d/jellyfin.toml", "title = \"My Jellyfin\"\ncommand = \"jellyfin-desktop\"\nargs = [\"--tv\", \"a b\"]\n[env]\nQT_SCALE = \"2\"\n");
    write(user / "apps.d/twitch.toml", "hidden = true\ncommand = \"x\"\n");
    write(user / "apps.d/broken.toml", "title = \"no command\"\n");
    LoadReport rep;
    auto apps = load_apps({user / "apps.d", sys / "apps.d"}, &rep);
    REQUIRE(apps.count("jellyfin"));
    CHECK(apps["jellyfin"].title == "My Jellyfin");
    CHECK(apps["jellyfin"].launch_command() == "QT_SCALE=2 jellyfin-desktop --tv 'a b'");
    CHECK_FALSE(apps.count("twitch"));
    CHECK_FALSE(apps.count("broken"));
    CHECK(rep.warnings.size() == 1);
    // art relative to the app file
    auto sys_only = load_apps({sys / "apps.d"});
    CHECK(sys_only["jellyfin"].resolve_art("jf.svg") == sys / "apps.d/jf.svg");

    AppEntry a = apps["jellyfin"];
    a.id = "copy";
    REQUIRE(save_app(user / "apps.d", a));
    auto again = load_apps({user / "apps.d"});
    CHECK(again["copy"].args.size() == 2);
    CHECK(again["copy"].env["QT_SCALE"] == "2");
}

TEST_CASE("theme loading: inherit, breakpoint override, user accent/wallpaper") {
    TmpDir d;
    setenv("FOYER_ISOLATED", "1", 1);
    Paths::init(d.p.string());
    write(d / "themes/default/theme.toml",
        "[colors]\naccent = \"#112233\"\nbg = \"#000000\"\n[layout]\npage_margin = 72\n"
        "[breakpoint.xs.layout]\npage_margin = 30\n");
    write(d / "themes/child/theme.toml",
        "[meta]\ntitle = \"Child\"\ninherit = \"default\"\n[colors]\ntext = \"#FFFFFF80\"\n");
    Settings s;
    auto k = load_theme("child", "md", s);
    CHECK(k.accent.r == 0x11);
    CHECK(k.text.a == 0x80);
    CHECK(k.page_margin == doctest::Approx(72));
    auto kx = load_theme("default", "xs", s);
    CHECK(kx.page_margin == doctest::Approx(30));
    s.accent = "#FF0000"; s.wallpaper = "#202020"; s.reduce_motion = true;
    auto ku = load_theme("child", "md", s);
    CHECK(ku.accent.r == 0xFF);
    CHECK(ku.wallpaper_mode == "color");
    CHECK(ku.bg.r == 0x20);
    CHECK(ku.reduce_motion);
    LoadReport rep;
    auto missing = load_theme("nope", "md", Settings{}, nullptr, &rep);
    CHECK(missing.accent.r == 0x11);   // fell back to default
    CHECK(rep.warnings.size() == 1);
    auto themes = list_themes();
    REQUIRE(themes.size() == 2);
    CHECK(themes[0].name == "child");
    CHECK(themes[0].title == "Child");
}

TEST_CASE("legacy import: layout.xml + config.ini -> TOML") {
    TmpDir d;
    write(d / "layout.xml",
        "<layout>\n"
        " <menu title=\"Media\">\n"
        "  <entry title=\"Jellyfin\"><command>htpc-jellyfin</command><card><icon>assets/icons/jellyfin.svg</icon><background>#000B25</background></card></entry>\n"
        "  <entry title=\"YouTube\"><command>foyer-app youtube</command><card><icon>assets/icons/red.svg</icon></card></entry>\n"
        "  <entry title=\"Old Red\"><command>htpc-red</command><card><icon>x.svg</icon></card></entry>\n"
        "  <entry title=\"Custom\"><command>/usr/bin/mpv --fs</command><card><icon>c.svg</icon></card></entry>\n"
        " </menu>\n"
        " <menu title=\"Settings\">\n"
        "  <entry title=\"WiFi\"><command>:wifi</command><card><icon>w.svg</icon></card></entry>\n"
        " </menu>\n"
        " <command title=\"Reboot\">:restart</command>\n"
        "</layout>\n");
    write(d / "config.ini", "[Sound]\nEnabled=true\nVolume=4\n[Clock]\nEnabled=true\nFormat=%I:%M\n[Launcher]\nUIScale=120\n");
    ImportResult r = import_legacy(d / "layout.xml", d / "config.ini");
    REQUIRE(r.ok);
    CHECK(r.settings.sound_volume == 4);
    CHECK(r.settings.clock_format == "%I:%M");
    REQUIRE(r.layout.tabs.size() == 3);
    REQUIRE(r.layout.tabs[0].rows.size() == 1);   // Settings menu dropped (built in)
    auto &tiles = r.layout.tabs[0].rows[0].tiles;
    REQUIRE(tiles.size() == 4);
    CHECK(tiles[0].app == "jellyfin");
    CHECK(r.apps["jellyfin"].command == "foyer-app jellyfin");
    CHECK(r.apps["jellyfin"].background == "#000B25");
    CHECK(tiles[2].app == "youtube");              // htpc-red -> youtube
    CHECK(r.apps["custom"].command == "/usr/bin/mpv --fs");
    CHECK(r.layout.tabs[2].kind == "settings");
    CHECK(r.layout.power[0].first == "Reboot");

    // maybe_import writes files once
    TmpDir cfg;
    std::string msg;
    CHECK(maybe_import(cfg.p.string(), d / "layout.xml", d / "config.ini", &msg));
    CHECK(fs::exists(cfg / "layout.toml"));
    CHECK(fs::exists(cfg / "settings.toml"));
    CHECK(fs::exists(cfg / "apps.d/jellyfin.toml"));
    CHECK_FALSE(maybe_import(cfg.p.string(), d / "layout.xml", d / "config.ini"));
    Layout back = load_layout(cfg / "layout.toml");
    CHECK(back.tabs[0].rows[0].tiles.size() == 4);
}

TEST_CASE("shipped data files parse") {
    std::string root = std::string(FOYER_SOURCE_DIR) + "/data";
    LoadReport rep;
    Layout l = load_layout(root + "/layout.toml", &rep);
    Settings s = load_settings(root + "/settings.toml", &rep);
    auto apps = load_apps({root + "/apps.d"}, &rep);
    CHECK(rep.warnings.empty());
    CHECK(l.tabs.size() == 3);
    CHECK(apps.size() == 5);
    for (auto &tab : l.tabs)
        for (auto &row : tab.rows)
            for (auto &t : row.tiles) CHECK(apps.count(t.app));
    setenv("FOYER_ISOLATED", "1", 1);
    Paths::init(root);
    for (auto &th : list_themes()) {
        LoadReport tr;
        load_theme(th.name, "md", s, nullptr, &tr);
        CHECK_MESSAGE(tr.warnings.empty(), th.name);
    }
    CHECK(list_themes().size() == 3);
    unsetenv("FOYER_ISOLATED");
}

}
