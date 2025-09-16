//
// Created by nio on 2025/7/6.
//

#ifndef BOOKCOMPOSE_CONTEXT_H
#define BOOKCOMPOSE_CONTEXT_H

#include "Utils.h"
#include "Common.h"
#include "Surface.h"
#include "PhysicalDevice.h"
#include "vk_mem_alloc.h"

namespace {
#if defined(VK_EXT_debug_utils)
    VkBool32 VKAPI_PTR debugMessengerCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageTypes,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
            LOGE("debugMessengerCallback : MessageCode is %s & Message is %s",
                 pCallbackData->pMessageIdName, pCallbackData->pMessage);
#if defined(_WIN32)
            __debugbreak();
#else
            raise(SIGTRAP);
#endif
        } else if (messageSeverity & (~VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
            LOGW("debugMessengerCallback : MessageCode is %s & Message is %s",
                 pCallbackData->pMessageIdName, pCallbackData->pMessage);
        } else {
            LOGI("debugMessengerCallback : MessageCode is %s & Message is %s",
                 pCallbackData->pMessageIdName, pCallbackData->pMessage);
        }

        return VK_FALSE;
    }
#endif
}  // namespace

namespace VkCore{
    class Context final{
    public:
        MOVABLE_ONLY(Context)

        explicit Context(Window& window, const std::vector<std::string>& requestedLayers,
                         const std::vector<std::string>& requestedInstanceExtensions,
                         const std::vector<std::string>& requestedDeviceExtensions,
                         VkQueueFlags requestedQueueTypes, bool printEnumerations = false,
                         bool enableRayTracing = false, const std::string& name = "");

        explicit Context(const VkApplicationInfo& appInfo,
                         const std::vector<std::string>& requestedLayers,
                         const std::vector<std::string>& requestedInstanceExtensions,
                         bool printEnumerations = false, const std::string& name = "");

        void createVkDevice(VkPhysicalDevice vkPhysicalDevice,
                            const std::vector<std::string>& requestedDeviceExtensions,
                            VkQueueFlags requestedQueueTypes, const std::string& name = "");

        ~Context();

        [[nodiscard]] VkDevice device() const { return device_; }

        [[nodiscard]] VkInstance instance() const { return instance_; }

        [[nodiscard]] const PhysicalDevice& physicalDevice() const { return physicalDevice_; }

        [[nodiscard]] VkQueue graphicsQueue(int index = 0) const { return graphicsQueues_[index]; }

        [[nodiscard]] VmaAllocator memoryAllocator() const { return allocator_; }

        void dumpMemoryStats(const std::string& fileName) const;
    private:
        void createMemoryAllocator();

        [[nodiscard]] static std::vector<std::string> enumerateInstanceLayers(
                bool printEnumerations_ = false);

        [[nodiscard]] std::vector<std::string> enumerateInstanceExtensions() const;

        [[nodiscard]] std::vector<PhysicalDevice> enumeratePhysicalDevices(
                const std::vector<std::string>& requestedExtensions, bool enableRayTracing) const;

        [[nodiscard]] PhysicalDevice choosePhysicalDevice(
                std::vector<PhysicalDevice>&& devices,
                const std::vector<std::string>& deviceExtensions) const;

    private:
        const VkApplicationInfo applicationInfo_ = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "BookCompose",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_3,
        };

        VkInstance instance_ = VK_NULL_HANDLE;
        bool printEnumerations_ = false;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        PhysicalDevice physicalDevice_;
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue presentationQueue_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = nullptr;

        // these are extra queues which can be used for any other async stuff if
        // required, these won't contain above queues
        std::vector<VkQueue> graphicsQueues_;
        std::vector<VkQueue> computeQueues_;
        std::vector<VkQueue> transferQueues_;
        std::vector<VkQueue> sparseQueues_;

        // 可用的实例层
        std::unordered_set<std::string> enabledLayers_;
        // 可用的实例扩展
        std::unordered_set<std::string> enabledInstanceExtensions_;

#if defined(VK_EXT_debug_utils)
        VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
#endif
    };
}

#endif //BOOKCOMPOSE_CONTEXT_H
