#include <cmath>
#include <algorithm>

#include <lconfig.h>

#include "settings_screen.hpp"
#include "text.hpp"
#include "network.hpp"
#include "image.hpp"
#include "logger.hpp"

// Rendering constants (fractions of screen size)
static constexpr float SETTINGS_MARGIN_X     = 0.08f;
static constexpr float SETTINGS_TITLE_Y       = 0.08f;
static constexpr float SETTINGS_TITLE_SIZE    = 0.05f;
static constexpr float SETTINGS_ITEM_SIZE     = 0.036f;
static constexpr float SETTINGS_INFO_SIZE     = 0.028f;
static constexpr float SETTINGS_LIST_Y        = 0.24f;
static constexpr float SETTINGS_ROW_ADVANCE   = 0.058f;
static constexpr float SETTINGS_FOOTER_Y      = 0.90f;
static constexpr float SETTINGS_MESSAGE_Y     = 0.83f;
static constexpr int   SETTINGS_MAX_VISIBLE   = 11;

static constexpr SDL_Color COLOR_OVERLAY  = {0x05, 0x05, 0x0c, 0xf2};
static constexpr SDL_Color COLOR_BAR      = {0xff, 0xff, 0xff, 0x50};
static constexpr SDL_Color COLOR_TEXT     = {0xff, 0xff, 0xff, 0xff};
static constexpr SDL_Color COLOR_MESSAGE  = {0x9f, 0xd6, 0xff, 0xff};

BL::SettingsScreen::~SettingsScreen()
{
    clear_rows();
    delete title_texture;
    delete footer_texture;
    delete message_texture;
    delete password_texture;
    delete overlay;
    delete bar;
    delete title_font;
    delete item_font;
    delete info_font;
}

void BL::SettingsScreen::clear_rows()
{
    for (Texture *texture : row_textures)
        delete texture;
    row_textures.clear();
}

BL::Texture *BL::SettingsScreen::make_solid_texture(const SDL_Color &color)
{
    SDL_Surface *surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
    const Uint32 pixel = SDL_MapSurfaceRGBA(surface, color.r, color.g, color.b, color.a);
    SDL_FillSurfaceRect(surface, nullptr, pixel);
    Texture *texture = renderer->create_texture(*surface);
    texture->set_color_mod(color);
    BL::free_surface(surface);
    return texture;
}

void BL::SettingsScreen::rebuild_texture(Texture *&texture, const std::string &text, Font &font)
{
    delete texture;
    texture = nullptr;
    if (text.empty())
        return;
    const int max_width = static_cast<int>(static_cast<float>(screen_w) * (1.f - 2.f * SETTINGS_MARGIN_X));
    SDL_Surface *surface = font.render_text(text, nullptr, nullptr, max_width);
    if (!surface)
        return;
    texture = renderer->create_texture(*surface);
    BL::free_surface(surface);
    texture->set_color_mod(COLOR_TEXT);
}

void BL::SettingsScreen::rebuild_textures()
{
    clear_rows();
    for (const std::string &label : labels) {
        if (label.empty()) {
            row_textures.push_back(nullptr);
            continue;
        }
        Texture *texture = nullptr;
        rebuild_texture(texture, label, *item_font);
        row_textures.push_back(texture);
    }
    rebuild_texture(title_texture,
        (page == SettingsPage::WIFI) ? "WiFi" : "Bluetooth", *title_font);
    rebuild_texture(footer_texture,
        password_mode
            ? "Type password + Enter    Esc cancel"
            : "Arrows move    Enter select    Esc back    R refresh",
        *info_font);
}

void BL::SettingsScreen::set_message(const std::string &text)
{
    message = text;
    rebuild_texture(message_texture, message, *info_font);
    if (message_texture)
        message_texture->set_color_mod(COLOR_MESSAGE);
}

void BL::SettingsScreen::open(SettingsPage p, Renderer &r, int w, int h)
{
    renderer = &r;
    screen_w = w;
    screen_h = h;
    page = p;
    active = true;
    password_mode = false;
    input_text.clear();
    message.clear();
    index = 0;
    view_offset = 0;

    const float f_h = static_cast<float>(h);
    delete title_font; delete item_font; delete info_font;
    delete overlay; delete bar;
    title_font = new Font(SIDEBAR_FONT, static_cast<int>(std::round(f_h * SETTINGS_TITLE_SIZE)));
    item_font  = new Font(SIDEBAR_FONT, static_cast<int>(std::round(f_h * SETTINGS_ITEM_SIZE)));
    info_font  = new Font(SIDEBAR_FONT, static_cast<int>(std::round(f_h * SETTINGS_INFO_SIZE)));
    overlay = make_solid_texture(COLOR_OVERLAY);
    bar = make_solid_texture(COLOR_BAR);

    load_page();
}

void BL::SettingsScreen::close()
{
    active = false;
    password_mode = false;
    input_text.clear();
    set_message("");
}

void BL::SettingsScreen::load_page()
{
    index = 0;
    view_offset = 0;
    password_mode = false;
    input_text.clear();
    labels.clear();
    actions.clear();

    if (page == SettingsPage::WIFI) {
        const std::string state = BL::net::wifi_status_value("state");
        const std::string ssid  = BL::net::wifi_status_value("ssid");
        const std::string ip    = BL::net::wifi_status_value("ip");
        if (state.empty() || state == "none")
            labels.push_back("No WiFi adapter found");
        else if (state == "COMPLETED")
            labels.push_back("Connected: " + ssid + "   IP: " + ip);
        else
            labels.push_back("State: " + state + "   IP: " + (ip.empty() ? "-" : ip));
        actions.push_back("");

        const std::vector<BL::net::WifiNetwork> networks = BL::net::wifi_scan();
        if (!networks.empty()) {
            labels.push_back("Select a network to join:");
            actions.push_back("");
        }
        for (const BL::net::WifiNetwork &network : networks) {
            labels.push_back(network.ssid + "   " + network.signal + " dBm   " + network.flags);
            actions.push_back("wifi:connect:" + network.ssid + "\t" + network.flags);
        }
    }
    else {
        const std::string power = BL::net::bt_power_state();
        if (power == "none")
            labels.push_back("No Bluetooth adapter found");
        else
            labels.push_back(std::string("Bluetooth: ") + (power == "powered" ? "on" : "off"));
        actions.push_back("");

        labels.push_back("Scan for devices");
        actions.push_back("bt:scan");
        for (const BL::net::BtDevice &device : BL::net::bt_devices()) {
            labels.push_back("Pair: " + device.name + "  (" + device.mac + ")");
            actions.push_back("bt:pair:" + device.mac);
        }
    }

    rebuild_textures();
}

void BL::SettingsScreen::run_action(const std::string &action)
{
    if (action.rfind("wifi:connect:", 0) == 0) {
        const std::string rest = action.substr(std::string("wifi:connect:").size());
        const size_t tab = rest.find('\t');
        const std::string ssid = rest.substr(0, tab);
        const std::string flags = (tab == std::string::npos) ? "" : rest.substr(tab + 1);
        const bool needs_password =
            flags.find("PSK") != std::string::npos ||
            flags.find("SAE") != std::string::npos ||
            flags.find("WPA") != std::string::npos ||
            flags.find("RSN") != std::string::npos;
        if (needs_password) {
            password_mode = true;
            password_ssid = ssid;
            input_text.clear();
            set_message("Enter password for '" + ssid + "'");
            rebuild_textures();
        }
        else {
            const bool ok = BL::net::wifi_connect(ssid, "");
            set_message(ok ? "Connected to " + ssid : "Could not connect to " + ssid);
            load_page();
        }
    }
    else if (action == "bt:scan") {
        const std::vector<BL::net::BtDevice> devices = BL::net::bt_scan();
        set_message("Scan complete: " + std::to_string(devices.size()) + " device(s) found");
        load_page();
    }
    else if (action.rfind("bt:pair:", 0) == 0) {
        const std::string mac = action.substr(std::string("bt:pair:").size());
        const bool ok = BL::net::bt_pair(mac);
        set_message(ok ? "Paired " + mac : "Pairing failed for " + mac);
        load_page();
    }
}

void BL::SettingsScreen::submit_password()
{
    const std::string ssid = password_ssid;
    const std::string psk = input_text;
    password_mode = false;
    input_text.clear();
    const bool ok = !psk.empty() && BL::net::wifi_connect(ssid, psk);
    set_message(ok ? "Connected to " + ssid : "Could not connect to " + ssid);
    load_page();
}

void BL::SettingsScreen::move_up()
{
    if (!active || labels.empty() || password_mode)
        return;
    if (index > 0)
        index--;
}

void BL::SettingsScreen::move_down()
{
    if (!active || labels.empty() || password_mode)
        return;
    if (index < static_cast<int>(labels.size()) - 1)
        index++;
}

void BL::SettingsScreen::select()
{
    if (!active)
        return;
    if (password_mode) {
        submit_password();
        return;
    }
    if (index >= 0 && index < static_cast<int>(actions.size()) && !actions[index].empty())
        run_action(actions[index]);
}

void BL::SettingsScreen::text_input(const char *text)
{
    if (!active || !password_mode || !text)
        return;
    input_text += text;
    if (input_text.size() > 63)
        input_text.resize(63);
    rebuild_texture(password_texture, "Password: " + std::string(input_text.size(), '*'), *item_font);
}

void BL::SettingsScreen::backspace()
{
    if (!active || !password_mode || input_text.empty())
        return;
    input_text.pop_back();
    rebuild_texture(password_texture, "Password: " + std::string(input_text.size(), '*'), *item_font);
}

void BL::SettingsScreen::back()
{
    if (!active)
        return;
    if (password_mode) {
        // Cancel password entry
        password_mode = false;
        input_text.clear();
        delete password_texture;
        password_texture = nullptr;
        set_message("");
        rebuild_textures();
    }
    else
        close();
}

void BL::SettingsScreen::draw()
{
    if (!active || !renderer)
        return;

    const float w = static_cast<float>(screen_w);
    const float h = static_cast<float>(screen_h);
    const float x = w * SETTINGS_MARGIN_X;

    // Dim backdrop over the whole screen
    overlay->update_pos({0.f, 0.f, w, h});
    renderer->draw(*overlay);

    // Title
    if (title_texture) {
        title_texture->set_x(x);
        title_texture->set_y(h * SETTINGS_TITLE_Y);
        renderer->draw(*title_texture);
    }

    // Rows
    float row_y = h * SETTINGS_LIST_Y;
    const float row_advance = h * SETTINGS_ROW_ADVANCE;
    const int last = static_cast<int>(labels.size());
    if (index >= view_offset + SETTINGS_MAX_VISIBLE)
        view_offset = index - SETTINGS_MAX_VISIBLE + 1;
    if (index < view_offset)
        view_offset = index;

    for (int i = view_offset; i < last && i < view_offset + SETTINGS_MAX_VISIBLE; i++) {
        if (i == index) {
            bar->update_pos({x - 10.f, row_y - 6.f, w - 2.f * x + 20.f, row_advance - 4.f});
            renderer->draw(*bar);
        }
        if (Texture *row = row_textures[i]) {
            row->set_x(x);
            row->set_y(row_y);
            renderer->draw(*row);
        }
        row_y += row_advance;
    }

    // Password prompt
    if (password_mode) {
        if (!password_texture)
            rebuild_texture(password_texture, "Password: ", *item_font);
        if (password_texture) {
            password_texture->set_x(x);
            password_texture->set_y(h * SETTINGS_LIST_Y + row_advance);
            renderer->draw(*password_texture);
        }
    }

    // Message
    if (message_texture) {
        message_texture->set_x(x);
        message_texture->set_y(h * SETTINGS_MESSAGE_Y);
        renderer->draw(*message_texture);
    }

    // Footer
    if (footer_texture) {
        footer_texture->set_x(x);
        footer_texture->set_y(h * SETTINGS_FOOTER_Y);
        renderer->draw(*footer_texture);
    }
}