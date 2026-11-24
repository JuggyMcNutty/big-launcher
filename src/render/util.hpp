#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <SDL3/SDL.h>

namespace BL {
    enum class FileType{
        CONFIG,
        FONT,
        AUDIO
    };

    // Set by the app at startup (SDL_GetBasePath)
    void set_executable_dir(const char *dir);

    int utf8_length(std::string_view string);
    Uint16 get_unicode_code_point(const char *p, int &bytes);
    std::string utf8_truncate(const std::string &string, int width, int max_width);
    bool hex_to_color(std::string_view string, SDL_Color &color);
    // Split a string into lines (strips trailing newlines)
    std::vector<std::string> split_lines(const std::string &s);
    void join_paths(std::string &out, std::initializer_list<const char*> list);
    template <FileType file_type>
    std::string find_file(const char *filename);
    inline bool join_path([[maybe_unused]] std::filesystem::path &p)
    {
        return true;
    }

    template<typename... Args>
    inline bool join_path(std::filesystem::path &path, const std::filesystem::path &first, const Args&... args)
    {
        path /= first;
        return join_path(path, args...);
    }

    template<typename... Args>
    inline std::filesystem::path join_paths(const std::filesystem::path &first, const Args&... args)
    {
        std::filesystem::path path(first);
        return join_path(path, args...) ? path : std::filesystem::path();
    }
}
