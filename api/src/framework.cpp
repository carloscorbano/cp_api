#include "cp_api/framework.hpp"
#include "cp_api/core/debug.hpp"
#include "cp_api/window/window.hpp"
#include "cp_api/world/world.hpp"
#include "cp_api/core/threadPool.hpp"

namespace cp_api {
    Framework::Framework() {
        CP_LOG_INFO("Framework constructed.");
    }

    Framework::~Framework() {
        CP_LOG_INFO("Framework destructed.");
    }

    void Framework::Init() {
        m_window = std::make_unique<Window>(800, 600, "CP_API Window");
        m_threadPool = std::make_unique<ThreadPool>();
        m_diagnostics = std::make_unique<DiagnosticsManager>();
        m_world = std::make_unique<World>();

        CP_LOG_INFO("Framework initialized.");
        m_isInitialized = true;
    }

    void Framework::Run() {
        if (!m_isInitialized) {
            CP_LOG_ERROR("Framework not initialized. Call Init() before Run().");
            return;
        }

        m_isRunning = true;

        CP_LOG_INFO("Framework running.");

        double dt = 0.0;

        const double fixedDelta = 1.0 / 60.0;
        double accumulator = 0.0;
        auto lastTime = std::chrono::high_resolution_clock::now();

        while(!m_window->ShouldClose() && m_isRunning) {
            m_diagnostics->BeginFrame();

            m_diagnostics->StartTimer("Frame");
            {
                m_diagnostics->StartTimer("WindowUpdate");
                {
                    m_window->Update();
                }
                m_diagnostics->StopTimer("WindowUpdate");
                
                // Calcular deltaTime
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> frameTime = now - lastTime;
                lastTime = now;

                dt = frameTime.count();
                if (dt > 0.25) dt = 0.25;
                accumulator += dt;

                const int maxSubSteps = 5;
                int steps = 0;

                while(accumulator >= fixedDelta && steps < maxSubSteps) {
                    m_diagnostics->StartTimer("FixedUpdate");
                    {
                        m_world->FixedUpdate(fixedDelta);
                    }
                    m_diagnostics->StopTimer("FixedUpdate");

                    accumulator -= fixedDelta;
                    steps++;
                }

                //implement render method
                // float alpha = static_cast<float>(accumulator / fixedDelta);

                m_diagnostics->StartTimer("WorldUpdate");
                {
                    m_world->Update(dt);
                }
                m_diagnostics->StopTimer("WorldUpdate");

                m_diagnostics->StartTimer("WindowWorldProcess");
                {
                    m_window->ProcessWorld(*m_world);
                }
            }
            m_diagnostics->StopTimer("Frame");

            m_diagnostics->EndFrame();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // CP_LOG_DEBUG("{}", m_diagnostics->Summary());
        }
    }
} // namespace cp_api