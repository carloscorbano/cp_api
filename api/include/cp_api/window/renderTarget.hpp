#pragma once

#include "vkImage.hpp"

namespace cp_api {
    class RenderTarget {
    public:
        RenderTarget() = default;
        ~RenderTarget() { destroy(); }

        RenderTarget(const RenderTarget&) = delete;
        RenderTarget& operator=(const RenderTarget&) = delete;

        RenderTarget(RenderTarget&& other) noexcept {
            *this = std::move(other);
        }

        RenderTarget& operator=(RenderTarget&& other) noexcept {
            if (this != &other) {
                destroy();

                color = std::move(other.color);
                depth = std::move(other.depth);
                colorFormat = other.colorFormat;
                depthFormat = other.depthFormat;
                width = other.width;
                height = other.height;

                other.colorFormat = VK_FORMAT_UNDEFINED;
                other.depthFormat = VK_FORMAT_UNDEFINED;
                other.width = other.height = 0;
            }
            return *this;
        }

        // -------------------------
        // CREATE / RECREATE
        // -------------------------
        void Create(VkDevice device, VmaAllocator allocator, uint32_t w, uint32_t h, VkFormat colorFmt, VkFormat depthFmt);
        void Recreate(VkDevice device, VmaAllocator allocator, uint32_t newW,  uint32_t newH);

        // -------------------------
        // HELPERS
        // -------------------------
        bool IsValid() const noexcept { return width > 0 && height > 0 && color.GetImage() != VK_NULL_HANDLE; }

        uint32_t GetWidth() const noexcept { return width; }
        uint32_t GetHeight() const noexcept { return height; }

        VulkanImage& GetColorImage() noexcept { return color; }
        VulkanImage& GetDepthImage() noexcept { return depth; }

    private:
        void destroy();

    private:
        VulkanImage color;
        VulkanImage depth;

        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;

        uint32_t width = 0;
        uint32_t height = 0;
    };

} // namespace cp_api
