//
// Created by nio on 2025/7/7.
//

#include "PhysicalDevice.h"

VkCore::PhysicalDevice::PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                                       const std::vector<std::string> &requestedExtensions,
                                       bool printEnumerations, bool enableRayTracing) :
        physicalDevice_{device} {


    {
        // 队列族
        uint32_t queueFamilyCount{0};
        vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice_, &queueFamilyCount, nullptr);
        queueFamilyProperties_.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice_, &queueFamilyCount, queueFamilyProperties_.data());
    }

    {
        // 扩展
        uint32_t propertyCount{0};
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr,
                                                      &propertyCount, nullptr))
        std::vector<VkExtensionProperties> properties(propertyCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr,
                                                      &propertyCount, properties.data()))

        std::transform(properties.begin(), properties.end(), std::back_inserter(extensions_),
                       [](const VkExtensionProperties &property) {
                           return std::string(property.extensionName);
                       });

        enabledExtensions_ = util::filterExtensions(extensions_, requestedExtensions);
    }
}
