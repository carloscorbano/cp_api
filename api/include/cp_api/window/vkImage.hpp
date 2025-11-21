#pragma once

#include <utility>
#include "glfw.inc.hpp"
#include "vma.inc.hpp"

namespace cp_api {
    class VulkanImage {
    public:
        VulkanImage() = default;
        ~VulkanImage();

        VulkanImage(const VulkanImage&) = delete;
        VulkanImage& operator=(const VulkanImage&) = delete;

        VulkanImage(VulkanImage&& o) noexcept {
            *this = std::move(o);
        }

        VulkanImage& operator=(VulkanImage&& o) noexcept {
            if (this != &o) {
                // limpar estado anterior
                destroy();

                // copiar triviais
                image = o.image;
                view = o.view;
                allocation = o.allocation;
                allocationInfo = o.allocationInfo;
                layout = o.layout;
                format = o.format;
                extent = o.extent;
                usage = o.usage;
                device = o.device;
                alloc = o.alloc;

                // invalida o outro
                o.image = VK_NULL_HANDLE;
                o.view = VK_NULL_HANDLE;
                o.allocation = VK_NULL_HANDLE;
                o.allocationInfo = {};
                o.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                o.format = VK_FORMAT_UNDEFINED;
                o.extent = {};
                o.usage = 0;
                o.device = VK_NULL_HANDLE;
                o.alloc = VK_NULL_HANDLE;
            }
            return *this;
        }

        static VulkanImage CreateImage(VkDevice device, 
            VmaAllocator allocator, 
            uint32_t width, 
            uint32_t height, 
            VkFormat format, 
            VkImageUsageFlags usage, 
            VmaMemoryUsage memoryUsage, 
            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

        static bool FormatHasStencil(VkFormat format);
        static void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        static void TransitionImageLayout(VkCommandBuffer cmdBuffer, VulkanImage& image, VkImageLayout newLayout);
        static void CopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImage dstImage, uint32_t width, uint32_t height, uint32_t mipLevel = 0, uint32_t layerCount = 1);

        inline VkImage GetImage() const noexcept { return image; }
        inline VkImageView GetView() const noexcept { return view; }
        inline VmaAllocation GetAllocation() const noexcept { return allocation; }
        inline const VmaAllocationInfo& GetAllocationInfo() const noexcept { return allocationInfo; }
        inline VkImageLayout GetLayout() const noexcept { return layout; }
        inline VkFormat GetFormat() const noexcept { return format; }
        inline const VkExtent3D& GetExtent() const noexcept { return extent; }
        inline VkImageUsageFlags GetUsage() const noexcept { return usage; }

    private:
        void destroy();
    private:
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;      // para bind no pipeline
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{};     // opcional
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent{};                    // largura, altura, profundidade
        VkImageUsageFlags usage = 0;            // para referência (ex.: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)    
        
        VkDevice device = VK_NULL_HANDLE;
        VmaAllocator alloc = VK_NULL_HANDLE;
    };
} // namespace cp_api