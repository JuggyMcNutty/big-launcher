#include <cstdio>
#include <cstdint>

#include <SDL3/SDL.h>

#include "job_queue.hpp"
#include "../render/logger.hpp"

BL::JobQueue::JobQueue()
{
    worker_ = std::thread(&JobQueue::worker_loop, this);
}

BL::JobQueue::~JobQueue()
{
    stop();
}

void BL::JobQueue::stop()
{
    {
        std::unique_lock lock(queue_mutex_);
        if (stop_) return;
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

uint32_t BL::JobQueue::run(const std::string &command, std::function<void(const JobResult&)> on_done)
{
    return run_stream(command, nullptr, std::move(on_done));
}

uint32_t BL::JobQueue::run_stream(const std::string &command,
                                  std::function<void(const std::string&)> on_progress,
                                  std::function<void(const JobResult&)> on_done)
{
    uint32_t id;
    auto job = std::make_shared<Job>();
    {
        std::unique_lock lock(jobs_mutex_);
        id = next_id_++;
        job->id = id;
        job->command = command;
        job->on_done = std::move(on_done);
        job->on_progress = std::move(on_progress);
        jobs_[id] = job;
        queue_.push_back(id);
    }
    cv_.notify_one();
    BL::logger::debug("Job {} queued: {}", id, command);
    return id;
}

void BL::JobQueue::worker_loop()
{
    while (true) {
        uint32_t id;
        {
            std::unique_lock lock(queue_mutex_);
            cv_.wait(lock, [&]{ return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty())
                break;
            id = queue_.front();
            queue_.pop_front();
        }
        std::shared_ptr<Job> job;
        {
            std::unique_lock lock(jobs_mutex_);
            auto it = jobs_.find(id);
            if (it != jobs_.end())
                job = it->second;
        }
        if (job)
            execute(job);
    }
}

void BL::JobQueue::execute(std::shared_ptr<Job> job)
{
    FILE *fp = popen(job->command.c_str(), "r");
    if (!fp) {
        std::unique_lock lock(job->mutex);
        job->result.ok = false;
        job->result.exit_code = -1;
        job->result.output = "popen failed";
    }
    else {
        char buf[2048];
        std::string line;
        bool streaming = (bool) job->on_progress;
        while (fgets(buf, sizeof(buf), fp)) {
            line += buf;
            if (line.size() >= sizeof(buf)) {
                // very long line; flush what we have
                std::unique_lock lock(job->mutex);
                job->output += line;
                if (streaming) job->progress.push_back(line);
                if (streaming) {
                    SDL_Event e; e.type = EVT_JOB_PROGRESS; e.user.data1 = (void*)(intptr_t) job->id;
                    SDL_PushEvent(&e);
                }
                line.clear();
            }
            else if (line.back() == '\n') {
                line.pop_back();
                std::unique_lock lock(job->mutex);
                job->output += line + "\n";
                if (streaming) job->progress.push_back(line);
                if (streaming) {
                    SDL_Event e; e.type = EVT_JOB_PROGRESS; e.user.data1 = (void*)(intptr_t) job->id;
                    SDL_PushEvent(&e);
                }
                line.clear();
            }
        }
        if (!line.empty()) {
            std::unique_lock lock(job->mutex);
            job->output += line;
            if (streaming) job->progress.push_back(line);
        }
        int status = pclose(fp);
        std::unique_lock lock(job->mutex);
        job->result.ok = (status == 0);
        job->result.exit_code = status;
        job->result.output = job->output;
    }
    SDL_Event e; e.type = EVT_JOB_DONE; e.user.data1 = (void*)(intptr_t) job->id;
    SDL_PushEvent(&e);
}

bool BL::JobQueue::handle_event(const SDL_Event &event)
{
    if (event.type != EVT_JOB_DONE && event.type != EVT_JOB_PROGRESS)
        return false;
    uint32_t id = (uint32_t)(intptr_t) event.user.data1;
    std::shared_ptr<Job> job;
    {
        std::unique_lock lock(jobs_mutex_);
        auto it = jobs_.find(id);
        if (it == jobs_.end())
            return true;
        job = it->second;
    }
    if (event.type == EVT_JOB_PROGRESS) {
        std::string line;
        {
            std::unique_lock lock(job->mutex);
            if (!job->progress.empty()) {
                line = job->progress.front();
                job->progress.pop_front();
            }
        }
        if (!line.empty() && job->on_progress)
            job->on_progress(line);
    }
    else {  // JOB_DONE
        JobResult result;
        std::function<void(const JobResult&)> on_done;
        {
            std::unique_lock lock(job->mutex);
            result = job->result;
            on_done = job->on_done;
        }
        jobs_.erase(id);
        if (on_done)
            on_done(result);
    }
    return true;
}
