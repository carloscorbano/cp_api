#pragma once

#include "glfw.inc.hpp"
#include "vma.inc.hpp"

namespace cp_api {
    struct VulkanImage {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;      // para bind no pipeline
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{};     // opcional
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent{};                    // largura, altura, profundidade
        VkImageUsageFlags usage = 0;            // para referência (ex.: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    };

    VulkanImage CreateImage(VkDevice device, 
        VmaAllocator allocator, 
        uint32_t width, 
        uint32_t height, 
        VkFormat format, 
        VkImageUsageFlags usage, 
        VmaMemoryUsage memoryUsage, 
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);


    void DestroyImage(VkDevice device, VmaAllocator allocator, VulkanImage& image);
    bool FormatHasStencil(VkFormat format);
    void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void TransitionImageLayout(VkCommandBuffer cmdBuffer, VulkanImage& image, VkImageLayout newLayout);
    void CopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImage dstImage, uint32_t width, uint32_t height, uint32_t mipLevel = 0, uint32_t layerCount = 1);
} // namespace cp_api