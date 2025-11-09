#pragma once

#include "glfw.inc.hpp"
#include <vector>
#include <optional>

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

namespace cp_api {
    class Vulkan {
    public:
       struct QueueFamilyIndices {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;
            std::optional<uint32_t> computeFamily;
            std::optional<uint32_t> transferFamily;

            bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
        };

        struct FamiliesQueue {
            VkQueue graphics, present, compute, transfer;
        };

        struct SwapChainSupportDetails {
            VkSurfaceCapabilities2KHR capabilities;
            std::vector<VkSurfaceFormat2KHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        Vulkan(GLFWwindow* window);
        ~Vulkan();

        Vulkan(const Vulkan&) = delete;
        Vulkan(Vulkan&&) = delete;
        Vulkan& operator=(const Vulkan&) = delete;
        Vulkan& operator=(Vulkan&&) = delete;

        VkInstance& GetInstance() { return m_instance; }
        VkDevice& GetDevice() { return m_device; }
        VkPhysicalDevice& GetPhysicalDevice() { return m_physDevice; }

    private:
        void createInstance();
        void destroyInstance();

        void createSurface();
        void destroySurface();

        void createDebugMessenger();
        void destroyDebugMessenger();

        void pickPhysicalDevice();

        void createLogicalDevice();
        void destroyLogicalDevice();

        std::vector<const char*> getGlfwRequiredExtensions();
        bool checkValidationLayerSupport();
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);

        bool isDeviceSuitable(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

        void logDeviceFeatures(
            const VkPhysicalDeviceFeatures2& supported,
            const VkPhysicalDeviceVulkan11Features& supported11,
            const VkPhysicalDeviceVulkan12Features& supported12,
            const VkPhysicalDeviceVulkan13Features& supported13,
            const VkPhysicalDeviceFeatures2& enabled,
            const VkPhysicalDeviceVulkan11Features& enabled11,
            const VkPhysicalDeviceVulkan12Features& enabled12,
            const VkPhysicalDeviceVulkan13Features& enabled13);

        void logSelectedGPU(VkPhysicalDevice device);
    private:
        GLFWwindow*                 m_wndHandler;
        VkSurfaceKHR                m_surface = VK_NULL_HANDLE;
        VkInstance                  m_instance = VK_NULL_HANDLE;
        VkDevice                    m_device = VK_NULL_HANDLE;
        QueueFamilyIndices          m_familyIndices;
        FamiliesQueue               m_familyQueues;
        VkPhysicalDevice            m_physDevice = VK_NULL_HANDLE;

        std::vector<const char*>    m_validationLayers = { "VK_LAYER_KHRONOS_validation" };
        std::vector<const char*>    m_deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDebugUtilsMessengerEXT    m_debugMessenger = VK_NULL_HANDLE;
    };
} // namespace cp_api
