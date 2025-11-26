#include "cp_api/graphics/vkBuffer.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {
    VulkanBuffer::~VulkanBuffer() {
        destroy();
    }

    VulkanBuffer VulkanBuffer::CreateBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
        VulkanBuffer result;
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        result.allocator = allocator;
    
        if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &result.buffer, &result.allocation, &result.allocationInfo) != VK_SUCCESS)
        {
            CP_LOG_THROW("Failed to create VMA buffer");
        }
    
        CP_LOG_INFO("Created buffer of size {} bytes", result.allocationInfo.size);
        result.usage = usage;
    
        return result;
    }
        
    void VulkanBuffer::CopyDataToGPU(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, void* srcData, cp_api::VulkanBuffer& dstBuffer, VkDeviceSize size) {
        if (!srcData || size == 0 || dstBuffer.buffer == VK_NULL_HANDLE)
            throw std::runtime_error("Invalid parameters for CopyDataToGPU");
    
        // --- Criar buffer staging ---
        VulkanBuffer stagingBuffer = CreateBuffer(
            allocator,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
    
        // --- Mapear e copiar dados ---
        void* data;
        if (vmaMapMemory(allocator, stagingBuffer.allocation, &data) != VK_SUCCESS)
            CP_LOG_THROW("Failed to map staging buffer memory");
        std::memcpy(data, srcData, static_cast<size_t>(size));
        vmaUnmapMemory(allocator, stagingBuffer.allocation);
    
        // --- Criar command buffer temporário ---
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
    
        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
            CP_LOG_THROW("Failed to allocate command buffer");
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
        // --- Copy buffer ---
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);
    
        vkEndCommandBuffer(commandBuffer);
    
        // --- Submit e esperar ---
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
    
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
    
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void VulkanBuffer::destroy() {
        if (buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator, buffer, allocation);
            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
    
            CP_LOG_INFO("Destroyed VMA buffer size {} bytes", allocationInfo.size);
        }
    }
}
