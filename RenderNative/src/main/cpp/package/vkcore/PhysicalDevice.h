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
    };
}


#endif //BOOKCOMPOSE_PHYSICALDEVICE_H
