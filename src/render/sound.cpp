#include <string>
#include <cmath>
#include <bit>

#include <SDL3/SDL.h>
#include "logger.hpp"

#include <lconfig.h>
#include "sound.hpp"
#include "util.hpp"

namespace Foyer {
    constexpr double RANGE_DB = 40.0;
    constexpr int MAX_VOLUME = 10;
}

Foyer::SoundFile::SoundFile(const std::string &path, int volume)
{
    SDL_AudioSpec spec;
    SDL_LoadWAV(path.c_str(), &spec, &buffer, &len);
    if (spec.format != SDL_AUDIO_S16LE || spec.channels != 1 || spec.freq != 48000)
        Foyer::logger::error_throw("Audio file {} has invalid format", path);

    // set volume
    if (volume <= 0 || volume >= Foyer::MAX_VOLUME)
        return;
    float scale_factor = static_cast<float>(std::pow(10.0, (static_cast<double>(volume - Foyer::MAX_VOLUME) * (Foyer::RANGE_DB / static_cast<double>(Foyer::MAX_VOLUME))) / 20.0));
    Sint16 *data = reinterpret_cast<Sint16*>(buffer);
    size_t nb_samples = len / sizeof(Sint16);
    for (size_t i = 0; i < nb_samples; i++) {
        if constexpr (std::endian::native == std::endian::big)
            data[i] = std::byteswap<Sint16>(static_cast<Sint16>(std::round(static_cast<float>(std::byteswap<Sint16>(data[i])) * scale_factor)));
        else
            data[i] = static_cast<Sint16>(std::round(static_cast<float>(data[i]) * scale_factor));
    }
}

Foyer::SoundFile::~SoundFile()
{
    SDL_free(buffer);
}

Foyer::Sound::Sound(int volume)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        Foyer::logger::error_throw("Failed to initialize audio");
    }
    Foyer::logger::debug("Successfully initialized audio");

    std::string click_path = Foyer::find_file<Foyer::FileType::AUDIO>(CLICK_FILENAME);
    if (click_path.empty())
        Foyer::logger::error_throw("Could not locate audio file '{}'", CLICK_FILENAME);
    std::string select_path = Foyer::find_file<Foyer::FileType::AUDIO>(SELECT_FILENAME);
    if (select_path.empty())
        Foyer::logger::error_throw("Could not locate audio file '{}'", SELECT_FILENAME);
    click = new Foyer::SoundFile(click_path, volume);
    select = new Foyer::SoundFile(select_path, volume);

    connect();
}

Foyer::Sound::~Sound()
{
    disconnect();
    delete click;
    delete select;
}

bool Foyer::Sound::connect()
{
    Foyer::logger::debug("Opening audio device...");
    SDL_AudioSpec spec = { SDL_AUDIO_S16LE, 1, 48000 };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Foyer::Sound::callback, this);
    if (!stream || !SDL_ResumeAudioStreamDevice(stream)) {
        Foyer::logger::error("Failed to open audio device (SDL Error: {})", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream);
    Foyer::logger::debug("Successfully opened audio device");
    return true;
}

void Foyer::Sound::disconnect()
{
    if (stream){
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
}

void Foyer::Sound::callback(void *userdata, SDL_AudioStream * /*stream*/, int additional_amount, int /*total_amount*/)
{
    reinterpret_cast<Foyer::Sound*>(userdata)->put_data(additional_amount);
}

void Foyer::Sound::put_data(int amount)
{
    std::unique_lock lock(mutex);
    if (!current_file)
        return;
    int bytes_read = std::min(amount, static_cast<int>(current_file->get_len() - current_file->get_pos()));
    SDL_PutAudioStreamData(stream, reinterpret_cast<const void*>(current_file->get_buffer() + current_file->get_pos()), bytes_read);
    current_file->inc_pos(bytes_read);
    if (!current_file->get_pos())
        current_file = nullptr;
}

void Foyer::Sound::set_current_file(Foyer::SoundFile &file)
{
    if (!stream)
        return;
    std::unique_lock lock(mutex);
    current_file = &file;
    current_file->reset_pos();
}

void Foyer::Sound::play_click()
{
    set_current_file(*click);
}

void Foyer::Sound::play_select()
{
    set_current_file(*select);
}
