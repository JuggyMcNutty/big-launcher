#include <filesystem>
#include <string>
#include <string_view>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>
#include <lconfig.h>
#include "logger.hpp"
#include "util.hpp"

namespace Foyer {
    static const char *executable_dir_path = nullptr;

    void set_executable_dir(const char *dir)
    {
        executable_dir_path = dir ? dir : ".";
    }

    const char* executable_dir()
    {
        return executable_dir_path ? executable_dir_path : ".";
    }

    static const char* safe_exec_dir()
    {
        return executable_dir_path ? executable_dir_path : ".";
    }

    static const char* safe_home()
    {
        const char *h = getenv("HOME");
        return h ? h : "/tmp";
    }
}

// Calculates the length of a utf-8 encoded C-style string
int Foyer::utf8_length(std::string_view string)
{
    int length = 0;
    auto it = string.cbegin();
    while (it < string.cend()) {
        if ((*it & 0x80) == 0) // If byte is 0xxxxxxx, it's a 1 byte (ASCII) char
            it++;
        else if ((*it & 0xE0) == 0xC0) // If byte is 110xxxxx, it's a 2 byte char
            it +=2;
        else if ((*it & 0xF0) == 0xE0) // If byte is 1110xxx, it's a 3 byte char
            it +=3;
        else if ((*it & 0xF8) == 0xF0) // If byte is 1111x0xx, it's a 4 byte char
            it+=4;

        length++;
    }
    return length;
}

// A function to extract the Unicode code point from the first character in a UTF-8 encoded C-style string
Uint16 Foyer::get_unicode_code_point(const char *p, int &bytes)
{
    Uint16 result;

    // 1 byte ASCII char
    if ((*p & 0x80) == 0) {
        result = (Uint16) *p;
        bytes = 1;
    }

    // If byte is 110xxxxx, it's a 2 byte char
    else if ((*p & 0xE0) == 0xC0) {
        Uint8 byte1 = *p & 0x1F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        result = (Uint16) ((byte1 << 6) + byte2);
        bytes = 2;
    }

    // If byte is 1110xxx, it's a 3 byte char
    else if ((*p & 0xF0) == 0xE0) {
        Uint8 byte1 = *p & 0x0F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        Uint8 byte3 = *(p + 2) & 0x3F;
        result = (Uint16) ((byte1 << 12) + (byte2 << 6) + byte3);
        bytes = 3;
    }
    else {
        result = 0;
        bytes = 1;
    }
    return result;
}

// A function to truncate a utf-8 encoded string to max number of pixels
std::string Foyer::utf8_truncate(const std::string &string, int width, int max_width)
{
    int string_length = utf8_length(string);
    int avg_width = width / string_length;
    int num_chars = max_width / avg_width;
    int spaces = (string_length - num_chars) + 3; // Number of spaces to go back
    auto ptr = string.cend();
    int chars = 0;

    // Go back required number of spaces
    do {
        ptr--;
        if (!(*ptr & 0x80)) // ASCII characters have 0 as most significant bit
            chars++;
        else { // Non-ASCII character detected
            do {
                ptr--;
            } while (ptr > string.begin() && (*ptr & 0xC0) == 0x80); // Non-ASCII most significant byte begins with 0b11
            chars++;
        }
    } while (chars < spaces);

    // Add "..." to end of string to inform user of truncation
    return std::string(string, 0, (ptr - string.cbegin())) + "...";
}

// A function to convert a hex-formatted string into a color struct
bool Foyer::hex_to_color(std::string_view string, SDL_Color &color)
{
    auto it = string.cbegin();
    if (*it != '#' || string.size() != 7)
        return false;
    it++;

    Uint32 hex = (Uint32) strtoul(&*it, nullptr, 16);
    if (!hex && strcmp(&*it, "000000"))
        return false;

    color = {
        (Uint8) (hex >> 16),
        (Uint8) ((hex & 0x0000ff00) >> 8),
        (Uint8) (hex & 0x000000ff),
        0xFF
    };
    return true;
}

std::vector<std::string> Foyer::split_lines(const std::string &s)
{
    std::vector<std::string> out;
    std::string line;
    for (char c : s) {
        if (c == '\n') {
            if (!line.empty()) out.push_back(line);
            line.clear();
        }
        else if (c != '\r')
            line += c;
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

void Foyer::join_paths(std::string &out, std::initializer_list<const char*> list)
{
    std::filesystem::path path;
    for (const char *p : list)
        path /= p;
    out = path.string();
}

template <Foyer::FileType file_type>
std::string Foyer::find_file(const char *filename)
{
    static std::vector<std::filesystem::path> paths;
    if constexpr(file_type == Foyer::FileType::CONFIG) {
        if (!paths.size()) {
            paths.resize(4);
            paths[0] = CURRENT_DIRECTORY;
            paths[1] = safe_exec_dir();
            paths[2] = Foyer::join_paths(safe_home(), ".config", EXECUTABLE_TITLE);
            paths[3] = SYSTEM_SHARE_DIR;
        }
    }

    if constexpr(file_type == Foyer::FileType::FONT || file_type == Foyer::FileType::AUDIO) {
        if (!paths.size()) {
            const char *sub = file_type == Foyer::FileType::FONT ? "fonts" : "sounds";
            if (const char *d = getenv("FOYER_DATA_DIR"); d && *d)
                paths.push_back(Foyer::join_paths(d, sub));
            paths.push_back(Foyer::join_paths(safe_exec_dir(), "data", sub));
            paths.push_back(Foyer::join_paths(safe_exec_dir(), "..", "data", sub));
            paths.push_back(Foyer::join_paths(safe_exec_dir(), "assets", sub));
            paths.push_back(Foyer::join_paths(SYSTEM_SHARE_DIR, sub));
            paths.push_back(Foyer::join_paths("/usr/share/foyer", sub));
        }
    }
    for (const std::filesystem::path &path : paths) {
        auto file = join_paths(path, filename);
        if (std::filesystem::exists(file))
            return file.string();
    }
    return std::string();
}
template std::string Foyer::find_file<Foyer::FileType::CONFIG>(const char *filename);
template std::string Foyer::find_file<Foyer::FileType::FONT>(const char *filename);
template std::string Foyer::find_file<Foyer::FileType::AUDIO>(const char *filename);
