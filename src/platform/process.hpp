#pragma once

#include <string>

namespace BL {
    // Manages a launched child process: fork + setsid, reaping, kill-by-group.
    // The app's running state is derived from Process::alive() (no focus
    // heuristics).
    class Process {
    public:
        // Launch a command via /bin/sh -c in a new session. Returns a Process
        // (pid()==-1 on failure).
        static Process spawn(const std::string &command);
        ~Process();

        bool alive() const { return pid_ > 0 && !reaped_; }
        int  pid() const { return pid_; }
        int  exit_status() const;   // 0 if clean, else signal/exit code
        bool exited() const { return reaped_; }
        void kill(int sig = 15);    // SIGTERM the process group by default
        static void kill_group(int pgid, int sig = 15);

        // Reap if the process has exited. Returns true if it just exited.
        bool poll();

        Process() = default;
        Process(Process &&o) noexcept;
        Process &operator=(Process &&o) noexcept;
        Process(const Process &) = delete;
        Process &operator=(const Process &) = delete;

    private:
        int pid_ = -1;
        int pidfd_ = -1;
        int status_ = 0;
        bool reaped_ = false;
        void release();
    };
}
