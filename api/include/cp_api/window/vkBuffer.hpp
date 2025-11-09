#pragma once

#include "glfw.inc.hpp"
#include "vma.inc.hpp"

namespace cp_api {
    struct VulkanBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{}; 
        VkBufferUsageFlags usage = 0;
    };

    inline VulkanBuffer CreateBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    inline void DestroyBuffer(VmaAllocator allocator, VulkanBuffer& buffer);
    inline void CopyDataToGPU(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, void* srcData, VulkanBuffer& dstBuffer, VkDeviceSize size);
} // namespace cp_api
