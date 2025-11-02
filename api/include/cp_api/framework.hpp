#pragma once

#include "core/diagnostics.hpp"
#include <iostream>
#include <memory>

namespace cp_api {
    class Window;
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
        std::unique_ptr<DiagnosticsManager> m_diagnostics;
    };
} // namespace api
