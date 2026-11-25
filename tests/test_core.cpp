#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>

#include "../src/core/config.hpp"
#include "../src/core/layout_model.hpp"
#include "../src/input/action_mapper.hpp"
#include "../src/platform/process.hpp"

using namespace BL;

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

}
