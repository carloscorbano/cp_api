#pragma once

#include "core/diagnostics.hpp"
#include <iostream>
#include <memory>

namespace cp_api {
    class Window;
    class World;
    class ThreadPool;
    class Framework {
    public:
        Framework();
        ~Framework();

        void Init();
        void Run();
    private:
        bool m_isInitialized = false;
        bool m_isRunning = false;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<ThreadPool> m_threadPool;
        std::unique_ptr<DiagnosticsManager> m_diagnostics;
        std::unique_ptr<World> m_world;
    };
} // namespace api
