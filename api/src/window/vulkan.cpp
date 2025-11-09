#include "cp_api/window/vulkan.hpp"
#include "cp_api/core/debug.hpp"
#include <cstring>
#include <set>

namespace cp_api {
    Vulkan::Vulkan(GLFWwindow* window) : m_wndHandler(window) {
        createInstance();
        createSurface();
        createDebugMessenger();
        pickPhysicalDevice();
        createLogicalDevice();
    }

    Vulkan::~Vulkan() {
        destroyLogicalDevice();
        destroyDebugMessenger();
        destroySurface();
        destroyInstance();
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
        if(m_instance) vkDestroyInstance(m_instance, nullptr);
    }

    void Vulkan::createSurface() {
        if(glfwCreateWindowSurface(m_instance, m_wndHandler, nullptr, &m_surface) != VK_SUCCESS) {
            CP_LOG_THROW("Failed to create window surface!");
        }
    }

    void Vulkan::destroySurface() {
        if(m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
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
        createInfo.pEnabledFeatures = nullptr; // porque estamos usando a struct *2 e pNext

        // Extensões do device
        std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

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
        VkDeviceQueueInfo2 queueInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
        queueInfo.queueIndex = 0;

        queueInfo.queueFamilyIndex = m_familyIndices.graphicsFamily.value();
        vkGetDeviceQueue2(m_device, &queueInfo, &m_familyQueues.graphics);

        queueInfo.queueFamilyIndex = m_familyIndices.presentFamily.value();
        vkGetDeviceQueue2(m_device, &queueInfo, &m_familyQueues.present);

        queueInfo.queueFamilyIndex = m_familyIndices.computeFamily.value();
        vkGetDeviceQueue2(m_device, &queueInfo, &m_familyQueues.compute);

        queueInfo.queueFamilyIndex = m_familyIndices.transferFamily.value();
        vkGetDeviceQueue2(m_device, &queueInfo, &m_familyQueues.transfer);
    }

    void Vulkan::destroyLogicalDevice() {
        if(m_device != VK_NULL_HANDLE) vkDestroyDevice(m_device, nullptr);
    }

#pragma endregion VULKAN_INITIALIZATION

#pragma region HELPERS
    std::vector<const char*> Vulkan::getGlfwRequiredExtensions() {
        uint32_t count = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);

        if(!glfwExtensions) CP_LOG_THROW("GLFW NOT INITIALIZED OR FAILED TO OBTAIN REQUIRED EXTENSIONS");

        std::vector<const char*> ext(glfwExtensions, glfwExtensions + count);

        if(enableValidationLayers) ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        ext.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

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
        if (vkGetPhysicalDeviceSurfaceCapabilities2KHR != nullptr) {
            result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &surfaceInfo, &details.capabilities);
        } else {
            result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities.surfaceCapabilities);
        }

        // --- Surface Formats ---
        uint32_t formatCount = 0;
        if (vkGetPhysicalDeviceSurfaceFormats2KHR != nullptr) {
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

        // --- Present Modes ---
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
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
            } else if ("{}", messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            CP_LOG_INFO("{}", formatted);
            } else {
            CP_LOG_DEBUG("{}", formatted);
            }
        }
        return VK_FALSE;
    }