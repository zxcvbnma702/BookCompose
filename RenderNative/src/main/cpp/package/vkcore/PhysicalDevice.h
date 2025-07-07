//
// Created by nio on 2025/7/7.
//

#ifndef BOOKCOMPOSE_PHYSICALDEVICE_H
#define BOOKCOMPOSE_PHYSICALDEVICE_H

#include <list>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "Log.h"
#include "Utils.h"

namespace VkCore{
    class PhysicalDevice final{
    public:
        explicit PhysicalDevice()= default;
        explicit PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                                const std::vector<std::string>& requestedExtensions,
                                bool printEnumerations = false, bool enableRayTracing = false);
    private:
//        VkPhysicalDeviceProperties2 properties_ = {
//                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
//                .pNext = &rayTracingPipelineProperties_,
//        };

    private:
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        // 设备支持的扩展
        std::vector<std::string> extensions_;

        // 队列族属性
        std::vector<VkQueueFamilyProperties> queueFamilyProperties_;
        // 要使用的扩展
        std::unordered_set<std::string> enabledExtensions_;
    };
}


#endif //BOOKCOMPOSE_PHYSICALDEVICE_H
