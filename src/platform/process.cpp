#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <poll.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <filesystem>

#include "process.hpp"
#include "../render/logger.hpp"

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

namespace Foyer {
    static int open_pidfd(int pid)
    {
#ifdef pidfd_open
        int fd = pidfd_open(pid, 0);
        if (fd >= 0) return fd;
        if (errno != ENOSYS) return -1;
#endif
        return (int) syscall(SYS_pidfd_open, pid, 0);
    }
}

Foyer::Process::Process(Process &&o) noexcept
{
    pid_ = o.pid_;
    pidfd_ = o.pidfd_;
    status_ = o.status_;
    reaped_ = o.reaped_;
    o.pid_ = -1;
    o.pidfd_ = -1;
    o.reaped_ = false;
}

Foyer::Process &Foyer::Process::operator=(Process &&o) noexcept
{
    if (this != &o) {
        release();
        pid_ = o.pid_;
        pidfd_ = o.pidfd_;
        status_ = o.status_;
        reaped_ = o.reaped_;
        o.pid_ = -1;
        o.pidfd_ = -1;
        o.reaped_ = false;
    }
    return *this;
}

Foyer::Process::~Process()
{
    release();
}

void Foyer::Process::release()
{
    if (pidfd_ >= 0)
        close(pidfd_);
    pidfd_ = -1;
    // If we still own a live child, reap it to avoid zombies
    if (pid_ > 0 && !reaped_) {
        int status;
        while (waitpid(pid_, &status, WNOHANG) > 0) {
            status_ = status;
            reaped_ = true;
        }
    }
    pid_ = -1;
}

Foyer::Process Foyer::Process::spawn(const std::string &command, const std::string &log_path)
{
    if (!log_path.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(log_path).parent_path(), ec);
    }
    pid_t pid = fork();
    if (pid < 0) {
        Foyer::logger::error("Could not fork new process");
        return Process();
    }
    if (pid == 0) {
        // Child: new session so we can kill the whole group later
        if (setsid() < 0)
            _exit(127);
        if (!log_path.empty()) {
            int fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
            }
        }
        const char *args[] = {"/bin/sh", "-c", command.c_str(), nullptr};
        execvp("/bin/sh", (char* const*) args);
        _exit(127);  // exec failed
    }
    // Parent
    Process p;
    p.pid_ = pid;
    p.pidfd_ = open_pidfd(pid);
    if (p.pidfd_ < 0)
        Foyer::logger::debug("pidfd_open failed for pid {} (errno={}), using waitpid", pid, errno);
    return p;
}

bool Foyer::Process::poll()
{
    if (!alive())
        return false;
    if (pidfd_ >= 0) {
        struct pollfd pfd{pidfd_, POLLIN, 0};
        int r = ::poll(&pfd, 1, 0);
        if (r > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
            int status;
            if (waitpid(pid_, &status, 0) == pid_) {
                status_ = status;
                reaped_ = true;
                close(pidfd_);
                pidfd_ = -1;
                return true;
            }
        }
        return false;
    }
    // Fallback: non-blocking waitpid
    int status;
    if (waitpid(pid_, &status, WNOHANG) == pid_) {
        status_ = status;
        reaped_ = true;
        return true;
    }
    return false;
}

int Foyer::Process::exit_status() const
{
    if (!reaped_) return -1;
    if (WIFEXITED(status_)) return WEXITSTATUS(status_);
    if (WIFSIGNALED(status_)) return 128 + WTERMSIG(status_);
    return -1;
}

void Foyer::Process::kill(int sig)
{
    if (alive())
        kill_group(pid_, sig);
}

void Foyer::Process::kill_group(int pgid, int sig)
{
    if (::kill(-pgid, sig) < 0) {
        if (errno == ESRCH) {
            // Group not found (child may not have called setsid yet); kill the pid directly
            ::kill(pgid, sig);
        } else {
            Foyer::logger::debug("kill group {} failed: {}", pgid, strerror(errno));
        }
    }
}
