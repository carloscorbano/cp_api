#pragma once

#include <vector>
#include "glfw.inc.hpp"
#include "vkImage.hpp"

namespace cp_api {
    struct RenderTarget {
        VulkanImage color;
        VulkanImage depth;
        VkFormat colorFormat;
        VkFormat depthFormat;

        uint32_t width;
        uint32_t height;
    };

    struct Frame
    {
        std::vector<VkCommandPool> cmdPool;
        std::vector<VkCommandBuffer> secondaries;
        VkCommandBuffer primary = VK_NULL_HANDLE;

        VkSemaphore imageAvailable = VK_NULL_HANDLE;

        uint64_t recordValue = 0;
        uint64_t renderValue = 0;

        RenderTarget renderTarget;
    };
} // namespace cp_api 