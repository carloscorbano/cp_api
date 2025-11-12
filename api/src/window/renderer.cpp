#include "cp_api/window/renderer.hpp"
#include "cp_api/core/debug.hpp"
#include "cp_api/window/vulkan.hpp"
#include "cp_api/core/threadPool.hpp"

namespace cp_api {

    Renderer::Renderer(Vulkan& vulkan, ThreadPool& threadPool)
        : m_vulkan(vulkan), m_threadPool(threadPool) {
        CP_LOG_INFO("[RENDERER] Renderer created");
    }

    Renderer::~Renderer() {
        CP_LOG_INFO("[RENDERER] Renderer destroyed");
    }
} // namespace cp_api
