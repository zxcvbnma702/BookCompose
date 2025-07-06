//
// Created by nio on 2025/7/6.
//

#ifndef BOOKCOMPOSE_CONTEXT_H
#define BOOKCOMPOSE_CONTEXT_H
#define VK_NO_PROTOTYPES
#include "Utils.h"
#include "vulkan/vulkan.h"
#include "Log.h"
#include "../volk/volk.h"
#include "Surface.h"

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
    private:
        [[nodiscard]] static std::vector<std::string> enumerateInstanceLayers(
                bool printEnumerations_ = false);

        [[nodiscard]] std::vector<std::string> enumerateInstanceExtensions();

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

        // 可用的实例层
        std::unordered_set<std::string> enabledLayers_;
        // 可用的实例扩展
        std::unordered_set<std::string> enabledInstanceExtensions_;
    };
}

#endif //BOOKCOMPOSE_CONTEXT_H
