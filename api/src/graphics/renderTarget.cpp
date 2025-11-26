#include "cp_api/graphics/renderTarget.hpp"

namespace cp_api {
    void RenderTarget::Create(VkDevice device, VmaAllocator allocator, uint32_t w, uint32_t h, VkFormat colorFmt, VkFormat depthFmt) {
        destroy(); // caso já exista

        width = w;
        height = h;
        colorFormat = colorFmt;
        depthFormat = depthFmt;

        color = VulkanImage::CreateImage(
            device, allocator,
            width, height,
            colorFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        depth = VulkanImage::CreateImage(
            device, allocator,
            width, height,
            depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
        );
    }   

    void RenderTarget::Recreate(VkDevice device, VmaAllocator allocator, uint32_t newW, uint32_t newH) {
        if (newW == width && newH == height) return; // nada a fazer

        Create(device, allocator, newW, newH, colorFormat, depthFormat);
    }

    void RenderTarget::destroy() {
        // VulkanImage já é RAII, só resetamos para acionar o destrutor
        color = VulkanImage{};
        depth = VulkanImage{};
        width = height = 0;
        colorFormat = VK_FORMAT_UNDEFINED;
        depthFormat = VK_FORMAT_UNDEFINED;
    }
}