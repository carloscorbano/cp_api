#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <queue>
#include <optional>
#include <thread>
#include <condition_variable>

namespace cp_api {

    // ---------------------------
    // Base de evento
    // ---------------------------
    struct Event {
        virtual ~Event() = default;
    };

    // ID para listener
    using ListenerID = uint64_t;

    // ---------------------------
    // EventDispatcher avançado
    // ---------------------------
    class EventDispatcher {
    public:
        EventDispatcher() : m_nextListenerID(1) {}

        ~EventDispatcher() {
            StopAsync();
        }

        // Subscribes com prioridade opcional (default 0)
        template <typename EventType>
        ListenerID Subscribe(std::function<void(const EventType&)> callback, int priority = 0)
        {
            const std::type_index type = typeid(EventType);
            std::unique_lock lock(m_mutex);

            ListenerID id = m_nextListenerID++;
            auto wrapper = [callback](const Event& e) {
                callback(static_cast<const EventType&>(e));
            };

            ListenerEntry entry{id, priority, wrapper};

            auto& vec = m_listeners[type];
            vec.push_back(entry);

            // ordena por prioridade decrescente
            std::sort(vec.begin(), vec.end(),
                      [](const ListenerEntry& a, const ListenerEntry& b) {
                          return a.priority > b.priority;
                      });

            return id;
        }

        // Remove listener específico
        template <typename EventType>
        void Unsubscribe(ListenerID id)
        {
            const std::type_index type = typeid(EventType);
            std::unique_lock lock(m_mutex);

            auto it = m_listeners.find(type);
            if (it == m_listeners.end()) return;

            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                     [id](const ListenerEntry& e){ return e.id == id; }),
                      vec.end());
        }

        // Dispara evento imediatamente
        template <typename EventType>
        void Emit(const EventType& event)
        {
            const std::type_index type = typeid(EventType);
            std::unique_lock lock(m_mutex);

            auto it = m_listeners.find(type);
            if (it == m_listeners.end()) return;

            for (auto& entry : it->second)
                entry.callback(event);
        }

        // ---------------------------
        // Suporte assíncrono
        // ---------------------------
        template <typename EventType>
        void QueueEvent(const EventType& event)
        {
            std::unique_lock lock(m_queueMutex);
            m_eventQueue.push(std::make_shared<EventWrapperTyped<EventType>>(event));
            m_cv.notify_one();
        }

        void StartAsync()
        {
            m_running = true;
            m_thread = std::thread([this]() { ProcessQueue(); });
        }

        void StopAsync()
        {
            if (m_running)
            {
                m_running = false;
                m_cv.notify_one();
                if (m_thread.joinable())
                    m_thread.join();
            }
        }

    private:
        struct ListenerEntry {
            ListenerID id;
            int priority;
            std::function<void(const Event&)> callback;
        };

        std::unordered_map<std::type_index, std::vector<ListenerEntry>> m_listeners;
        std::mutex m_mutex;
        std::atomic<ListenerID> m_nextListenerID;

        // ---------------------------
        // Fila assíncrona
        // ---------------------------
        struct EventWrapper {
            virtual ~EventWrapper() = default;
            virtual void Dispatch(EventDispatcher* dispatcher) = 0;
        };

        template <typename T>
        struct EventWrapperTyped : EventWrapper {
            T event;
            EventWrapperTyped(const T& e) : event(e) {}
            void Dispatch(EventDispatcher* dispatcher) override { dispatcher->Emit(event); }
        };

        std::queue<std::shared_ptr<EventWrapper>> m_eventQueue;
        std::mutex m_queueMutex;
        std::condition_variable m_cv;
        std::thread m_thread;
        std::atomic<bool> m_running{false};

        void ProcessQueue()
        {
            while (m_running)
            {
                std::shared_ptr<EventWrapper> ev;

                {
                    std::unique_lock lock(m_queueMutex);
                    m_cv.wait(lock, [this]() { return !m_eventQueue.empty() || !m_running; });
                    if (!m_running) break;

                    ev = m_eventQueue.front();
                    m_eventQueue.pop();
                }

                if (ev) ev->Dispatch(this);
            }
        }
    };

} // namespace cp_api
