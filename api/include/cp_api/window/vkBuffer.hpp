#pragma once

#include "glfw.inc.hpp"
#include "vma.inc.hpp"

namespace cp_api {
    class VulkanBuffer {
    public:
        static VulkanBuffer CreateBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        static void DestroyBuffer(VmaAllocator allocator, VulkanBuffer& buffer);
        static void CopyDataToGPU(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, void* srcData, VulkanBuffer& dstBuffer, VkDeviceSize size);  

        VkBuffer GetBuffer() const noexcept { return buffer; }
        VmaAllocation GetAllocation() const noexcept { return allocation; }
        const VmaAllocationInfo& GetAllocationInfo() const noexcept { return allocationInfo; }
        VkBufferUsageFlags GetUsage() const noexcept { return usage; }
        
    private:
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{}; 
        VkBufferUsageFlags usage = 0;
    };
} // namespace cp_api
