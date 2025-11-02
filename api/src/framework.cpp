#include "cp_api/framework.hpp"
#include "cp_api/core/debug.hpp"
#include "cp_api/window/window.hpp"

namespace cp_api {
    Framework::Framework() {
        CP_LOG_INFO("Framework constructed.");
    }

    Framework::~Framework() {
        CP_LOG_INFO("Framework destructed.");
    }

    void Framework::Init() {
        m_window = std::make_unique<Window>(800, 600, "CP_API Window");
        m_diagnostics = std::make_unique<DiagnosticsManager>();

        CP_LOG_INFO("Framework initialized.");
        m_isInitialized = true;
    }

    void Framework::Run() {
        if (!m_isInitialized) {
            CP_LOG_ERROR("Framework not initialized. Call Init() before Run().");
            return;
        }

        CP_LOG_INFO("Framework running.");

        double dt = 0.0;

        while(m_isRunning = !m_window->ShouldClose()) {
            m_diagnostics->BeginFrame();
            
            m_window->Update();

            m_diagnostics->EndFrame();
            dt = m_diagnostics->GetFrameData().timeInfo.deltaTime;
        }
    }
} // namespace cp_api