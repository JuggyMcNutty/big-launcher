#include "hub.hpp"
#include "../../core/app.hpp"
#include "audio.hpp"
#include "display.hpp"
#include "network.hpp"
#include "bluetooth.hpp"
#include "system.hpp"
#include "power.hpp"
#include "launcher.hpp"

BL::SettingsHub::SettingsHub(App &app): ListScreen(app)
{
    set_title("Settings");
    set_footer("D-pad: navigate  A: open  B: back");
    add_item("Audio");
    add_item("Display");
    add_item("Network");
    add_item("Bluetooth");
    add_item("System");
    add_item("Power");
    add_item("Launcher Preferences");
}

void BL::SettingsHub::on_select(int index)
{
    switch (index) {
        case 0: app_.screens().push(std::make_unique<AudioSettings>(app())); break;
        case 1: app_.screens().push(std::make_unique<DisplaySettings>(app())); break;
        case 2: app_.screens().push(std::make_unique<NetworkSettings>(app())); break;
        case 3: app_.screens().push(std::make_unique<BluetoothSettings>(app())); break;
        case 4: app_.screens().push(std::make_unique<SystemSettings>(app())); break;
        case 5: app_.screens().push(std::make_unique<PowerSettings>(app())); break;
        case 6: app_.screens().push(std::make_unique<LauncherSettings>(app())); break;
    }
}
