#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>

#include "../src/core/config.hpp"
#include "../src/core/layout_model.hpp"
#include "../src/input/action.hpp"
#include "../src/input/action_mapper.hpp"
#include "../src/input/airmouse.hpp"
#include "../src/platform/process.hpp"

using namespace Foyer;

static std::string tmp_path(const std::string &name)
{
    std::string p = std::string("/tmp/bl_test_") + name;
    return p;
}

// --- Config round-trip ---
TEST_SUITE("Config") {

TEST_CASE("ini round-trip preserves values") {
    std::string path = tmp_path("config.ini");
    {
        std::ofstream f(path);
        f << "# comment line\n";
        f << "[Settings]\n";
        f << "MouseSelect=true\n";
        f << "SidebarHighlightColor=#F2F2F2\n";
        f << "[Sound]\n";
        f << "Enabled=false\n";
        f << "Volume=7\n";
        f << "[Clock]\n";
        f << "Enabled=true\n";
        f << "Format=%H:%M\n";
    }

    Config c;
    c.parse(path);
    CHECK(c.mouse_select == true);
    CHECK(c.sound_enabled == false);
    CHECK(c.sound_volume == 7);
    CHECK(c.clock_enabled == true);
    CHECK(c.clock_format == "%H:%M");

    // Write and re-parse
    c.sound_volume = 9;
    c.write(path);

    Config c2;
    c2.parse(path);
    CHECK(c2.sound_volume == 9);
    CHECK(c2.mouse_select == true);
    CHECK(c2.clock_format == "%H:%M");

    // Comment preserved?
    std::ifstream in(path);
    std::string line;
    bool found_comment = false;
    while (std::getline(in, line))
        if (line.find("# comment line") != std::string::npos) found_comment = true;
    CHECK(found_comment);

    std::filesystem::remove(path);
}

TEST_CASE("bad values do not throw") {
    std::string path = tmp_path("config_bad.ini");
    {
        std::ofstream f(path);
        f << "[Sound]\nVolume=notanumber\n";
    }
    Config c;
    c.parse(path);  // should not throw
    CHECK(c.sound_volume == 10);  // default
    std::filesystem::remove(path);
}

}

// --- Layout round-trip ---
TEST_SUITE("LayoutModel") {

TEST_CASE("layout.xml round-trip") {
    std::string path = tmp_path("layout.xml");
    {
        std::ofstream f(path);
        f << "<layout>\n";
        f << "  <menu title=\"Media\">\n";
        f << "    <entry title=\"Jellyfin\">\n";
        f << "      <command>htpc-jellyfin</command>\n";
        f << "      <card>\n";
        f << "        <icon>assets/icons/jellyfin.svg</icon>\n";
        f << "        <background>#000B25</background>\n";
        f << "      </card>\n";
        f << "    </entry>\n";
        f << "  </menu>\n";
        f << "  <command title=\"Reboot\">:restart</command>\n";
        f << "</layout>\n";
    }

    LayoutModel m;
    m.parse(path);
    CHECK(m.menus.size() == 1);
    CHECK(m.menus[0].title == "Media");
    CHECK(m.menus[0].entries.size() == 1);
    CHECK(m.menus[0].entries[0].title == "Jellyfin");
    CHECK(m.menus[0].entries[0].command == "htpc-jellyfin");
    CHECK(m.menus[0].entries[0].card.icon == "assets/icons/jellyfin.svg");
    CHECK(m.menus[0].entries[0].card.background == "#000B25");
    CHECK(m.sidebar_commands.size() == 1);
    CHECK(m.sidebar_commands[0].title == "Reboot");
    CHECK(m.sidebar_commands[0].command == ":restart");

    // Write and re-parse
    m.write(path);
    LayoutModel m2;
    m2.parse(path);
    CHECK(m2.menus.size() == 1);
    CHECK(m2.menus[0].entries[0].title == "Jellyfin");
    CHECK(m2.sidebar_commands.size() == 1);

    std::filesystem::remove(path);
}

}

// --- Action mapping ---
TEST_SUITE("ActionMapper") {

TEST_CASE("keyboard mapping") {
    CHECK(ActionMapper::keyboard(SDLK_UP) == Action::Up);
    CHECK(ActionMapper::keyboard(SDLK_DOWN) == Action::Down);
    CHECK(ActionMapper::keyboard(SDLK_LEFT) == Action::Left);
    CHECK(ActionMapper::keyboard(SDLK_RIGHT) == Action::Right);
    CHECK(ActionMapper::keyboard(SDLK_RETURN) == Action::Accept);
    CHECK(ActionMapper::keyboard(SDLK_ESCAPE) == Action::Back);
    CHECK(!ActionMapper::keyboard(SDLK_A).has_value());
}

TEST_CASE("gamepad button mapping") {
    CHECK(ActionMapper::gamepad_button(SDL_GAMEPAD_BUTTON_DPAD_UP) == Action::Up);
    CHECK(ActionMapper::gamepad_button(SDL_GAMEPAD_BUTTON_SOUTH) == Action::Accept);
    CHECK(ActionMapper::gamepad_button(SDL_GAMEPAD_BUTTON_EAST) == Action::Back);
    CHECK(ActionMapper::gamepad_button(SDL_GAMEPAD_BUTTON_GUIDE) == Action::Home);
}

}

// --- Process reaping ---
TEST_SUITE("Process") {

TEST_CASE("spawn and reap a short process") {
    Process p = Process::spawn("true");
    CHECK(p.alive());
    // Wait for it to exit (poll a few times)
    for (int i = 0; i < 100 && p.alive(); i++) {
        p.poll();
        usleep(10000);
    }
    CHECK(!p.alive());
    CHECK(p.exit_status() == 0);
}

TEST_CASE("spawn and kill a long process") {
    Process p = Process::spawn("sleep 30");
    CHECK(p.alive());
    usleep(200000);  // let the child call setsid()
    p.kill();
    for (int i = 0; i < 200 && p.alive(); i++) {
        p.poll();
        usleep(10000);
    }
    CHECK(!p.alive());
}

}

int main(int argc, char **argv)
{
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();
    return res;
}

// --- htpc-ctl net status parsing ---
#include "../src/platform/sysctl.hpp"
TEST_SUITE("NetStatus") {

TEST_CASE("parses iface lines and summaries") {
    std::string out =
        "iface\teth0\tethernet\tup\t192.168.1.123/24\t98:fc:84:e2:0a:4f\t192.168.1.1\t192.168.1.1\t1000 Mb/s\n"
        "iface\twlan0\twifi\tdown\t-\taa:bb:cc:dd:ee:ff\t-\t-\t-\n"
        "eth=192.168.1.123\n"
        "wifi=disconnected\n";
    auto st = parse_net_status(out);
    REQUIRE(st.ifaces.size() == 2);
    CHECK(st.ifaces[0].name == "eth0");
    CHECK(st.ifaces[0].type == "ethernet");
    CHECK(st.ifaces[0].ip == "192.168.1.123/24");
    CHECK(st.ifaces[0].gateway == "192.168.1.1");
    CHECK(st.ifaces[0].speed == "1000 Mb/s");
    CHECK(st.ifaces[1].type == "wifi");
    CHECK(st.eth == "192.168.1.123");
    CHECK(st.wifi == "disconnected");
}

TEST_CASE("tolerates malformed and legacy output") {
    auto st = parse_net_status("eth=10.0.0.2\nwifi=no adapter\niface\tbroken\n");
    CHECK(st.ifaces.empty());
    CHECK(st.eth == "10.0.0.2");
    CHECK(st.wifi == "no adapter");
}

// --- display report parsing ---
#include "../src/platform/sysctl.hpp"
TEST_SUITE("DisplayReport") {

TEST_CASE("parses connectors, modes and panel info") {
    std::string out =
        "connector\tDP-2\tdisconnected\tDisplayPort\n"
        "connector\tDP-1\tconnected\tDisplayPort\n"
        "make=AOC\n"
        "name=Q32G3WG3R3\n"
        "native=3840x2160@60\n"
        "preferred=2560x1440@60\n"
        "hdr=HDR10 (ST.2084)\n"
        "hdr_max=302 cd/m2\n"
        "mode\t3840x2160\t50\t\n"
        "mode\t3840x2160\t60\tnative\n"
        "mode\t1920x1080\t60\tpreferred\n";
    auto r = parse_display_report(out);
    CHECK(r.connector == "DP-1");
    CHECK(r.connector_state == "connected");
    CHECK(r.connector_type == "DisplayPort");
    CHECK(r.native == "3840x2160@60");
    CHECK(r.preferred == "2560x1440@60");
    CHECK(r.panel.at("make") == "AOC");
    CHECK(r.panel.at("hdr") == "HDR10 (ST.2084)");
    REQUIRE(r.modes.size() == 3);
    CHECK(r.modes[0].w == 3840);
    CHECK(r.modes[0].h == 2160);
    CHECK(r.modes[0].rate == 60);
    CHECK(r.modes[0].native);
    CHECK_FALSE(r.modes[0].preferred);
    CHECK(r.modes[1].rate == 50);
    CHECK(r.modes[2].w == 1920);
    CHECK(r.modes[2].preferred);
    CHECK(display_mode_label(r.modes[0]) == "3840x2160 \xC3\x97 60 Hz");
}

TEST_CASE("empty or malformed output") {
    auto r = parse_display_report("");
    CHECK(r.modes.empty());
    CHECK(r.native.empty());
    auto r2 = parse_display_report("mode\tbroken\nmode\t640x480\t60\tpreferred\n");
    REQUIRE(r2.modes.size() == 1);
    CHECK(r2.modes[0].w == 640);
    CHECK(r2.modes[0].h == 480);
    CHECK(r2.modes[0].preferred);
}

}

// --- sys stats parsing ---
TEST_SUITE("SysStats") {

TEST_CASE("parses a snapshot") {
    std::string out =
        "cpu\t12\n"
        "core\t0\t10\n"
        "core\t1\t14\n"
        "mem\t1919376\t1135488\n"
        "swap\t524284\t102400\n"
        "load\t0.34\t0.43\t0.39\n"
        "proc\t212\t3\n"
        "up\t254432.5\n"
        "temp\tx86_pkg_temp\t45300\n"
        "gpu\t78\t1200\n"
        "net\teno1\t125\t2048\n"
        "disk\t11800000\t29000000\n";
    auto s = parse_sys_stats(out);
    CHECK(s.cpu_busy == 12);
    REQUIRE(s.cores.size() == 2);
    CHECK(s.cores[0].index == 0);
    CHECK(s.cores[0].busy == 10);
    CHECK(s.cores[1].index == 1);
    CHECK(s.cores[1].busy == 14);
    CHECK(s.mem_total_kb == 1919376);
    CHECK(s.mem_avail_kb == 1135488);
    CHECK(s.swap_total_kb == 524284);
    CHECK(s.swap_used_kb == 102400);
    CHECK(s.load1 == 0.34);
    CHECK(s.load15 == 0.39);
    CHECK(s.procs == 212);
    CHECK(s.procs_running == 3);
    CHECK(s.uptime_s == 254432.5);
    REQUIRE(s.temps.size() == 1);
    CHECK(s.temps[0].first == "x86_pkg_temp");
    CHECK(s.temps[0].second == 45300);
    CHECK(s.gpu_busy == "78");
    CHECK(s.gpu_freq == "1200");
    REQUIRE(s.net.size() == 1);
    CHECK(s.net[0].name == "eno1");
    CHECK(s.net[0].rx == 125);
    CHECK(s.net[0].tx == 2048);
    CHECK(s.disk_used_kb == 11800000);
    CHECK(s.disk_total_kb == 29000000);
    CHECK(fmt_temp(45300) == "45\xC2\xB0" "C");
    CHECK(fmt_temp(45999) == "46\xC2\xB0" "C");
    CHECK(fmt_kb(2048) == "2 kB");
    CHECK(fmt_kb(1024 * 1024) == "1.0 MB");
    CHECK(fmt_kb(2 * 1024LL * 1024 * 1024) == "2.0 GB");
    CHECK(fmt_bps(1024) == "1 kB/s");
    CHECK(fmt_bps(1.5 * 1024 * 1024) == "1.5 MB/s");
    CHECK(fmt_uptime(59) == "59 s");
    CHECK(fmt_uptime(300) == "5 min");
    CHECK(fmt_uptime(86400 + 2 * 3600 + 17 * 60) == "1 day, 2:17");
    CHECK(fmt_uptime(3 * 86400 + 5 * 60) == "3 days, 0:05");
}

TEST_CASE("missing gpu fields and empty snapshot") {
    auto s = parse_sys_stats("gpu\t-\t-\ncpu\t100\n");
    CHECK(s.gpu_busy.empty());
    CHECK(s.gpu_freq.empty());
    CHECK(s.cpu_busy == 100);
    auto e = parse_sys_stats("");
    CHECK(e.cpu_busy == 0);
    CHECK(e.cores.empty());
    CHECK(e.net.empty());
}

}
}

// --- dp units / breakpoints ---
#include "../src/ui/engine/units.hpp"
TEST_SUITE("Units") {

TEST_CASE("1080p is 1:1") {
    Metrics m; m.configure(1920, 1080);
    CHECK(m.scale() == doctest::Approx(1.0));
    CHECK(m.dp(100) == doctest::Approx(100));
    CHECK(m.breakpoint() == Breakpoint::MD);
}

TEST_CASE("4K doubles, 720p shrinks with boost, 480p boosted") {
    Metrics m;
    m.configure(3840, 2160);
    CHECK(m.scale() == doctest::Approx(2.0));
    CHECK(m.breakpoint() == Breakpoint::LG);
    m.configure(1280, 720);
    CHECK(m.breakpoint() == Breakpoint::SM);
    CHECK(m.scale() == doctest::Approx(720.0 / 1080.0 * 1.10));
    m.configure(640, 480);
    CHECK(m.breakpoint() == Breakpoint::XS);
    CHECK(m.dp(30) > 15.f);   // body text stays readable at 480p
}

TEST_CASE("ui scale, overscan and aspect") {
    Metrics m;
    m.configure(1920, 1080, 150, 5);
    CHECK(m.scale() == doctest::Approx(1.5));
    CHECK(m.safe_insets().left == doctest::Approx(96));
    CHECK(m.safe_rect().h == doctest::Approx(1080 - 2 * 54));
    m.configure(2560, 1080);   // 21:9: same scale, more width
    CHECK(m.scale() == doctest::Approx(1.0));
    CHECK(m.safe_width_dp() == doctest::Approx(2560));
    m.configure(1080, 1920);   // portrait uses the short edge
    CHECK(m.scale() == doctest::Approx(1.0));
    m.configure(1920, 1080, 1000, 99);   // clamped
    CHECK(m.scale() == doctest::Approx(2.5));
    CHECK(m.safe_insets().left == doctest::Approx(384));
}

}


TEST_SUITE("Airmouse") {

TEST_CASE("action names round-trip") {
    CHECK(parse_action("Accept") == Action::Accept);
    CHECK(parse_action("accept") == Action::Accept);
    CHECK(parse_action("VolUp") == Action::VolUp);
    CHECK(parse_action("None") == Action::None);
    CHECK(parse_action("") == Action::None);
    CHECK(parse_action("bogus") == Action::None);
    CHECK(std::string(action_name(Action::VolDown)) == "VolDown");
    size_t n = 0;
    const Action *acts = remap_actions(n);
    CHECK(n == 12);
    CHECK(acts[0] == Action::None);
}

TEST_CASE("default keymap from the XING WEI capture") {
    CHECK(airmouse_default_action(0x090001) == "Accept");   // left click
    CHECK(airmouse_default_action(0x07002A) == "Accept");   // click button
    CHECK(airmouse_default_action(0x0C0044) == "Accept");   // enter
    CHECK(airmouse_default_action(0x0C0224) == "Accept");   // return
    CHECK(airmouse_default_action(0x0C00CF) == "Back");     // del
    CHECK(airmouse_default_action(0x0C0223) == "Mute");
    CHECK(airmouse_default_action(0x0C00EA) == "VolDown");
    CHECK(airmouse_default_action(0x0C00E9) == "VolUp");
    CHECK(airmouse_default_action(0x070065) == "None");     // power: inert
    CHECK(airmouse_default_action(0x0C0042) == "None");     // menu keys: user's call
    CHECK(airmouse_default_label(0x070065) == "Power");
    CHECK(airmouse_default_label(0x090001) == "Left click");
    CHECK(airmouse_default_label(0xC00000) == "Button");    // unknown
    CHECK(parse_action(airmouse_default_action(0x0C00E9)) == Action::VolUp);
}

TEST_CASE("MSC press/release pairing and sweep") {
    std::map<int, bool> st;
    // three full press/release cycles on each of two codes
    for (int i = 0; i < 3; i++) {
        CHECK(Airmouse::pair(0x0C0043, st) == true);    // press
        CHECK(Airmouse::pair(0x0C0044, st) == true);    // press
        CHECK(Airmouse::pair(0x0C0043, st) == false);   // release
        CHECK(Airmouse::pair(0x0C0044, st) == false);   // release
    }
    CHECK(st.at(0x0C0043) == false);
    CHECK(st.at(0x0C0044) == false);
    // glitch: a press that never releases is swept after the deadline
    std::map<int, std::pair<bool, uint64_t>> held;
    held[5] = {true, 1000};
    held[6] = {true, 9000};
    std::vector<int> rel;
    Airmouse::sweep(held, 10000, 2000, &rel);
    CHECK(rel.size() == 1);
    CHECK(rel[0] == 5);
    CHECK(held.at(5).first == false);
    CHECK(held.at(6).first == true);
}

}
