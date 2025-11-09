#pragma once

#include "glfw.inc.hpp"
#include "vma.inc.hpp"
#include <string>

namespace cp_api {
    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;

        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{};

        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent{};
        VkImageUsageFlags usage = 0;

        std::string name; // opcional, útil para debug e logs
    };
} // namespace cp_api
