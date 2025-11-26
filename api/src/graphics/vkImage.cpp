#include "cp_api/graphics/vkImage.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {
    VulkanImage::~VulkanImage() {
        destroy();
    }

    VulkanImage VulkanImage::CreateImage(VkDevice device, 
        VmaAllocator allocator, 
        uint32_t width, 
        uint32_t height, 
        VkFormat format, 
        VkImageUsageFlags usage, 
        VmaMemoryUsage memoryUsage, 
        VkImageAspectFlags aspectMask) {

        VulkanImage result;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &result.image, &result.allocation, nullptr) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create VMA image");
        }

        vmaGetAllocationInfo(allocator, result.allocation, &result.allocationInfo);

        result.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        result.format = imageInfo.format;
        result.usage = imageInfo.usage;
        result.extent = imageInfo.extent;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = result.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectMask;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &result.view) != VK_SUCCESS) {
            vmaDestroyImage(allocator, result.image, result.allocation);
            result.image = VK_NULL_HANDLE;
            result.allocation = VK_NULL_HANDLE;
            CP_LOG_THROW("Failed to create image view!");
        }


        result.device = device;
        result.alloc = allocator;

        return result;

    }

    bool VulkanImage::FormatHasStencil(VkFormat format) {
        switch (format)
        {
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return true;
            default:
                return false;
        }
    }

    void VulkanImage::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        // Define aspect mask
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (FormatHasStencil(format))
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        VkPipelineStageFlags srcStage;
        VkPipelineStageFlags dstStage;

        // --- CASOS ESPECÍFICOS ---
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        else
        {
            // Fallback seguro: assume color attachment
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            srcStage, dstStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    void VulkanImage::TransitionImageLayout(VkCommandBuffer cmdBuffer, VulkanImage& image, VkImageLayout newLayout) {
        if(image.layout == newLayout) return;

        TransitionImageLayout(cmdBuffer, image.image, image.format, image.layout, newLayout);
        image.layout = newLayout;
    }

    void VulkanImage::CopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImage dstImage, uint32_t width, uint32_t height, uint32_t mipLevel, uint32_t layerCount) {
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.mipLevel = mipLevel;
        copyRegion.srcSubresource.baseArrayLayer = 0;
        copyRegion.srcSubresource.layerCount = layerCount;

        copyRegion.srcOffset = {0, 0, 0};

        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.mipLevel = mipLevel;
        copyRegion.dstSubresource.baseArrayLayer = 0;
        copyRegion.dstSubresource.layerCount = layerCount;

        copyRegion.dstOffset = {0, 0, 0};

        copyRegion.extent.width = width;
        copyRegion.extent.height = height;
        copyRegion.extent.depth = 1;

        // Registrar o comando de cópia
        vkCmdCopyImage(
            commandBuffer,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );
    }

    void VulkanImage::destroy() {
        if (device == VK_NULL_HANDLE || alloc == VK_NULL_HANDLE)
            return;

        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }

        if (image != VK_NULL_HANDLE) {
            vmaDestroyImage(alloc, image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }

        device = VK_NULL_HANDLE;
        alloc = VK_NULL_HANDLE;
    }
}