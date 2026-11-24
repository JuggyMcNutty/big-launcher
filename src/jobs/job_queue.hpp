#pragma once

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>

namespace BL {
    struct JobResult {
        bool ok = false;
        int exit_code = -1;
        std::string output;
    };

    // Runs system commands on a worker thread; results are delivered to the
    // main loop via SDL user events (pump()). Streaming jobs post progress
    // lines as they arrive. Nothing blocks the UI thread.
    class JobQueue {
    public:
        JobQueue();
        ~JobQueue();

        // Run a command; on_done is invoked on the main thread (via pump()).
        uint32_t run(const std::string &command, std::function<void(const JobResult&)> on_done);

        // Run a command, streaming stdout lines to on_progress as they arrive.
        uint32_t run_stream(const std::string &command,
                            std::function<void(const std::string&)> on_progress,
                            std::function<void(const JobResult&)> on_done);

        // Call from the main loop when a job user-event is received.
        // Returns true if the event was a job event.
        bool handle_event(const SDL_Event &event);
        void stop();

        static constexpr uint32_t EVT_JOB_DONE     = 0x80000001;
        static constexpr uint32_t EVT_JOB_PROGRESS = 0x80000002;

    private:
        struct Job {
            uint32_t id = 0;
            std::string command;
            std::function<void(const JobResult&)> on_done;
            std::function<void(const std::string&)> on_progress;
            std::string output;
            std::deque<std::string> progress;
            JobResult result;
            std::mutex mutex;
        };

        void worker_loop();
        void execute(std::shared_ptr<Job> job);

        std::map<uint32_t, std::shared_ptr<Job>> jobs_;
        std::mutex jobs_mutex_;
        std::deque<uint32_t> queue_;
        std::mutex queue_mutex_;
        std::condition_variable cv_;
        std::thread worker_;
        uint32_t next_id_ = 1;
        bool stop_ = false;
    };
}
