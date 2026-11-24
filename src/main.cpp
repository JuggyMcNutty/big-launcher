#include <string>
#include <cstdio>
#include <getopt.h>

#include <SDL3/SDL.h>
#include "core/app.hpp"
#include "screens/quick_menu.hpp"
#include "render/util.hpp"
#include "render/logger.hpp"
#include <lconfig.h>

using namespace BL;

static void print_help()
{
    fmt::print("Usage: big-launcher [OPTIONS]\n");
    fmt::print("    -c p,  --config=p     Load config file from path p.\n");
    fmt::print("    -l p,  --layout=p     Load layout file from path p.\n");
    fmt::print("    -d,    --debug        Enable debug messages.\n");
    fmt::print("    -h,    --help         Show this help message.\n");
}

int main(int argc, char *argv[])
{
    set_executable_dir(SDL_GetBasePath());
    auto logger = init_logging_global();
    int c;
    static struct option long_opts[] = {
        { "config",  required_argument, nullptr, 'c' },
        { "layout",  required_argument, nullptr, 'l' },
        { "debug",   no_argument,       nullptr, 'd' },
        { "overlay", no_argument,       nullptr, 'o' },
        { "help",    no_argument,       nullptr, 'h' },
        { 0, 0, 0, 0 }
    };
    std::string config_path, layout_path;
    bool debug = false, overlay = false;
    while ((c = getopt_long(argc, argv, "+c:l:dh o", long_opts, nullptr)) != -1) {
        switch (c) {
            case 'c': if (optarg) config_path = optarg; break;
            case 'l': if (optarg) layout_path = optarg; break;
            case 'd': debug = true; break;
            case 'o': overlay = true; break;
            case 'h': print_help(); return 0;
            default: break;
        }
    }

    // Locate files
    if (config_path.empty()) {
        config_path = find_file<FileType::CONFIG>(CONFIG_FILENAME);
        if (config_path.empty())
            config_path = CONFIG_FILENAME;
    }
    if (layout_path.empty()) {
        layout_path = find_file<FileType::CONFIG>(LAYOUT_FILENAME);
        if (layout_path.empty())
            layout_path = LAYOUT_FILENAME;
    }

    try {
        App app(config_path, layout_path, overlay);
        if (debug)
            app.config().debug = true;
        if (overlay)
            app.screens().push(std::make_unique<QuickMenu>(app));
        return app.run();
    }
    catch (std::exception &e) {
        fprintf(stderr, "big-launcher: fatal error: %s\n", e.what());
        BL::logger::error("Fatal error: {}", e.what());
        return 1;
    }
}
