#include "cp_api/graphics/renderTargetManager.hpp"
#include "cp_api/graphics/vulkan.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {

    void RenderTargetManager::Init(Vulkan* vk) {
        m_vk = vk;
    }

    void RenderTargetManager::Destroy() {
        m_targets.clear(); // unique_ptr dos RT destrói as imagens
    }

    void RenderTargetManager::BeginFrame(uint64_t frameIndex) {
        m_currentFrame = frameIndex;
    }

    RenderTarget* RenderTargetManager::Acquire(
        uint32_t id,
        uint32_t width,
        uint32_t height,
        VkFormat colorFormat,
        VkFormat depthFormat
    ) {
        auto& entry = m_targets[id];

        // Se nunca existiu, criar RT novo
        if (!entry.rt) {
            entry.rt = std::make_unique<RenderTarget>();
            entry.rt->Create(
                m_vk->GetDevice(),
                m_vk->GetVmaAllocator(),
                width,
                height,
                colorFormat,
                depthFormat
            );
            entry.width = width;
            entry.height = height;
            entry.colorFmt = colorFormat;
            entry.depthFmt = depthFormat;

            auto cmd = m_vk->BeginSingleTimeCommands();

            {
                // Transições: color (se existir) -> COLOR_ATTACHMENT_OPTIMAL
                if (entry.rt->GetColorImage().GetImage() != VK_NULL_HANDLE) {
                    VulkanImage::TransitionImageLayout(cmd, entry.rt->GetColorImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                }

                // depth -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL (se existir)
                if (entry.rt->GetDepthImage().GetImage() != VK_NULL_HANDLE) {
                    VulkanImage::TransitionImageLayout(cmd, entry.rt->GetDepthImage(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                }
            }

            m_vk->EndSingleTimeCommands(cmd);
        }
        else {
            // Se existia mas o tamanho mudou, recriar
            if (entry.width != width || entry.height != height ||
                entry.colorFmt != colorFormat || entry.depthFmt != depthFormat)
            {
                entry.rt->Create(
                    m_vk->GetDevice(),
                    m_vk->GetVmaAllocator(),
                    width,
                    height,
                    colorFormat,
                    depthFormat
                );
                entry.width = width;
                entry.height = height;
                entry.colorFmt = colorFormat;
                entry.depthFmt = depthFormat;
            }
        }

        entry.lastUsedFrame = m_currentFrame;
        return entry.rt.get();
    }

    void RenderTargetManager::Release(uint32_t id) {
        auto it = m_targets.find(id);
        if (it != m_targets.end()) {
            m_targets.erase(it);
        }
    }

    void RenderTargetManager::PurgeUnused(uint64_t thresholdFrames) {
        for (auto it = m_targets.begin(); it != m_targets.end(); ) {
            auto& entry = it->second;
            if ((m_currentFrame - entry.lastUsedFrame) >= thresholdFrames) {
                it = m_targets.erase(it);
            } else {
                ++it;
            }
        }
    }

    void RenderTargetManager::InvalidateByResolution(uint32_t newW, uint32_t newH) {
        for (auto& [id, entry] : m_targets) {
            if (entry.width == newW && entry.height == newH)
                continue;

            entry.rt->Recreate(
                m_vk->GetDevice(),
                m_vk->GetVmaAllocator(),
                newW,
                newH
            );

            entry.width = newW;
            entry.height = newH;
        }
    }

} // namespace cp_api
