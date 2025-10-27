#pragma once
#include <functional>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <type_traits>

#ifdef _DEBUG
#include "cp_api/core/debug.hpp"
#define CP_DELEGATE_LOG(...) CP_LOG_INFO(__VA_ARGS__)
#define CP_DELEGATE_LOG_DEBUG(...) CP_LOG_DEBUG(__VA_ARGS__)
#define CP_DELEGATE_LOG_WARN(...) CP_LOG_WARN(__VA_ARGS__)
#else
#define CP_DELEGATE_LOG(...) (void)0
#define CP_DELEGATE_LOG_DEBUG(...) (void)0
#define CP_DELEGATE_LOG_WARN(...) (void)0
#endif

namespace cp_api {

    // ========================================================
    // SINGLE DELEGATE
    // ========================================================
    template<typename Signature>
    class Delegate;

    template<typename R, typename... Args>
    class Delegate<R(Args...)>
    {
    public:
        using FuncType = std::function<R(Args...)>;

        Delegate() = default;

         // Construtor a partir de função livre ou callable compatível
        Delegate(FuncType f) : func_(std::move(f)) {}

        template<typename T>
        static Delegate<T> FromFunction(const std::function<T>& func) {
            Delegate<T> del;
            del.SetFunction(func); // depende de como você implementou Delegate
            return del;
        }

        // Factory para lambda ou std::function
        static Delegate FromLambda(FuncType f) {
            return Delegate(std::move(f));
        }

        // Bind função/lambda
        template<typename F>
        void Bind(F&& f)
        {
            func_ = std::forward<F>(f);
            instance_ptr_ = nullptr;
            method_id_ = 0;
            CP_DELEGATE_LOG_DEBUG("[Delegate] Bound Lambda/Callable");
        }

        // Bind método de instância
        template<typename T>
        void Bind(T* instance, R(T::*method)(Args...))
        {
            func_ = [instance, method](Args... args) -> R {
                return (instance->*method)(std::forward<Args>(args)...);
            };
            instance_ptr_ = instance;
            method_id_ = MethodId(method);
            CP_DELEGATE_LOG_DEBUG("[Delegate] Bound Method -> instance={} method={}", (void*)instance, typeid(method).name());
        }

        // Bind método const
        template<typename T>
        void Bind(const T* instance, R(T::*method)(Args...) const)
        {
            func_ = [instance, method](Args... args) -> R {
                return (instance->*method)(std::forward<Args>(args)...);
            };
            instance_ptr_ = const_cast<T*>(instance);
            method_id_ = MethodId(method);
            CP_DELEGATE_LOG_DEBUG("[Delegate] Bound Const Method -> instance={} method={}", (void*)instance, typeid(method).name());
        }

        void Unbind()
        {
            func_ = nullptr;
            instance_ptr_ = nullptr;
            method_id_ = 0;
            CP_DELEGATE_LOG_DEBUG("[Delegate] Unbind");
        }

        bool Empty() const { return !static_cast<bool>(func_); }

        void operator()(Args... args) const { Invoke(std::forward<Args>(args)...); }

        R Invoke(Args... args) const
        {
            if (func_) {
                if constexpr (std::is_void_v<R>)
                    func_(std::forward<Args>(args)...);
                else
                    return func_(std::forward<Args>(args)...);
            }
        }

        bool operator==(const Delegate& other) const
        {
            return instance_ptr_ == other.instance_ptr_ && method_id_ == other.method_id_;
        }

    private:
        template<typename T, typename M>
        static size_t MethodId(M T::*method)
        {
            return reinterpret_cast<size_t>(*(void**)&method);
        }

        FuncType func_;
        void* instance_ptr_ = nullptr;
        size_t method_id_ = 0;
    };

    // ========================================================
    // MULTICAST DELEGATE
    // ========================================================
    template<typename Signature>
    class MulticastDelegate;

    template<typename R, typename... Args>
    class MulticastDelegate<R(Args...)>
    {
    public:
        using DelegateType = Delegate<R(Args...)>;

        struct Entry
        {
            DelegateType delegate;
            int32_t priority = 0;
            uint64_t callCount = 0;
        };

        // Adiciona delegate pronto
        void Add(const DelegateType& del, int32_t priority = 0)
        {
            std::scoped_lock lock(mutex_);
            entries_.push_back({ del, priority, 0 });
            SortEntries();
            CP_DELEGATE_LOG("[MulticastDelegate] Added delegate -> total={}, priority={}", entries_.size(), priority);
        }

        // Bind função/lambda
        template<typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, DelegateType>>>
        void Add(F&& f, int32_t priority = 0)
        {
            DelegateType del;
            del.Bind(std::forward<F>(f));
            Add(del, priority);
        }

        // Bind método de instância
        template<typename T>
        void Add(T* instance, R(T::*method)(Args...), int32_t priority = 0)
        {
            DelegateType del;
            del.Bind(instance, method);
            Add(del, priority);
        }

        // Bind método const
        template<typename T>
        void Add(const T* instance, R(T::*method)(Args...) const, int32_t priority = 0)
        {
            DelegateType del;
            del.Bind(instance, method);
            Add(del, priority);
        }

        // Remove delegate específico
        void Remove(const DelegateType& del)
        {
            std::scoped_lock lock(mutex_);
            size_t before = entries_.size();

            entries_.erase(
                std::remove_if(entries_.begin(), entries_.end(),
                    [&](const Entry& e) { return e.delegate == del; }),
                entries_.end()
            );

            size_t removed = before - entries_.size();
            if (removed > 0)
                CP_DELEGATE_LOG("[MulticastDelegate] Removed {} delegate(s), remaining={}", removed, entries_.size());
        }

        // Remove método de instância
        template<typename T>
        void Remove(T* instance, R(T::*method)(Args...))
        {
            DelegateType tmp;
            tmp.Bind(instance, method);
            Remove(tmp);
        }

        // Remove método const
        template<typename T>
        void Remove(const T* instance, R(T::*method)(Args...) const)
        {
            DelegateType tmp;
            tmp.Bind(instance, method);
            Remove(tmp);
        }

        void Clear()
        {
            std::scoped_lock lock(mutex_);
            CP_DELEGATE_LOG("[MulticastDelegate] Clearing all delegates -> total before clear = {}", entries_.size());
            entries_.clear();
        }

        bool Empty() const
        {
            std::scoped_lock lock(mutex_);
            return entries_.empty();
        }

        void operator()(Args... args)
        {
            std::scoped_lock lock(mutex_);
            const size_t total = entries_.size();

            if (total == 0) {
                CP_DELEGATE_LOG("=== Emissão abortada: nenhum delegate registrado ===");
                return;
            }

            CP_DELEGATE_LOG("=== Emitindo MulticastDelegate -> total delegates = {} ===", total);
            size_t idx = 1;

            for (auto& e : entries_) {
                // Índice e chamada
                CP_DELEGATE_LOG_DEBUG("[CALL {}/{}] Invocando delegate", idx, total);

                // Invoca delegate
                e.delegate.Invoke(std::forward<Args>(args)...);

                // Incrementa contador
                e.callCount++;

                // Log específico do delegate (info ou debug)
                if (e.delegate.Empty()) {
                    CP_DELEGATE_LOG_WARN("Delegate vazio no índice {}", idx);
                }

                idx++;
            }

            // Contadores resumidos após emissão
            CP_DELEGATE_LOG("Contadores após emissão:");
            idx = 1;
            for (auto& e : entries_) {
                CP_DELEGATE_LOG("    [{}] callCount = {}", idx, e.callCount);
                idx++;
            }

            CP_DELEGATE_LOG("=== Fim da emissão ===");
        }

        const std::vector<Entry>& GetEntries() const { return entries_; }

    private:
        void SortEntries()
        {
            std::sort(entries_.begin(), entries_.end(),
                [](const Entry& a, const Entry& b) { return a.priority > b.priority; });
        }

        mutable std::mutex mutex_;
        std::vector<Entry> entries_;
    };

} // namespace cp_api
