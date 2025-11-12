#pragma once

namespace cp_api {
    class ThreadPool;
    class Vulkan;
    class Renderer {
    public:
        Renderer(Vulkan& vulkan, ThreadPool& threadPool);
        ~Renderer();
    private:
        Vulkan& m_vulkan;
        ThreadPool& m_threadPool;
    };
} // namespace cp_api