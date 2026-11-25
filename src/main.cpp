#include <cstdio>
#include <filesystem>
#include <getopt.h>
#include <string>

#include <SDL3/SDL.h>

#include "core/app.hpp"
#include "render/logger.hpp"
#include "render/util.hpp"
#include <lconfig.h>

using namespace Foyer;
namespace fs = std::filesystem;

static void print_help()
{
    printf("Usage: foyer [OPTIONS]\n"
           "  -c, --config DIR       Config dir (settings.toml, layout.toml, apps.d/, themes/).\n"
           "                         Default: $FOYER_CONF or /etc/foyer. A legacy config.ini path\n"
           "                         is accepted and its directory is used.\n"
           "  -l, --layout FILE      Legacy layout.xml to import on first run.\n"
           "      --window           Run in a window instead of fullscreen.\n"
           "      --size WxH         Window / offscreen size (e.g. 3840x2160).\n"
           "      --screenshot FILE  Render offscreen once, save a PNG, exit (testing).\n"
           "      --open CMD         Run a command on start (e.g. :settings, :display).\n"
           "      --keys LIST        With --screenshot: send actions first (up,down,left,right,\n"
           "                         accept,back,menu,home; comma separated).\n"
           "      --overlay          Quick panel for gamescope's external overlay.\n"
           "  -d, --debug            Verbose logging + frame stats.\n"
           "  -v, --version          Print version.\n"
           "  -h, --help             This help.\n");
}

int main(int argc, char *argv[])
{
    set_executable_dir(SDL_GetBasePath());
    auto logger = init_logging_global();
    Options o;
    static struct option opts[] = {
        {"config", required_argument, nullptr, 'c'},
        {"layout", required_argument, nullptr, 'l'},
        {"window", no_argument, nullptr, 'w'},
        {"size", required_argument, nullptr, 's'},
        {"screenshot", required_argument, nullptr, 'S'},
        {"open", required_argument, nullptr, 'O'},
        {"keys", required_argument, nullptr, 'K'},
        {"overlay", no_argument, nullptr, 'o'},
        {"debug", no_argument, nullptr, 'd'},
        {"version", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}};
    int c;
    while ((c = getopt_long(argc, argv, "+c:l:dvh", opts, nullptr)) != -1) {
        switch (c) {
            case 'c': if (optarg) o.config_dir = optarg; break;
            case 'l': if (optarg) o.legacy_layout = optarg; break;
            case 'w': o.windowed = true; break;
            case 's': if (optarg) sscanf(optarg, "%dx%d", &o.width, &o.height); break;
            case 'S': if (optarg) o.screenshot = optarg; break;
            case 'O': if (optarg) o.open = optarg; break;
            case 'K': if (optarg) o.keys = optarg; break;
            case 'o': o.overlay = true; break;
            case 'd': o.debug = true; break;
            case 'v': printf("foyer %s\n", PROJECT_VERSION); return 0;
            case 'h': print_help(); return 0;
            default: print_help(); return 2;
        }
    }
    if (o.debug) logger->set_level(spdlog::level::debug);
    if (o.config_dir.empty()) {
        const char *env = getenv("FOYER_CONF");
        o.config_dir = env && *env ? env : "/etc/foyer";
    }
    // accept the legacy `-c config.ini`
    std::error_code ec;
    if (fs::is_regular_file(o.config_dir, ec) || o.config_dir.ends_with(".ini")) {
        o.legacy_config = o.config_dir;
        o.config_dir = fs::path(o.config_dir).parent_path().string();
        if (o.config_dir.empty()) o.config_dir = ".";
    }
    try {
        App app(o);
        return app.run();
    } catch (std::exception &e) {
        fprintf(stderr, "foyer: fatal error: %s\n", e.what());
        Foyer::logger::error("Fatal error: {}", e.what());
        return 1;
    }
}
