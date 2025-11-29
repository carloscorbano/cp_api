#pragma once

#include <vector>
#include "glfw.inc.hpp"
#include "renderTarget.hpp"
#include "vkBuffer.hpp"
#include <array>
#include <memory>
#include <unordered_map>

namespace cp_api {
    struct WorkerCmdData {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cb = VK_NULL_HANDLE;
    };

    constexpr uint16_t MAX_WORKERS_PER_CAMERA = 4;
    
    struct CameraWork {
        uint32_t cameraEntityId = 0;
        uint32_t width = 0;
        uint32_t height = 0;      
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;  
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
        
        std::array<WorkerCmdData, MAX_WORKERS_PER_CAMERA> workers;
    };

    struct Frame
    {
        VkCommandPool primaryCmdPool = VK_NULL_HANDLE;
        VkCommandBuffer primary = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;

        uint64_t recordValue = 0;
        uint64_t renderValue = 0;

        std::unordered_map<uint32_t, CameraWork> cameraWorks;

        VkCommandPool imguiCmdPool;
        VkCommandBuffer imguiCmdBuffer;
    };
} // namespace cp_api 