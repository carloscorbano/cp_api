#include "cp_api/graphics/vulkan.hpp"
#include "cp_api/core/debug.hpp"
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <cstdint> 
#include <limits> 
#include <algorithm>

namespace cp_api {
    Vulkan::Vulkan(GLFWwindow* window) : m_wndHandler(window) {
        if (!window) CP_LOG_THROW("Vulkan received null GLFW window pointer!");
        createInstance();
        createSurface();
        createDebugMessenger();
        pickPhysicalDevice();
        createLogicalDevice();
        createVmaAllocator();
        m_swapchain = createSwapchain(VK_PRESENT_MODE_FIFO_KHR);
        createSingleTimeCommandsPool();
    }

    Vulkan::~Vulkan() {
        destroySingleTimeCommandsPool();
        destroySwapchain(&m_swapchain);
        destroyVmaAllocator();
        destroyLogicalDevice();
        destroyDebugMessenger();
        destroySurface();
        destroyInstance();
    }

    VkQueue Vulkan::GetQueue(QueueType type) const {
        switch (type) {
            case QueueType::GRAPHICS:   return m_deviceQueues.graphics;
            case QueueType::COMPUTE:    return m_deviceQueues.compute;
            case QueueType::TRANSFER:   return m_deviceQueues.transfer;
            case QueueType::PRESENT:    return m_deviceQueues.present;
        }

        CP_LOG_THROW("Requested queue index out of range or queue type not available");
        return VK_NULL_HANDLE;
    }

    void Vulkan::RecreateSwapchain(VkPresentModeKHR preferredMode, bool useOldSwapchain) {
        vkDeviceWaitIdle(m_device);

        Swapchain oldSwapchain = m_swapchain;
        m_swapchain = createSwapchain(preferredMode, useOldSwapchain ? &oldSwapchain : nullptr);
        destroySwapchain(&oldSwapchain);
    }

    VkCommandBuffer Vulkan::BeginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_singleTimeCmdPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(GetDevice(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to begin single time command!");
        } 

        return commandBuffer;
    }

    void Vulkan::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to end single time command!");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence = VK_NULL_HANDLE;
        if(vkCreateFence(GetDevice(), &fci, nullptr, &fence) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create fence for single time commands!");
        }

        if(vkQueueSubmit(GetQueue(QueueType::GRAPHICS), 1, &submitInfo, fence) != VK_SUCCESS) {
            CP_LOG_ERROR("Failed to submit RT initial transition");
        } else { 
            vkWaitForFences(GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
        }

        vkDestroyFence(GetDevice(), fence, nullptr);
        vkFreeCommandBuffers(GetDevice(), m_singleTimeCmdPool, 1, &commandBuffer);
    }

    void Vulkan::RecreateSurface() {
        vkDeviceWaitIdle(m_device);
        destroySurface();
        createSurface();
    }

    VkResult Vulkan::BeginCommandBuffer(  VkCommandBuffer cmdBuffer, 
                                            const std::vector<VkFormat>& colorAttachments, 
                                            const VkFormat& depthFormat, 
                                            const VkFormat& stencilFormat,
                                            const VkSampleCountFlagBits& rasterizationSamples) {

        VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{};
        inheritanceRenderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
        inheritanceRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        inheritanceRenderingInfo.pColorAttachmentFormats = colorAttachments.data();
        inheritanceRenderingInfo.depthAttachmentFormat = depthFormat;
        inheritanceRenderingInfo.stencilAttachmentFormat = stencilFormat;
        inheritanceRenderingInfo.rasterizationSamples = rasterizationSamples;

        VkCommandBufferInheritanceInfo inh{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
        inh.pNext = &inheritanceRenderingInfo;
        inh.renderPass = VK_NULL_HANDLE;
        inh.subpass = 0;
        inh.framebuffer = VK_NULL_HANDLE;

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        bi.pInheritanceInfo = &inh;

        return vkBeginCommandBuffer(cmdBuffer, &bi);
    }

    VkResult Vulkan::AcquireSwapchainNextImage(VkSemaphore availableSemaphore, uint32_t* outIndex,  uint64_t timeout) {
        return vkAcquireNextImageKHR(GetDevice(), GetSwapchain().handler, timeout, availableSemaphore, VK_NULL_HANDLE, outIndex);
    }

    void Vulkan::SignalTimelineSemaphore(VkSemaphore semaphore, const uint64_t& value) {
        VkSemaphoreSignalInfo signalInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
        signalInfo.pNext = nullptr;
        signalInfo.semaphore = semaphore;
        signalInfo.value = value;
        if(vkSignalSemaphore(GetDevice(), &signalInfo) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to signal timeline semaphore!");
        }
    }

    void Vulkan::WaitTimelineSemaphores(const std::vector<VkSemaphore>& semaphores, const std::vector<uint64_t>& values, const uint64_t& timeout) {
        VkSemaphoreWaitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
        waitInfo.semaphoreCount = (uint32_t)semaphores.size();
        waitInfo.pSemaphores = semaphores.data();
        waitInfo.pValues = values.data();

        if(vkWaitSemaphores(GetDevice(), &waitInfo, timeout) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to wait timeline semaphore!");
        }
    }

#pragma region VULKAN_INITIALIZATION

    void Vulkan::createInstance() {
        if(enableValidationLayers && !checkValidationLayerSupport()) {
            CP_LOG_THROW("Validation layers required but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "cp_app";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "cp_api";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo ci { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        ci.pApplicationInfo = &appInfo;
        
        auto reqExtensions = getGlfwRequiredExtensions();

        ci.enabledExtensionCount = static_cast<uint32_t>(reqExtensions.size());
        ci.ppEnabledExtensionNames = reqExtensions.data();
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        for (auto ext : reqExtensions)
            CP_LOG_INFO("Instance extension enabled: {}", ext);

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        if(enableValidationLayers) {
            ci.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
            ci.ppEnabledLayerNames = m_validationLayers.data();

            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = debugCallback;

            ci.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;

        } else {
            ci.enabledLayerCount = 0;
            ci.pNext = nullptr;
        }
        
        if(vkCreateInstance(&ci, nullptr, &m_instance) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create vulkan instance");
        }
    }

    void Vulkan::destroyInstance() {
        if(m_instance) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }
    }

    void Vulkan::createSurface() {
        if(glfwCreateWindowSurface(m_instance, m_wndHandler, nullptr, &m_surface) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create window surface!");
        }
    }

    void Vulkan::destroySurface() {
        if(m_surface) {  vkDestroySurfaceKHR(m_instance, m_surface, nullptr); m_surface = VK_NULL_HANDLE; }
    }

    void Vulkan::createDebugMessenger() {
        if(!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr; // Optional

        auto createFunc = [](  VkInstance instance, 
                                const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
                                const VkAllocationCallbacks* pAllocator, 
                                VkDebugUtilsMessengerEXT* pDebugMessenger) -> VkResult {

            auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
            if (func != nullptr) {
                return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
            } else {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        };

        if(createFunc(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create debug messenger");
        }
    }

    void Vulkan::destroyDebugMessenger() {
        if(m_debugMessenger == VK_NULL_HANDLE) return;

        auto destroyMessenger = [](VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) -> void {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(instance, debugMessenger, pAllocator);
            }
        };

        destroyMessenger(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    void Vulkan::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

        if(deviceCount == 0) CP_LOG_THROW("Failed to enumerate physical devices!");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for(const auto& device : devices) {
            if(isDeviceSuitable(device)) {
                m_physDevice = device;
                break;
            }
        }

        if(m_physDevice == VK_NULL_HANDLE) {
            CP_LOG_THROW("Failed to find a suitable GPU!");
        }

        logSelectedGPU(m_physDevice);
    }

    void Vulkan::createLogicalDevice() {
        m_familyIndices = findQueueFamilies(m_physDevice, m_surface);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(m_physDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
        for(auto& qf : queueFamilies) {
            qf.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
            qf.pNext = nullptr;
        }

        vkGetPhysicalDeviceQueueFamilyProperties2(m_physDevice, &queueFamilyCount, queueFamilies.data());

        CP_LOG_INFO("============================================================");
        CP_LOG_INFO("[ QUEUE FAMILIES INFO ]");

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            const auto& props = queueFamilies[i].queueFamilyProperties;
            std::string bits;
            if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) bits += "GRAPHICS ";
            if (props.queueFlags & VK_QUEUE_COMPUTE_BIT) bits += "COMPUTE ";
            if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) bits += "TRANSFER ";
            if (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) bits += "SPARSE_BINDING ";
            CP_LOG_INFO("[QueueFamily {}] {} queues | {}", i, props.queueCount, bits);
        }

        CP_LOG_INFO("============================================================");
        CP_LOG_INFO("[ QUEUE FAMILIES IDS ]");
        CP_LOG_INFO("Graphics Queue Family: {}", m_familyIndices.graphicsFamily.value());
        CP_LOG_INFO("Compute Queue Family:  {}", m_familyIndices.computeFamily.value());
        CP_LOG_INFO("Transfer Queue Family: {}", m_familyIndices.transferFamily.value());
        CP_LOG_INFO("Present Queue Family:  {}", m_familyIndices.presentFamily.value());

        CP_LOG_INFO("============================================================");

        // --- 1) Filtrar famílias únicas ---
        std::set<uint32_t> uniqueFamilies = {
            m_familyIndices.graphicsFamily.value(),
            m_familyIndices.presentFamily.value(),
            m_familyIndices.computeFamily.value(),
            m_familyIndices.transferFamily.value()
        };

        float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo info{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            info.queueFamilyIndex = family;
            info.queueCount = 1;
            info.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(info);
        }

        // --- 2) Query features disponíveis ---
        VkPhysicalDeviceFeatures2 supportedFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        VkPhysicalDeviceVulkan11Features supported11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
        VkPhysicalDeviceVulkan12Features supported12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        VkPhysicalDeviceVulkan13Features supported13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

        supportedFeatures.pNext = &supported11;
        supported11.pNext = &supported12;
        supported12.pNext = &supported13;

        vkGetPhysicalDeviceFeatures2(m_physDevice, &supportedFeatures);

        // --- 3) Escolher as que queremos habilitar ---
        VkPhysicalDeviceFeatures2 enabledFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        VkPhysicalDeviceVulkan11Features enabled11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
        VkPhysicalDeviceVulkan12Features enabled12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        VkPhysicalDeviceVulkan13Features enabled13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

        enabledFeatures.pNext = &enabled11;
        enabled11.pNext = &enabled12;
        enabled12.pNext = &enabled13;

        enabled12.timelineSemaphore = VK_TRUE;

        // --- 4) Ativar só o que a GPU suporta ---

        // Vulkan 1.0 features
        if (supportedFeatures.features.samplerAnisotropy)
            enabledFeatures.features.samplerAnisotropy = VK_TRUE;

        if (supportedFeatures.features.sampleRateShading)
            enabledFeatures.features.sampleRateShading = VK_TRUE;

        if (supportedFeatures.features.fillModeNonSolid)
            enabledFeatures.features.fillModeNonSolid = VK_TRUE;

        if (supportedFeatures.features.wideLines)
            enabledFeatures.features.wideLines = VK_TRUE;

        // Vulkan 1.1 / 1.2 / 1.3 extras (só exemplo)
        if (supported12.scalarBlockLayout)
            enabled12.scalarBlockLayout = VK_TRUE;

        if (supported12.descriptorIndexing)
            enabled12.descriptorIndexing = VK_TRUE;

        if (supported13.dynamicRendering)
            enabled13.dynamicRendering = VK_TRUE;

        if (supported13.synchronization2)
            enabled13.synchronization2 = VK_TRUE;

        logDeviceFeatures(
            supportedFeatures, supported11, supported12, supported13,
            enabledFeatures, enabled11, enabled12, enabled13
        );

        // --- 5) Criar device info ---
        VkDeviceCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pNext = &enabledFeatures;

        // Extensões do device
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

        // Validation layers
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
            createInfo.ppEnabledLayerNames = m_validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // --- 6) Criar device ---
        if (vkCreateDevice(m_physDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
            CP_LOG_THROW("Failed to create logical device!");

        // --- 7) Obter filas ---
        if(auto func = reinterpret_cast<PFN_vkGetDeviceQueue2>(vkGetDeviceProcAddr(m_device, "vkGetDeviceQueue2"))) {
            VkDeviceQueueInfo2 queueInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
            queueInfo.queueIndex = 0;

            queueInfo.queueFamilyIndex = m_familyIndices.graphicsFamily.value();
            vkGetDeviceQueue2(m_device, &queueInfo, &m_deviceQueues.graphics);

            queueInfo.queueFamilyIndex = m_familyIndices.presentFamily.value();
            vkGetDeviceQueue2(m_device, &queueInfo, &m_deviceQueues.present);

            queueInfo.queueFamilyIndex = m_familyIndices.computeFamily.value();
            vkGetDeviceQueue2(m_device, &queueInfo, &m_deviceQueues.compute);

            queueInfo.queueFamilyIndex = m_familyIndices.transferFamily.value();
            vkGetDeviceQueue2(m_device, &queueInfo, &m_deviceQueues.transfer);
        } else {
            vkGetDeviceQueue(m_device, m_familyIndices.graphicsFamily.value(), 0, &m_deviceQueues.graphics);
            vkGetDeviceQueue(m_device, m_familyIndices.presentFamily.value(), 0, &m_deviceQueues.present);
            vkGetDeviceQueue(m_device, m_familyIndices.computeFamily.value(), 0, &m_deviceQueues.compute);
            vkGetDeviceQueue(m_device, m_familyIndices.transferFamily.value(), 0, &m_deviceQueues.transfer);
        }
    }

    void Vulkan::destroyLogicalDevice() {
        if(m_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_device, nullptr); 
            m_device = VK_NULL_HANDLE;
        }
    }

    void Vulkan::createVmaAllocator() {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = m_physDevice;
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.pAllocationCallbacks = nullptr;
        allocatorInfo.pDeviceMemoryCallbacks = nullptr;
        allocatorInfo.flags = 0;

        // Try to enable memory budget extension support if available in this build of VMA
    #ifdef VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    #endif

        if (vmaCreateAllocator(&allocatorInfo, &m_vmaAllocator) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create VMA allocator!");
        }

        CP_LOG_INFO("VMA allocator created successfully");
    }

    void Vulkan::destroyVmaAllocator() {
        if (m_vmaAllocator != VK_NULL_HANDLE && m_vmaAllocator != nullptr) {
            vmaDestroyAllocator(m_vmaAllocator);
            m_vmaAllocator = nullptr;
            CP_LOG_INFO("VMA allocator destroyed");
        }
    }

    Vulkan::Swapchain Vulkan::createSwapchain(VkPresentModeKHR preferredMode, Vulkan::Swapchain* oldSwapchain) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physDevice, m_surface);

        VkSurfaceFormat2KHR format = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, preferredMode);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.surfaceCapabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.surfaceCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.surfaceCapabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.surfaceCapabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = format.surfaceFormat.format;
        createInfo.imageColorSpace = format.surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t queueFamilyIndices[] = {m_familyIndices.graphicsFamily.value(), m_familyIndices.presentFamily.value()};

        if (m_familyIndices.graphicsFamily != m_familyIndices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        createInfo.preTransform = swapChainSupport.capabilities.surfaceCapabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = oldSwapchain ? oldSwapchain->handler : VK_NULL_HANDLE;

        Swapchain sc = {};
        if(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &sc.handler) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create swap chain!");
        }

        // Helper functions to convert Vulkan enums to strings
        auto vkFormatToString = [](VkFormat format) -> std::string {
            switch (format) {
            case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
            case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
            case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
            case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
            default: return "UNKNOWN_FORMAT";
            }
        };

        auto vkColorSpaceToString = [](VkColorSpaceKHR colorSpace) -> std::string {
            switch (colorSpace) {
            case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
            case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
            case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
            default: return "UNKNOWN_COLOR_SPACE";
            }
        };

        auto vkPresentModeToString = [](VkPresentModeKHR presentMode) -> std::string {
            switch (presentMode) {
            case VK_PRESENT_MODE_IMMEDIATE_KHR: return "VK_PRESENT_MODE_IMMEDIATE_KHR";
            case VK_PRESENT_MODE_MAILBOX_KHR: return "VK_PRESENT_MODE_MAILBOX_KHR";
            case VK_PRESENT_MODE_FIFO_KHR: return "VK_PRESENT_MODE_FIFO_KHR";
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
            default: return "UNKNOWN_PRESENT_MODE";
            }
        };

        // Log swap chain details
        CP_LOG_INFO("============================================================");
        CP_LOG_INFO("[ SWAPCHAIN CONFIG ]");
        CP_LOG_INFO("  Format:           {}", vkFormatToString(format.surfaceFormat.format));
        CP_LOG_INFO("  Color Space:      {}", vkColorSpaceToString(format.surfaceFormat.colorSpace));
        CP_LOG_INFO("  Present Mode:     {}", vkPresentModeToString(presentMode));
        CP_LOG_INFO("  Image Count:      {}", imageCount);
        CP_LOG_INFO("  Extent:           {}x{}", extent.width, extent.height);
        CP_LOG_INFO("============================================================");

        vkGetSwapchainImagesKHR(m_device, sc.handler, &imageCount, nullptr);
        sc.images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, sc.handler, &imageCount, sc.images.data());

        sc.colorFormat = format.surfaceFormat.format;
        sc.depthFormat = findDepthFormat();
        sc.stencilFormat = hasStencilFormat(sc.depthFormat) ? sc.depthFormat : VK_FORMAT_UNDEFINED;
        sc.extent = extent;

        sc.views.resize(sc.images.size());
        for (size_t i = 0; i < sc.images.size(); i++) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = sc.images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = sc.colorFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_device, &viewInfo, nullptr, &sc.views[i]) != VK_SUCCESS) {
                CP_LOG_THROW("Failed to create image views!");
            }
        }

        return sc;
    }

    void Vulkan::destroySwapchain(Swapchain* swapchain) {
        if(swapchain == nullptr) return;
        
        if(swapchain->handler != VK_NULL_HANDLE) {
            for (auto imageView : swapchain->views) {
                vkDestroyImageView(m_device, imageView, nullptr);
            }

            vkDestroySwapchainKHR(m_device, swapchain->handler, nullptr);
            swapchain->handler = VK_NULL_HANDLE;
        }
    }

    void Vulkan::createSingleTimeCommandsPool() {
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.pNext = nullptr;
        cmdPoolInfo.queueFamilyIndex = GetQueueFamilyIndices().graphicsFamily.value();
        
        if(vkCreateCommandPool(GetDevice(), &cmdPoolInfo, nullptr, &m_singleTimeCmdPool) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create single time commands pool!");
        }
    }

    void Vulkan::destroySingleTimeCommandsPool() {
        if(m_singleTimeCmdPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(GetDevice(), m_singleTimeCmdPool, nullptr);
            m_singleTimeCmdPool = VK_NULL_HANDLE;
        }
    }

#pragma endregion VULKAN_INITIALIZATION

#pragma region HELPERS
    std::vector<const char*> Vulkan::getGlfwRequiredExtensions() {
        uint32_t count = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);

        if(!glfwExtensions) CP_LOG_THROW("GLFW NOT INITIALIZED OR FAILED TO OBTAIN REQUIRED EXTENSIONS");

        std::vector<const char*> ext(glfwExtensions, glfwExtensions + count);

        if(enableValidationLayers) ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        ext.insert(ext.end(), m_requiredExtensions.begin(), m_requiredExtensions.end());

        return ext;
    }

    bool Vulkan::checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : m_validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    bool Vulkan::checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    bool Vulkan::isDeviceSuitable(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties2 deviceProp { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        vkGetPhysicalDeviceProperties2(device, &deviceProp);

        VkPhysicalDeviceFeatures2 deviceFeat { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        vkGetPhysicalDeviceFeatures2(device, &deviceFeat);

        QueueFamilyIndices indices = findQueueFamilies(device, m_surface);

        bool extensionsSupported = checkDeviceExtensionSupport(device);
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, m_surface);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        bool isDiscreteOrIntegrated = 
            deviceProp.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
            deviceProp.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

        return isDiscreteOrIntegrated && indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    Vulkan::QueueFamilyIndices Vulkan::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        // --- 1) Query queue family properties (use *2 to allow pNext extensions) ---
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
        for (auto &q : queueFamilies) {
            q.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
            q.pNext = nullptr;
        }
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilies.data());

        // --- 2) Query device features for sparse binding and protected memory support ---
        VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = nullptr };
        VkPhysicalDeviceProtectedMemoryFeatures protectedFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES, .pNext = nullptr };
        features2.pNext = &protectedFeatures;
        vkGetPhysicalDeviceFeatures2(device, &features2);

        bool supportsSparseBinding = features2.features.sparseBinding == VK_TRUE;
        bool supportsProtectedMemory = protectedFeatures.protectedMemory == VK_TRUE;


        // --- 3) Iterate queue families and pick candidates ---
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            const auto& props = queueFamilies[i].queueFamilyProperties;
            VkQueueFlags flags = props.queueFlags;

            // --- Present support (surface) ---
            VkBool32 presentSupport = VK_FALSE;
            if (surface != VK_NULL_HANDLE) {
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            }

            // NOTE: some drivers expose whether a queue supports protected submissions via a
            // per-family property in the pNext chain of VkQueueFamilyProperties2.
            // To be robust, you can attempt to read that structure here (example below).
            bool familySupportsProtected = false;
            if (supportsProtectedMemory) {
                // Example: many Vulkan SDKs provide VkQueueFamilyProtectedProperties / KHR equivalent.
                // Try to read it from the pNext chain of queueFamilies[i] if available.
                // We'll use a generic approach (safe-cast) — if your build has the struct, you can
                // query it explicitly; otherwise leave familySupportsProtected = false.
                VkQueueFamilyProperties2 qf2 = queueFamilies[i];
                // If your SDK has VkQueueFamilyProtectedProperties, you can use it like:
                // VkQueueFamilyProtectedProperties protectedProps{ .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROTECTED_PROPERTIES };
                // protectedProps.pNext = nullptr;
                // qf2.pNext = &protectedProps;
                // vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilies.data());
                // familySupportsProtected = (protectedProps.protectedSubmit == VK_TRUE);
                // For portability here, we'll leave as false unless you enable the query above.
            }

            // --- Graphics + Present ---
            if (flags & VK_QUEUE_GRAPHICS_BIT) {
                if (!indices.graphicsFamily.has_value())
                    indices.graphicsFamily = i;
            }
            if (presentSupport && !indices.presentFamily.has_value())
                indices.presentFamily = i;

            // --- Compute-only (prefer exclusive) ---
            if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.computeFamily = i;
            }

            // --- Transfer-only (prefer exclusive) ---
            if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))) {
                indices.transferFamily = i;
            }
        }

        // --- 4) Fallback chain to ensure all fields are filled ---
        // If the system has only one family, these will all resolve to the same index.
        // --- Fallbacks ---
        if (!indices.computeFamily.has_value()) {
            // fallback: use any compute queue
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                if (queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    indices.computeFamily = i;
                    break;
                }
            }
        }

        if (!indices.transferFamily.has_value()) {
            // fallback: use any transfer queue
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                if (queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                    indices.transferFamily = i;
                    break;
                }
            }
        }

        if (!indices.presentFamily.has_value())
            indices.presentFamily = indices.graphicsFamily;

        return indices;
    }

    Vulkan::SwapChainSupportDetails Vulkan::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
       VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
            .pNext = nullptr,
            .surface = surface
        };

        SwapChainSupportDetails details{};
        details.capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
        details.capabilities.pNext = nullptr;

        VkResult result = VK_SUCCESS;

        // --- Surface Capabilities ---
        if (auto func = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR>(vkGetInstanceProcAddr(m_instance, "vkGetPhysicalDeviceSurfaceCapabilities2KHR"))) {
            result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &surfaceInfo, &details.capabilities);
        } else {
            result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities.surfaceCapabilities);
        }

        if (result != VK_SUCCESS) {
            CP_LOG_WARN("SwapChain query failed with VkResult = {}", std::to_string(result));
        }

        // --- Surface Formats ---
        uint32_t formatCount = 0;
        if (auto func = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormats2KHR>(vkGetInstanceProcAddr(m_instance, "vkGetPhysicalDeviceSurfaceFormats2KHR"))) {
            result = vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surfaceInfo, &formatCount, nullptr);
            if (formatCount != 0) {
                details.formats.resize(formatCount);
                for (auto& fmt : details.formats) {
                    fmt.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
                    fmt.pNext = nullptr;
                }
                result = vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surfaceInfo, &formatCount, details.formats.data());
            }
        } else {
            result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
            if (formatCount != 0) {
                std::vector<VkSurfaceFormatKHR> formats(formatCount);
                result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
                details.formats.resize(formatCount);
                for (uint32_t i = 0; i < formatCount; ++i) {
                    details.formats[i].surfaceFormat = formats[i];
                    details.formats[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
                    details.formats[i].pNext = nullptr;
                }
            }
        }

        if (result != VK_SUCCESS) {
            CP_LOG_WARN("SwapChain query failed with VkResult = {}", std::to_string(result));
        }

        // --- Present Modes ---
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormat2KHR Vulkan::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormat2KHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            
            if (availableFormat.surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR Vulkan::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR preferredMode) {
        if (std::find(availablePresentModes.begin(), availablePresentModes.end(), preferredMode) != availablePresentModes.end()) {
            return preferredMode;
        }

        for (const auto& availablePresentMode : availablePresentModes) {

            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Vulkan::chooseSwapExtent(const VkSurfaceCapabilities2KHR& capabilities) {
        if (capabilities.surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.surfaceCapabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(m_wndHandler, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.surfaceCapabilities.minImageExtent.width, capabilities.surfaceCapabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.surfaceCapabilities.minImageExtent.height, capabilities.surfaceCapabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    VkFormat Vulkan::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_physDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        CP_LOG_THROW("Failed to find suitable format!");
        return VK_FORMAT_UNDEFINED;
    }

    VkFormat Vulkan::findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool Vulkan::hasStencilFormat(const VkFormat& format) const {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void Vulkan::logDeviceFeatures(
            const VkPhysicalDeviceFeatures2& supported,
            const VkPhysicalDeviceVulkan11Features& supported11,
            const VkPhysicalDeviceVulkan12Features& supported12,
            const VkPhysicalDeviceVulkan13Features& supported13,
            const VkPhysicalDeviceFeatures2& enabled,
            const VkPhysicalDeviceVulkan11Features& enabled11,
            const VkPhysicalDeviceVulkan12Features& enabled12,
            const VkPhysicalDeviceVulkan13Features& enabled13) {
                CP_LOG_INFO("===== Vulkan Device Feature Report =====");

        auto logFeature = [](const char* name, VkBool32 supported, VkBool32 enabled) {
            std::string status = supported ? (enabled ? "ENABLED" : "AVAILABLE") : "UNSUPPORTED";
            CP_LOG_INFO("  {:<35} {}", name, status);
        };

        CP_LOG_INFO(">> Vulkan 1.0 Features");
        logFeature("samplerAnisotropy", supported.features.samplerAnisotropy, enabled.features.samplerAnisotropy);
        logFeature("sampleRateShading", supported.features.sampleRateShading, enabled.features.sampleRateShading);
        logFeature("fillModeNonSolid", supported.features.fillModeNonSolid, enabled.features.fillModeNonSolid);
        logFeature("wideLines", supported.features.wideLines, enabled.features.wideLines);
        logFeature("geometryShader", supported.features.geometryShader, enabled.features.geometryShader);
        logFeature("tessellationShader", supported.features.tessellationShader, enabled.features.tessellationShader);

        CP_LOG_INFO(">> Vulkan 1.1 Features");
        logFeature("multiview", supported11.multiview, enabled11.multiview);
        logFeature("protectedMemory", supported11.protectedMemory, enabled11.protectedMemory);
        logFeature("samplerYcbcrConversion", supported11.samplerYcbcrConversion, enabled11.samplerYcbcrConversion);
        logFeature("shaderDrawParameters", supported11.shaderDrawParameters, enabled11.shaderDrawParameters);

        CP_LOG_INFO(">> Vulkan 1.2 Features");
        logFeature("scalarBlockLayout", supported12.scalarBlockLayout, enabled12.scalarBlockLayout);
        logFeature("descriptorIndexing", supported12.descriptorIndexing, enabled12.descriptorIndexing);
        logFeature("imagelessFramebuffer", supported12.imagelessFramebuffer, enabled12.imagelessFramebuffer);
        logFeature("uniformBufferStandardLayout", supported12.uniformBufferStandardLayout, enabled12.uniformBufferStandardLayout);
        logFeature("separateDepthStencilLayouts", supported12.separateDepthStencilLayouts, enabled12.separateDepthStencilLayouts);
        logFeature("hostQueryReset", supported12.hostQueryReset, enabled12.hostQueryReset);
        logFeature("timeline semaphore", supported12.timelineSemaphore, enabled12.timelineSemaphore);

        CP_LOG_INFO(">> Vulkan 1.3 Features");
        logFeature("dynamicRendering", supported13.dynamicRendering, enabled13.dynamicRendering);
        logFeature("synchronization2", supported13.synchronization2, enabled13.synchronization2);
        logFeature("maintenance4", supported13.maintenance4, enabled13.maintenance4);

        CP_LOG_INFO("========================================");
    }

    void Vulkan::logSelectedGPU(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physDevice, &props);

        std::string typeStr;
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: typeStr = "Discrete GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeStr = "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: typeStr = "Virtual GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU: typeStr = "CPU (Software Rasterizer)"; break;
            default: typeStr = "Other/Unknown"; break;
        }

        uint32_t apiVersion = props.apiVersion;
        uint32_t apiMajor = VK_VERSION_MAJOR(apiVersion);
        uint32_t apiMinor = VK_VERSION_MINOR(apiVersion);
        uint32_t apiPatch = VK_VERSION_PATCH(apiVersion);

        uint32_t driverVersion = props.driverVersion;

        CP_LOG_INFO("============================================================");
        CP_LOG_INFO("[ GPU SELECIONADA ]");
        CP_LOG_INFO("  Nome:                 {}", props.deviceName);
        CP_LOG_INFO("  Tipo:                 {}", typeStr);
        CP_LOG_INFO("  Vulkan API Version:   {}.{}.{}", apiMajor, apiMinor, apiPatch);
        CP_LOG_INFO("  Driver Version:       {}", driverVersion);
        CP_LOG_INFO("  Vendor ID:            0x{:04X}", props.vendorID);
        CP_LOG_INFO("  Device ID:            0x{:04X}", props.deviceID);
        CP_LOG_INFO("============================================================");

        CP_LOG_INFO("[ LIMITES DO DISPOSITIVO ]");
        CP_LOG_INFO("  Max Image 2D:                {}", props.limits.maxImageDimension2D);
        CP_LOG_INFO("  Max Bound Descriptor Sets:   {}", props.limits.maxBoundDescriptorSets);
        CP_LOG_INFO("  Max Push Constants:          {} bytes", props.limits.maxPushConstantsSize);
        CP_LOG_INFO("============================================================");

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memProps);

        CP_LOG_INFO("[ MEMORIA ]");
        CP_LOG_INFO("  Heaps encontrados: {}", memProps.memoryHeapCount);
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            const auto& heap = memProps.memoryHeaps[i];
            float sizeGB = heap.size / (1024.0f * 1024.0f * 1024.0f);
            CP_LOG_INFO("    Heap {:>2}: {:>6.2f} GB ({})",
                        i, sizeGB,
                        (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "Device Local" : "Host Visible");
        }
        CP_LOG_INFO("============================================================");
    }

    #pragma endregion HELPERS
} // namespace cp_api

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
        if (pCallbackData && pCallbackData->pMessage) {
            const char* severityStr = "UNKNOWN";
            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            severityStr = "ERROR";
            } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            severityStr = "WARNING";
            } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            severityStr = "INFO";
            } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            severityStr = "VERBOSE";
            }

            std::string typeStr;
            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            if (!typeStr.empty()) typeStr += "/";
            typeStr += "GENERAL";
            }
            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            if (!typeStr.empty()) typeStr += "/";
            typeStr += "VALIDATION";
            }
            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            if (!typeStr.empty()) typeStr += "/";
            typeStr += "PERFORMANCE";
            }
            if (typeStr.empty()) typeStr = "UNKNOWN";

            std::string formatted = std::string("[VULKAN][") + severityStr + "][" + typeStr + "] " + pCallbackData->pMessage;

            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            CP_LOG_ERROR("{}", formatted);
            } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            CP_LOG_WARN("{}", formatted);
            } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            CP_LOG_INFO("{}", formatted);
            } else {
            CP_LOG_DEBUG("{}", formatted);
            }
        }
        return VK_FALSE;
    }