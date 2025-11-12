#pragma once

#include <thread>
#include <atomic> 
#include <chrono>
#include "glfw.inc.hpp"
#include "frame.hpp"

namespace cp_api {
    class World;
    class ThreadPool;
    class Window;
    class Renderer {
        const uint32_t SIMULTANEOS_WORKERS_RECORDING_COUNT = 2;
    public:
        Renderer(Window& window);
        ~Renderer();

        void ProcessWorld(World& world, ThreadPool& threadPool);
    private:
        void setupEventListeners();

        void createFrames();
        void destroyFrames();

        void createRenderTargets();
        void destroyRenderTargets();

        void createCommandResources();
        void destroyCommandResources();

        void createTransferResources();
        void destroyTransferResources();

        void initImGui();
        void cleanupImGui();

        void renderThreadWork();
    private:
        Window& m_window;

        std::thread m_renderThread;
        std::atomic<bool> m_renderEnabled { true };
        std::atomic<bool> m_swapchainIsDirty { false };
        std::atomic<bool> m_skipAfterSwapchainRecreation { false };
        std::atomic<bool> m_iconified { false };
        std::atomic<bool> m_surfaceLost { false };

        std::vector<Frame> m_frames;
        VkSemaphore m_timelineSem = VK_NULL_HANDLE;

        uint32_t m_writeFrameIndex = 0;
        uint32_t m_readFrameIndex = 0;

        VkCommandPool m_transferCmdPool = VK_NULL_HANDLE;
        VkCommandBuffer m_transferCmdBuffer = VK_NULL_HANDLE;


        VkDescriptorPool m_imguiPool;
    };
} // namespace cp_api