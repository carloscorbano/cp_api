#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

namespace cp_api {
    class DescriptorPool {
    public:
        DescriptorPool() = default;
        ~DescriptorPool();

        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;

        DescriptorPool(DescriptorPool&& o) noexcept { 
            move(std::move(o)); 
        }

        DescriptorPool& operator=(DescriptorPool&& o) noexcept {
            if(this != &o) { 
                destroy(); 
                move(std::move(o)); 
            }

            return *this;
        }

        void create(VkDevice dev, uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& sizes, VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
        VkDescriptorPool get() const { return pool; }

    private:
        void destroy();
        void move(DescriptorPool&& o);

        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
    };
}