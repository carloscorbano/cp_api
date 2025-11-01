#pragma once

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <random>
#include <memory>

namespace cp_api {

enum class TaskPriority { HIGH, NORMAL, LOW };

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template<typename Func, typename... Args>
    auto Submit(TaskPriority priority, Func&& f, Args&&... args)
        -> std::future<decltype(f(args...))>;

    void Shutdown();

private:
    void WorkerLoop(size_t index);

private:
    std::vector<std::deque<std::function<void()>>> m_queues;
    std::vector<std::mutex> m_mutexes;
    std::vector<std::condition_variable> m_conditions;
    std::vector<std::thread> m_workers;
    std::atomic_bool m_running;

    std::mt19937 m_rng{ std::random_device{}() };
    std::uniform_int_distribution<size_t> m_dist;
};

// ---------------- Template Implementation ----------------

template<typename Func, typename... Args>
auto ThreadPool::Submit(TaskPriority priority, Func&& f, Args&&... args)
    -> std::future<decltype(f(args...))>
{
    using ReturnType = decltype(f(args...));

    if (!m_running.load(std::memory_order_acquire))
        throw std::runtime_error("ThreadPool is shut down");

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Func>(f), std::forward<Args>(args)...)
    );
    std::future<ReturnType> future = task->get_future();

    size_t idx = m_dist(m_rng);
    {
        std::lock_guard<std::mutex> lock(m_mutexes[idx]);
        if(priority == TaskPriority::HIGH)
            m_queues[idx].emplace_front([task]{ (*task)(); });
        else
            m_queues[idx].emplace_back([task]{ (*task)(); });
    }

    m_conditions[idx].notify_one();
    return future;
}

} // namespace cp_api
