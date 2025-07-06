//
// Created by nio on 2025/7/6.
//
#include "Context.h"


std::vector<std::string> VkCore::Context::enumerateInstanceLayers(bool printEnumerations) {
    uint32_t instanceLayerCount{0};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
    std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data()));

    std::vector<std::string> instanceLayerNames;
    std::transform(instanceLayers.begin(), instanceLayers.end(),
                   std::back_inserter(instanceLayerNames),
                   [](const VkLayerProperties &layerProperties) {
                       return std::string(layerProperties.layerName);
                   });

    if (printEnumerations) {
        std::cerr << "FOUND " << instanceLayerCount << " available layers" << std::endl;
        for (const auto &layerName: instanceLayerNames) {
            std::cerr << "\t" << layerName << std::endl;
        };
    }

    return instanceLayerNames;
}

std::vector<std::string> VkCore::Context::enumerateInstanceExtensions() {
    uint32_t extensionCount{0};
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
    std::vector<VkExtensionProperties> extensions(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()));

    std::vector<std::string> extensionNames;
    std::transform(extensions.begin(), extensions.end(),
                   std::back_inserter(extensionNames),
                   [](const VkExtensionProperties &extensionProperties) {
        return std::string(extensionProperties.extensionName);
    });

    if (printEnumerations_) {
        std::cerr << "Found " << extensionCount << " extension(s) for the instance"
                  << std::endl;
        for (const auto& layer : extensionNames) {
            std::cerr << "\t" << layer << std::endl;
        }
    }

    return extensionNames;
}

VkCore::Context::Context(const VkApplicationInfo &appInfo,
                         const std::vector<std::string> &requestedLayers,
                         const std::vector<std::string> &requestedInstanceExtensions,
                         bool printEnumerations, const std::string &name)
                         : applicationInfo_{appInfo}, printEnumerations_{printEnumerations}{
    /**
     * This will attempt to load Vulkan loader from the system;
     * if this function returns VK_SUCCESS you can proceed to create Vulkan instance.
     * If this function fails, this means Vulkan loader isn't installed on your system.
     */
    VK_CHECK(volkInitialize());

    enabledLayers_ = util::filterExtensions(enumerateInstanceLayers(),
                                            requestedLayers);
    enabledInstanceExtensions_ = util::filterExtensions(enumerateInstanceExtensions(),
                                                        requestedInstanceExtensions);

    std::vector<const char *> instanceLayers(enabledLayers_.size());
    std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                   [](const std::string &layerName) {
                       return layerName.c_str();
                   });

    {
        std::vector<const char *> instanceExtensions(enabledInstanceExtensions_.size());
        std::transform(enabledInstanceExtensions_.begin(), enabledInstanceExtensions_.end(),
                       instanceExtensions.begin(),
                       [](const std::string &extensionName) {
                           return extensionName.c_str();
                       });

        const VkValidationFeatureEnableEXT validationFeaturesEnabled[] = {
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
                VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        };

        const VkValidationFeaturesEXT features = {
                .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
                .pNext = nullptr,
                .enabledValidationFeatureCount = sizeof(validationFeaturesEnabled) / sizeof(VkValidationFeatureEnableEXT),
                . pEnabledValidationFeatures = validationFeaturesEnabled,
        };

        const VkInstanceCreateInfo instanceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = &features,
                .flags = 0,
                .pApplicationInfo = &applicationInfo_,
                .enabledLayerCount = static_cast<uint32_t>(enabledLayers_.size()),
                .ppEnabledLayerNames = instanceLayers.data(),
                .enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions_.size()),
                .ppEnabledExtensionNames = instanceExtensions.data()
        };

        VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance_));

        ASSERT(instance_ != VK_NULL_HANDLE, "Error creating VkInstance");
    }

    /**
     * This function will load all required Vulkan entrypoints,
     * including all extensions; you can use Vulkan from here on as usual.
     */
    volkLoadInstance(instance_);

//#if defined(VK_EXT_debug_utils)
//    if (enabledInstanceExtensions_.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
//        const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
//                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
//                .flags = 0,
//                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
//                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
//                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
//                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
//                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
//                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
//                               #if defined(VK_EXT_device_address_binding_report)
//                               | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
//#endif
//                ,
//                .pfnUserCallback = &debugMessengerCallback,
//                .pUserData = nullptr,
//        };
//        VK_CHECK(
//                vkCreateDebugUtilsMessengerEXT(instance_, &messengerInfo, nullptr, &messenger_));
//    }
//#endif
//
//    if (device_) {
//        setVkObjectname(instance_, VK_OBJECT_TYPE_INSTANCE, "Instance: " + name);
//    }

    // todo
}

VkCore::Context::Context(Window& window, const std::vector<std::string> &requestedLayers,
                         const std::vector<std::string> &requestedInstanceExtensions,
                         const std::vector<std::string> &requestedDeviceExtensions,
                         VkQueueFlags requestedQueueTypes, bool printEnumerations,
                         bool enableRayTracing, const std::string &name) :
                         printEnumerations_{printEnumerations} {

    VK_CHECK(volkInitialize());

    enabledLayers_ = util::filterExtensions(enumerateInstanceLayers(), requestedLayers);
    enabledInstanceExtensions_ =
            util::filterExtensions(enumerateInstanceExtensions(), requestedInstanceExtensions);

    std::vector<const char*> instanceLayers(enabledLayers_.size());
    std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                   std::mem_fn(&std::string::c_str));

    {
        std::vector<const char*> instanceExtensions(enabledInstanceExtensions_.size());
        std::transform(enabledInstanceExtensions_.begin(), enabledInstanceExtensions_.end(),
                       instanceExtensions.begin(), std::mem_fn(&std::string::c_str));

    }

//    todo

    surface_ = VkCore::createSurface(instance_, window);

    //todo
}
