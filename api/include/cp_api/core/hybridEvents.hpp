#pragma once

#include "cp_api/core/events.hpp"
#include "cp_api/core/delegate.hpp"
#include <type_traits>

namespace cp_api {
    // =======================================
    // HybridEventDispatcher
    // Combina EventDispatcher assíncrono com Delegates
    // =======================================
    class HybridEventDispatcher : public EventDispatcher {
    public:
        HybridEventDispatcher() = default;
        ~HybridEventDispatcher() = default;

        // ---------------------------
        // Subscribe usando Delegate
        // ---------------------------
        template<typename EventType>
        ListenerID Subscribe(const Delegate<void(const EventType&)>& del, int priority = 0)
        {
            // Captura o delegate para lambda
            Delegate<void(const EventType&)> localDel = del;

            return this->EventDispatcher::template Subscribe<EventType>(
                [localDel](const EventType& e) {
                    localDel.Invoke(e);
                }, priority
            );
        }

        // ---------------------------
        // Subscribe usando qualquer callable (lambda, functor)
        // ---------------------------
        template<typename EventType, typename F>
        ListenerID Subscribe(F&& callback, int priority = 0)
        {
            return this->EventDispatcher::template Subscribe<EventType>(
                std::forward<F>(callback), priority
            );
        }

        // ---------------------------
        // Remove listener
        // ---------------------------
        template<typename EventType>
        void Unsubscribe(ListenerID id)
        {
            this->EventDispatcher::Unsubscribe<EventType>(id);
        }

        // ---------------------------
        // Emit (síncrono)
        // ---------------------------
        template<typename EventType>
        void Emit(const EventType& e)
        {
            this->EventDispatcher::Emit<EventType>(e);
        }

        // ---------------------------
        // QueueEvent (assíncrono)
        // ---------------------------
        template<typename EventType>
        void QueueEvent(const EventType& e)
        {
            this->EventDispatcher::QueueEvent<EventType>(e);
        }

        // ---------------------------
        // Start/Stop thread assíncrono
        // ---------------------------
        void StartAsync() { EventDispatcher::StartAsync(); }
        void StopAsync()  { EventDispatcher::StopAsync(); }
    };
}