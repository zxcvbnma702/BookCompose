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

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            std::cout << "QueueFamily[" << i << "]: ";
            if (queueFamilyProperties_[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                std::cout << "Graphics ";
            if (queueFamilyProperties_[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                std::cout << "Compute ";
            if (queueFamilyProperties_[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
                std::cout << "Transfer ";
            if (queueFamilyProperties_[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
                std::cout << "Sparse ";
            std::cout << std::endl;
        }
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

VkPhysicalDevice VkCore::PhysicalDevice::vkPhysicalDevice() const { return physicalDevice_; }

const std::vector<std::string> &VkCore::PhysicalDevice::extensions() const { return extensions_; }

void VkCore::PhysicalDevice::reserveQueues(VkQueueFlags requestedQueueTypes, VkSurfaceKHR surface) {

    ASSERT(requestedQueueTypes > 0, "Requested queue types is empty");

    for (uint32_t queueFamilyIndex = 0;
         queueFamilyIndex < queueFamilyProperties_.size(); ++queueFamilyIndex) {
        // 选择支持呈现的队列族
        if (!presentationFamilyIndex_.has_value() && surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = false;
            VK_CHECK(
                    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, queueFamilyIndex, surface,
                                                         &presentSupport));
            if (presentSupport == VK_TRUE) {
                presentationFamilyIndex_ = queueFamilyIndex;
                presentationQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            }
        }
        if (!graphicsFamilyIndex_.has_value() &&
            (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex_ = queueFamilyIndex;
            graphicsQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_GRAPHICS_BIT;
            continue;
        }
        if (!computeFamilyIndex_.has_value() &&
            (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_COMPUTE_BIT) {
            computeFamilyIndex_ = queueFamilyIndex;
            computeQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_COMPUTE_BIT;
            continue;
        }

        if (!transferFamilyIndex_.has_value() &&
            (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_TRANSFER_BIT) {
            transferFamilyIndex_ = queueFamilyIndex;
            transferQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_TRANSFER_BIT;
            continue;
        }

        if (!sparseFamilyIndex_.has_value() &&
            (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
            VK_QUEUE_SPARSE_BINDING_BIT) {
            sparseFamilyIndex_ = queueFamilyIndex;
            sparseQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
            requestedQueueTypes &= ~VK_QUEUE_SPARSE_BINDING_BIT;
            continue;
        }
    }

    ASSERT(graphicsFamilyIndex_.has_value() || computeFamilyIndex_.has_value() ||
           transferFamilyIndex_.has_value() || sparseFamilyIndex_.has_value(),
           "No suitable queue(s) found");

    ASSERT(surface == VK_NULL_HANDLE || presentationFamilyIndex_.has_value(),
           "No queues with presentation capabilities found");
}

[[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>>
VkCore::PhysicalDevice::queueFamilyIndexAndCount() const {
    std::set<std::pair<uint32_t, uint32_t>> familyIndices;
    if (graphicsFamilyIndex_.has_value()) {
        familyIndices.insert({graphicsFamilyIndex_.value(), graphicsQueueCount_});
    }
    if (computeFamilyIndex_.has_value()) {
        familyIndices.insert({computeFamilyIndex_.value(), computeQueueCount_});
    }
    if (transferFamilyIndex_.has_value()) {
        familyIndices.insert({transferFamilyIndex_.value(), transferQueueCount_});
    }
    if (sparseFamilyIndex_.has_value()) {
        familyIndices.insert({sparseFamilyIndex_.value(), sparseQueueCount_});
    }
    if (presentationFamilyIndex_.has_value()) {
        familyIndices.insert({presentationFamilyIndex_.value(), presentationQueueCount_});
    }
    std::vector<std::pair<uint32_t, uint32_t>> result(familyIndices.begin(), familyIndices.end());

    return result;
}

std::optional<uint32_t> VkCore::PhysicalDevice::graphicsFamilyIndex() const {
    return graphicsFamilyIndex_;
}

std::optional<uint32_t> VkCore::PhysicalDevice::computeFamilyIndex() const {
    return computeFamilyIndex_;
}

std::optional<uint32_t> VkCore::PhysicalDevice::transferFamilyIndex() const {
    return transferFamilyIndex_;
}

std::optional<uint32_t> VkCore::PhysicalDevice::sparseFamilyIndex() const {
    return sparseFamilyIndex_;
}

std::optional<uint32_t> VkCore::PhysicalDevice::presentationFamilyIndex() const {
    return presentationFamilyIndex_;
}
