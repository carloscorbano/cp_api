#pragma once
#include <unordered_map>
#include <cstdint>
#include <memory>

#include "renderTarget.hpp"

namespace cp_api {

    class Vulkan;
    class RenderTargetManager {
    public:
        RenderTargetManager() = default;
        ~RenderTargetManager() { Destroy(); }

        void Init(Vulkan* vk);
        void Destroy();

        // Acquire a render target for a given camera/light/etc.
        // If the RT does not exist, it's created.
        // If it exists but resolution differs, it's recreated.
        RenderTarget* Acquire(
            uint32_t id,
            uint32_t width,
            uint32_t height,
            VkFormat colorFormat,
            VkFormat depthFormat
        );

        void Release(uint32_t id);

        // Mark all RTs as potentially unused this frame
        void BeginFrame(uint64_t frameIndex);

        // Remove RTs not used for X frames
        void PurgeUnused(uint64_t thresholdFrames = 60);

        // Force recreation of all RTs that depend on window size (main cameras)
        void InvalidateByResolution(uint32_t newW, uint32_t newH);

    private:
        struct RTEntry {
            std::unique_ptr<RenderTarget> rt;
            uint64_t lastUsedFrame = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            VkFormat colorFmt = VK_FORMAT_UNDEFINED;
            VkFormat depthFmt = VK_FORMAT_UNDEFINED;
        };

        std::unordered_map<uint32_t, RTEntry> m_targets;

        Vulkan* m_vk = nullptr;
        uint64_t m_currentFrame = 0;
    };

} // namespace cp_api
