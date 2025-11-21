#pragma once

#include <vector>
#include "glfw.inc.hpp"
#include "renderTarget.hpp"
#include <array>

namespace cp_api {
    struct WorkerCmdData {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cb = VK_NULL_HANDLE;
    };

    constexpr uint16_t MAX_WORKERS_PER_FRAME = 4;

    struct Frame
    {
        VkCommandPool primaryCmdPool = VK_NULL_HANDLE;
        VkCommandBuffer primary = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;

        uint64_t recordValue = 0;
        uint64_t renderValue = 0;

        std::array<WorkerCmdData, MAX_WORKERS_PER_FRAME> workers;

        VkCommandPool imguiCmdPool;
        VkCommandBuffer imguiCmdBuffer;
    };
} // namespace cp_api 