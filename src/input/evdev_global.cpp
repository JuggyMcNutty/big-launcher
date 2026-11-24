#include "evdev_global.hpp"
#include "../render/logger.hpp"

#include <libevdev/libevdev.h>
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>

BL::EvdevGlobal::EvdevGlobal() = default;

BL::EvdevGlobal::~EvdevGlobal()
{
    stop();
}

void BL::EvdevGlobal::start()
{
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&EvdevGlobal::thread_loop, this);
}

void BL::EvdevGlobal::stop()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void BL::EvdevGlobal::thread_loop()
{
    // Open all input devices non-grabbing
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        BL::logger::warn("evdev: cannot open /dev/input");
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
    BL::logger::debug("evdev: monitoring {} devices", fds.size());

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
                        case 217: a = Action::Home; break;   // KEY_TV
                        case 172: a = Action::Home; break;   // KEY_HOME
                        case 115: a = Action::VolUp; break;
                        case 114: a = Action::VolDown; break;
                        case 113: a = Action::Mute; break;
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
