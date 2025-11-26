#pragma once

#include <utility>
#include "glfw.inc.hpp"
#include "vma.inc.hpp"

namespace cp_api {
    class VulkanBuffer {
    public:
        VulkanBuffer() = default;
        ~VulkanBuffer();

        VulkanBuffer(VulkanBuffer&& o) noexcept {
            *this = std::move(o);
        }

        VulkanBuffer& operator=(VulkanBuffer&& o) noexcept {
            if(this != &o) {
                destroy();

                buffer = o.buffer;
                allocator = o.allocator;
                allocation = o.allocation;
                allocationInfo = o.allocationInfo;
                usage = o.usage;

                o.buffer = VK_NULL_HANDLE;
                o.allocator = VK_NULL_HANDLE;
                o.allocation = VK_NULL_HANDLE;
                o.allocationInfo = {};
                o.usage = 0;
            }

            return *this;
        }

        static VulkanBuffer CreateBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        static void CopyDataToGPU(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, void* srcData, VulkanBuffer& dstBuffer, VkDeviceSize size);  

        VkBuffer GetBuffer() const noexcept { return buffer; }
        VmaAllocation GetAllocation() const noexcept { return allocation; }
        const VmaAllocationInfo& GetAllocationInfo() const noexcept { return allocationInfo; }
        VkBufferUsageFlags GetUsage() const noexcept { return usage; }
        
    private:
        void destroy();
    private:
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocator allocator = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{}; 
        VkBufferUsageFlags usage = 0;
    };
} // namespace cp_api
