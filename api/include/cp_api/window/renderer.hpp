#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include "glfw.inc.hpp"
#include "frame.hpp"

#include <entt/entt.hpp>

namespace cp_api {
    class Window;
    class World;
    class ThreadPool;
    class Vulkan;
    class RenderTargetManager;

    class Renderer {
    public:
        Renderer(Window& window, World& world, ThreadPool& threadPool);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void Render();
        
    private:
        void submitThreadWork();

        //events and callbacks
        void setupEventListeners();
        void onCameraCreationCallback(entt::registry& reg, entt::entity e);
        void onCameraDestructionCallback(entt::registry& reg, entt::entity e);

        //initialization methods
        void createGlobalDescriptorPool();
        void destroyGlobalDescriptorPool();

        void createFrames();
        void destroyFrames();

        void createMainCamera();
        void destroyMainCamera();

        void initImGui();
        void destroyImGui();

        void createRenderFinishedSemaphores();
        void destroyRenderFinishedSemaphores();

        void createCommandResources();
        void destroyCommandResources();

        bool isRenderEnabled() const { return m_renderEnabled.load(std::memory_order_acquire); }

    private:
        //refs cache
        Window& m_window;
        Vulkan& m_vulkan;
        World& m_world;
        ThreadPool& m_threadPool;    
        
        std::unique_ptr<RenderTargetManager> m_rtManager;

        //controller vars
        std::atomic<bool> m_renderEnabled { true };
        std::atomic<bool> m_swapchainIsDirty { false };
        std::atomic<bool> m_skipAfterSwapchainRecreation { false };
        std::atomic<bool> m_surfaceLost { false };
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        VkSemaphore m_timelineSem = VK_NULL_HANDLE;

        uint32_t m_writeFrameIndex = 0;
        uint32_t m_readFrameIndex = 0;

        std::vector<Frame> m_frames;

        //render thread
        std::thread m_renderThreadWorker;

        //Descriptor pool
        VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;

        uint32_t m_mainCameraUID = 0;
        uint64_t m_frameCounter = 0;
    };
} // namespace cp_api
