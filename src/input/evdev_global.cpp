#include "evdev_global.hpp"
#include "../render/logger.hpp"

#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>

Foyer::EvdevGlobal::EvdevGlobal() = default;

Foyer::EvdevGlobal::~EvdevGlobal()
{
    stop();
}

void Foyer::EvdevGlobal::start()
{
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&EvdevGlobal::thread_loop, this);
}

void Foyer::EvdevGlobal::stop()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void Foyer::EvdevGlobal::thread_loop()
{
    // Open all input devices non-grabbing
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        Foyer::logger::warn("evdev: cannot open /dev/input");
        return;
    }
    int epfd = epoll_create1(0);
    std::vector<int> fds;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        std::string path = std::string("/dev/input/") + ent->d_name;
        int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        // Non-grabbing: just read
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0)
            fds.push_back(fd);
    }
    closedir(dir);
    Foyer::logger::info("evdev: monitoring {} devices", fds.size());

    struct epoll_event events[16];
    while (running_) {
        int n = epoll_wait(epfd, events, 16, 100);
        for (int i = 0; i < n; i++) {
            input_event e;
            while (read(events[i].data.fd, &e, sizeof(e)) == sizeof(e)) {
                if (e.type != EV_KEY) continue;
                Action a = Action::None;
                if (e.value == 1) {  // press
                    switch (e.code) {
                        case KEY_HOME:  a = Action::Home; break;
                        case KEY_TV:    a = Action::Home; break;
                        case KEY_MENU:  a = Action::Menu; break;
                        case KEY_VOLUMEUP:   a = Action::VolUp; break;
                        case KEY_VOLUMEDOWN: a = Action::VolDown; break;
                        case KEY_MUTE:       a = Action::Mute; break;
                        default: break;
                    }
                }
                if (a != Action::None && cb_)
                    cb_(a);
            }
        }
    }
    for (int fd : fds) close(fd);
    close(epfd);
}
