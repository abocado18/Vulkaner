// my ThreadPool Implementation
#pragma once
#include <cinttypes>
#include <thread>
#include <queue>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>

namespace thread_pool
{
    class ThreadPool
    {
    public:
        ThreadPool(size_t thread_count = std::thread::hardware_concurrency()) : stop_flag(false)
        {
            for (size_t i = 0; i < thread_count; i++)
            {
                workers.emplace_back(&ThreadPool::workerLoop, this);
            }
        }

        ~ThreadPool()
        {
            stop();
        }

        void enqueue(std::function<void()> job)
        {

            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                job_queue.push(std::move(job));
            }

            condition.notify_one();
        }

        void stop()
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop_flag = true;
            }

            condition.notify_all();
            for (auto &worker : workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }

    private:
        void workerLoop()
        {
            while (true)
            {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [&]()
                                   { return stop_flag || !job_queue.empty(); });

                    if (stop_flag || job_queue.empty())
                        return;

                    job = std::move(job_queue.front());
                    job_queue.pop();
                }
                job();
            }
        }

        std::vector<std::thread> workers;
        std::queue<std::function<void()>> job_queue;
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop_flag;
    };
}
