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
#include <cerrno>
#include <map>
#include <string>
#include <sys/inotify.h>

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
    // Non-grabbing readers on every /dev/input/event* device. inotify on
    // /dev/input picks up hotplugged remotes/keyboards/gamepads.
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    std::map<std::string, int> open_fds;   // node name -> fd
    auto add = [&](const std::string &name) {
        if (name.rfind("event", 0) != 0 || open_fds.count(name)) return;
        std::string path = "/dev/input/" + name;
        int fd = -1;
        for (int i = 0; i < 20 && fd < 0; i++) {   // udev may still be fixing permissions
            fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) usleep(50000);
        }
        if (fd < 0) return;
        struct epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0) open_fds[name] = fd;
        else close(fd);
    };
    if (DIR *dir = opendir("/dev/input")) {
        while (struct dirent *ent = readdir(dir)) add(ent->d_name);
        closedir(dir);
    } else {
        Foyer::logger::warn("evdev: cannot open /dev/input");
    }
    int ino = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ino >= 0) {
        inotify_add_watch(ino, "/dev/input", IN_CREATE | IN_DELETE);
        struct epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = ino;
        epoll_ctl(epfd, EPOLL_CTL_ADD, ino, &ev);
    }
    Foyer::logger::info("evdev: monitoring {} devices", open_fds.size());

    struct epoll_event events[16];
    while (running_) {
        int n = epoll_wait(epfd, events, 16, 100);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == ino) {
                alignas(struct inotify_event) char buf[4096];
                ssize_t len;
                while ((len = read(ino, buf, sizeof buf)) > 0) {
                    for (char *p = buf; p < buf + len;) {
                        auto *ie = (struct inotify_event*) p;
                        std::string name = ie->len ? ie->name : "";
                        if (ie->mask & IN_CREATE) add(name);
                        else if ((ie->mask & IN_DELETE) && open_fds.count(name)) {
                            close(open_fds[name]);
                            open_fds.erase(name);
                        }
                        p += sizeof(struct inotify_event) + ie->len;
                    }
                }
                continue;
            }
            input_event e;
            ssize_t r;
            while ((r = read(fd, &e, sizeof(e))) == sizeof(e)) {
                if (e.type != EV_KEY || e.value != 1) continue;   // presses only
                Action a = Action::None;
                switch (e.code) {
                    case KEY_HOME:
                    case KEY_HOMEPAGE:
                    case KEY_TV:         a = Action::Home; break;
                    case KEY_MENU:
                    case KEY_COMPOSE:    a = Action::Menu; break;
                    case KEY_VOLUMEUP:   a = Action::VolUp; break;
                    case KEY_VOLUMEDOWN: a = Action::VolDown; break;
                    case KEY_MUTE:       a = Action::Mute; break;
                    case BTN_MODE:       a = Action::Home; break;   // gamepad guide button
                    default: break;
                }
                if (a != Action::None && cb_) cb_(a);
            }
            if (r < 0 && errno == ENODEV) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                for (auto it = open_fds.begin(); it != open_fds.end(); ++it)
                    if (it->second == fd) { open_fds.erase(it); break; }
                close(fd);
            }
        }
    }
    for (auto &[k, fd] : open_fds) close(fd);
    if (ino >= 0) close(ino);
    close(epfd);
}
