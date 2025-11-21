#include "cp_api/window/vkBuffer.hpp"
#include "cp_api/core/debug.hpp"

cp_api::VulkanBuffer cp_api::VulkanBuffer::CreateBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    cp_api::VulkanBuffer result;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &result.buffer, &result.allocation, &result.allocationInfo) != VK_SUCCESS)
    {
        CP_LOG_THROW("Failed to create VMA buffer");
    }

    CP_LOG_INFO("Created buffer of size {} bytes", result.allocationInfo.size);
    result.usage = usage;

    return result;
}

void cp_api::VulkanBuffer::DestroyBuffer(VmaAllocator allocator, cp_api::VulkanBuffer& buffer) {
    if (buffer.buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
        buffer.buffer = VK_NULL_HANDLE;
        buffer.allocation = VK_NULL_HANDLE;

        CP_LOG_INFO("Destroyed VMA buffer size {} bytes", buffer.allocationInfo.size);
    }
}

void cp_api::VulkanBuffer::CopyDataToGPU(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, void* srcData, cp_api::VulkanBuffer& dstBuffer, VkDeviceSize size) {
    if (!srcData || size == 0 || dstBuffer.buffer == VK_NULL_HANDLE)
        throw std::runtime_error("Invalid parameters for CopyDataToGPU");

    // --- Criar buffer staging ---
    cp_api::VulkanBuffer stagingBuffer = CreateBuffer(
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

    // --- Destruir staging buffer ---
    DestroyBuffer(allocator, stagingBuffer);
}