#include "airmouse.hpp"
#include "../render/logger.hpp"

#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <time.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <vector>

namespace fs = std::filesystem;

namespace Foyer {

    Airmouse::Airmouse(Callback cb): cb_(std::move(cb)) {}

    Airmouse::~Airmouse()
    {
        stop();
    }

    void Airmouse::set_match(const std::string &s)
    {
        match_ = s;
    }

    std::string Airmouse::device_name() const
    {
        std::lock_guard l(name_mu_);
        return devname_;
    }

    int Airmouse::step(int code, uint64_t now, std::map<int, std::pair<bool, uint64_t>> &held)
    {
        auto &h = held[code];
        int out;
        if (h.first) {
            h.first = false;          // release report (or a mirrored press)
            out = -1;
        } else if (now - h.second > DEDUP_US) {
            h.first = true;           // a genuinely new press
            out = 1;
        } else {
            out = 0;                  // mirror of the press we just handled
        }
        h.second = now;
        return out;
    }

    void Airmouse::sweep(std::map<int, std::pair<bool, uint64_t>> &held, uint64_t now, uint64_t ms,
                         std::vector<int> *released)
    {
        for (auto &kv : held)
            if (kv.second.first && now - kv.second.second > ms) {
                kv.second.first = false;
                if (released) released->push_back(kv.first);
            }
    }

    static std::string to_lower(std::string s)
    {
        for (char &c : s) c = (char) std::tolower((unsigned char) c);
        return s;
    }

    static std::string read_sysfs_file(const fs::path &p)
    {
        std::ifstream in(p);
        std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        return s;
    }

    // Walk up from the event node's device until a USB device (with a
    // `product` file) is found; return "manufacturer product".
    bool Airmouse::sysfs_usb_name(const std::string &node, std::string &out)
    {
        fs::path p = fs::weakly_canonical(fs::path("/sys/class/input") / node / "device");
        for (int i = 0; i < 12 && !p.empty(); i++) {
            if (fs::is_regular_file(p / "product")) {
                std::string m = read_sysfs_file(p / "manufacturer");
                std::string pr = read_sysfs_file(p / "product");
                out = (m + " " + pr);
                while (out.find("  ") != std::string::npos) out.replace(out.find("  "), 1, "");
                return !out.empty();
            }
            if (p == "/sys" || p == "/") return false;
            p = p.parent_path();
        }
        return false;
    }

    bool Airmouse::matches(const std::string &node, const std::string &match, std::string *name)
    {
        if (match.empty()) return false;
        std::string usb;
        if (sysfs_usb_name(node, usb)) {
            if (name) *name = usb;
            if (to_lower(usb).find(to_lower(match)) != std::string::npos) return true;
        }
        // Fallback: evdev name (some devices report one)
        fs::path np = fs::path("/sys/class/input") / node / "name";
        std::string evname = read_sysfs_file(np);
        if (name && usb.empty()) *name = evname;
        return !evname.empty() && to_lower(evname).find(to_lower(match)) != std::string::npos;
    }

    void Airmouse::start()
    {
        if (running_) return;
        running_ = true;
        thread_ = std::thread(&Airmouse::thread_loop, this);
    }

    void Airmouse::stop()
    {
        running_ = false;
        if (thread_.joinable())
            thread_.join();
    }

    void Airmouse::thread_loop()
    {
        int epfd = epoll_create1(EPOLL_CLOEXEC);
        std::map<std::string, int> open_fds;   // node -> fd
        std::map<int, std::pair<bool, uint64_t>> held;   // msc code -> (pressed, t)
        auto now_us = []() -> uint64_t {
            struct timespec ts{};
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return (uint64_t) ts.tv_sec * 1000000ull + ts.tv_nsec / 1000ull;
        };
        auto add = [&](const std::string &name) {
            if (name.rfind("event", 0) != 0 || open_fds.count(name)) return;
            std::string path = "/dev/input/" + name;
            int fd = -1;
            for (int i = 0; i < 20 && fd < 0; i++) {
                fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd < 0) usleep(50000);
            }
            if (fd < 0) return;
            std::string n;
            if (!matches(name, match_, &n)) {
                close(fd);
                return;
            }
            struct epoll_event ev{};
            ev.events = EPOLLIN;
            ev.data.fd = fd;
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0) { close(fd); return; }
            open_fds[name] = fd;
            {
                std::lock_guard l(name_mu_);
                devname_ = n;
            }
            connected_ = true;
            Foyer::logger::info("airmouse: matched {} ({})", name, n);
        };
        auto drop = [&](const std::string &name) {
            auto it = open_fds.find(name);
            if (it == open_fds.end()) return;
            epoll_ctl(epfd, EPOLL_CTL_DEL, it->second, nullptr);
            close(it->second);
            open_fds.erase(it);
            if (open_fds.empty()) {
                connected_ = false;
                std::lock_guard l(name_mu_);
                devname_.clear();
            }
        };
        if (DIR *dir = opendir("/dev/input")) {
            while (struct dirent *ent = readdir(dir)) add(ent->d_name);
            closedir(dir);
        }
        int ino = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (ino >= 0) {
            inotify_add_watch(ino, "/dev/input", IN_CREATE | IN_DELETE);
            struct epoll_event ev{};
            ev.events = EPOLLIN;
            ev.data.fd = ino;
            epoll_ctl(epfd, EPOLL_CTL_ADD, ino, &ev);
        }
        if (open_fds.empty())
            Foyer::logger::info("airmouse: no matching device (match '{}')", match_);

        struct epoll_event events[16];
        uint64_t last_sweep = 0;
        std::map<int, uint64_t> key_last;   // EV_KEY press dedup (per code)
        while (running_) {
            int n = epoll_wait(epfd, events, 16, 200);
            uint64_t now = now_us();
            if (now - last_sweep > 1000) {
                last_sweep = now;
                std::vector<int> rel;
                sweep(held, now, 500000, &rel);   // now is µs: release a stuck press after 500 ms
                for (int code : rel) if (cb_) cb_(code, false);
            }
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
                            else if (ie->mask & IN_DELETE) drop(name);
                            p += sizeof(struct inotify_event) + ie->len;
                        }
                    }
                    continue;
                }
                input_event e;
                ssize_t r;
                while ((r = read(fd, &e, sizeof(e))) == sizeof(e)) {
                    if (e.type == EV_MSC && e.code == MSC_SCAN) {
                        int edge = step(e.value, now, held);
                        if (edge != 0 && cb_) cb_(e.value, edge > 0);
                    }
                    else if (e.type == EV_KEY && e.value == 1 && e.code != 0) {
                        // Some air mice send real keys; report press edges the
                        // same way (releases are irrelevant for our actions).
                        // The same key can arrive on several nodes: dedup.
                        uint64_t &last = key_last[e.code];
                        if (now - last > DEDUP_US) { last = now; if (cb_) cb_(e.code, true); }
                    }
                }
                if (r < 0 && errno == ENODEV) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    for (auto it = open_fds.begin(); it != open_fds.end(); ++it)
                        if (it->second == fd) {
                            open_fds.erase(it);
                            if (open_fds.empty()) {
                                connected_ = false;
                                std::lock_guard l(name_mu_);
                                devname_.clear();
                            }
                            break;
                        }
                    close(fd);
                }
            }
        }
        for (auto &[k, fd] : open_fds) close(fd);
        if (ino >= 0) close(ino);
        close(epfd);
    }
}
