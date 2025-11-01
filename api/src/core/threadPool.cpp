#include "cp_api/core/threadPool.hpp"
#include <iostream>

namespace cp_api {

ThreadPool::ThreadPool(size_t threadCount)
    : m_running(true),
      m_queues(threadCount),
      m_mutexes(threadCount),
      m_conditions(threadCount),
      m_dist(0, threadCount - 1)
{
    for(size_t i = 0; i < threadCount; ++i)
        m_workers.emplace_back(&ThreadPool::WorkerLoop, this, i);
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

void ThreadPool::Shutdown() {
    m_running = false;
    for(auto& cond : m_conditions)
        cond.notify_all();

    for(auto& t : m_workers)
        if(t.joinable())
            t.join();
}

void ThreadPool::WorkerLoop(size_t index) {
    while(m_running) {
        std::function<void()> task;

        // Tenta pegar da própria fila
        {
            std::unique_lock<std::mutex> lock(m_mutexes[index]);
            m_conditions[index].wait(lock, [&]{
                return !m_queues[index].empty() || !m_running;
            });

            if(!m_running && m_queues[index].empty())
                return;

            if(!m_queues[index].empty()) {
                task = std::move(m_queues[index].front());
                m_queues[index].pop_front();
            }
        }

        // Work-stealing
        if(!task) {
            for(size_t i = 0; i < m_queues.size(); ++i) {
                if(i == index) continue;
                std::lock_guard<std::mutex> lock(m_mutexes[i]);
                if(!m_queues[i].empty()) {
                    task = std::move(m_queues[i].back());
                    m_queues[i].pop_back();
                    break;
                }
            }
        }

        if(task) task();
    }
}

} // namespace cp_api
