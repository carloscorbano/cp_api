#include "cp_api/graphics/descriptorPool.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {
    DescriptorPool::~DescriptorPool() { 
        destroy(); 
    }

    void DescriptorPool::create(VkDevice dev, uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& sizes, VkDescriptorPoolCreateFlags flags) {
        destroy(); 
        
        device = dev;

        VkDescriptorPoolCreateInfo info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        info.poolSizeCount = (uint32_t)sizes.size();
        info.pPoolSizes    = sizes.data();
        info.maxSets       = maxSets;
        info.flags         = flags;

        if(vkCreateDescriptorPool(dev, &info, nullptr, &pool) != VK_SUCCESS)
            CP_LOG_THROW("Failed to create descriptor pool!");
    }

    void DescriptorPool::destroy() {
        if(pool) vkDestroyDescriptorPool(device, pool, nullptr);
        pool = VK_NULL_HANDLE;
    }

    void DescriptorPool::move(DescriptorPool&& o) {
        pool = o.pool; 
        device = o.device;
        o.pool = VK_NULL_HANDLE; 
        o.device = VK_NULL_HANDLE;
    }
} // namespace cp_api
