#include "cp_api/framework.hpp"
#include "cp_api/core/debug.hpp"

#include "cp_api/graphics/window.hpp"
#include "cp_api/graphics/vulkan.hpp"

#include "cp_api/world/world.hpp"

#include "cp_api/core/threadPool.hpp"

#include "cp_api/components/uiComponent.hpp"
#include "cp_api/components/cameraComponent.hpp"
#include "cp_api/components/transformComponent.hpp"
#include "cp_api/components/rendererComponent.hpp"

#include "cp_api/graphics/vkBuffer.hpp"

namespace cp_api {
    Framework::Framework() {
        CP_LOG_INFO("Framework constructed.");
    }

    Framework::~Framework() {
        CP_LOG_INFO("Framework destructed.");
    }

    void Framework::Init() {
        m_threadPool = std::make_unique<ThreadPool>();
        m_diagnostics = std::make_unique<DiagnosticsManager>();
        m_world = std::make_unique<World>();
        m_window = std::make_unique<Window>(800, 600, "CP_API Window", *m_world, *m_threadPool);

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

#ifndef NDEBUG
        //draw diagnostics UI
        auto& reg = m_world->GetRegistry();

        auto e = reg.create();
        UICanvas& canvas = reg.emplace<UICanvas>(e);
        canvas.name = "Diagnostics";
        canvas.size = ImVec2(450, 150);
        auto& t = canvas.AddChild<UIText>();
        t.text = "";

        double timerUpdate = 0.0;
#endif

        //-----------------------------------------------------------------------------------
        // TEST AREA
        //-----------------------------------------------------------------------------------
        std::vector<uint32_t> data = { 0, 1, 2, 3, 4, 5, 6, 7,8, 9, 10};
        VulkanBuffer test;
        auto& vk = m_window->GetVulkan();

        test = VulkanBuffer::CreateBuffer(vk.GetVmaAllocator(), sizeof(uint32_t) * data.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO);
        VulkanBuffer::CopyDataToGPU(vk.GetDevice(), vk.GetVmaAllocator(), vk.GetSingleTimeCommandPool(), vk.GetQueue(QueueType::GRAPHICS), data.data(), test, sizeof(uint32_t) * data.size());
        //-----------------------------------------------------------------------------------
        // END OF TEST AREA
        //-----------------------------------------------------------------------------------

        while(!m_window->ShouldClose() && m_isRunning) {
            m_diagnostics->BeginFrame();

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
                m_window->Render();
            }
            
            m_diagnostics->EndFrame();

#ifndef NDEBUG
            timerUpdate += dt;
            if(timerUpdate >= 1.0) {
                t.text = m_diagnostics->Summary();
                timerUpdate = 0.0;
            }
#endif
        }
    }
} // namespace cp_api