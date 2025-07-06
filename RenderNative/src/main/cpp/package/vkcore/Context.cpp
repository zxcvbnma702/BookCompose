//
// Created by nio on 2025/7/6.
//
#define VOLK_IMPLEMENTATION
#include "Context.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <map>

constexpr bool DEBUG_SHADER_PRINTF_CALLBACK = false;

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

std::vector<std::string> VkCore::Context::enumerateInstanceExtensions() const {
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

#if defined(VK_EXT_debug_utils)
    if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != enabledInstanceExtensions_.end()) {
        const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .flags = 0,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                               #if defined(VK_EXT_device_address_binding_report)
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
#endif
                ,
                .pfnUserCallback = &debugMessengerCallback,
                .pUserData = nullptr,
        };
        VK_CHECK(
                vkCreateDebugUtilsMessengerEXT(instance_, &messengerInfo, nullptr, &messenger_));
    }
#endif
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

        // 验证层
        std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnabled;
#if defined(VK_EXT_layer_settings)
        if constexpr (!DEBUG_SHADER_PRINTF_CALLBACK) {
            // 启用 GPU 辅助验证（可以检测复杂的着色器错误)
            validationFeaturesEnabled.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
        } else {
            // 启用调试 printf，可以在 shader 中输出调试信息（从 Vulkan SDK 1.2.182 起支持）
            validationFeaturesEnabled.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
        }
#endif

#if defined(VK_EXT_layer_settings)
        const std::string layer_name = "VK_LAYER_KHRONOS_validation";
        const std::array<const char*, 1> setting_debug_action = {"VK_DBG_LAYER_ACTION_BREAK"};
        const std::array<const char*, 1> setting_gpu_based_action = {
                "GPU_BASED_DEBUG_PRINTF"};
        const std::array<VkBool32, 1> setting_printf_to_stdout = {VK_TRUE};
        const std::array<VkBool32, 1> setting_printf_verbose = {VK_TRUE};
        const std::array<VkLayerSettingEXT, 4> settings = {
                VkLayerSettingEXT{
                        .pLayerName = layer_name.c_str(),
                        .pSettingName = "debug_action",
                        .type = VK_LAYER_SETTING_TYPE_STRING_EXT,
                        .valueCount = 1,
                        .pValues = setting_debug_action.data(),
                },
                VkLayerSettingEXT{
                        .pLayerName = layer_name.c_str(),
                        .pSettingName = "validate_gpu_based",
                        .type = VK_LAYER_SETTING_TYPE_STRING_EXT,
                        .valueCount = 1,
                        .pValues = setting_gpu_based_action.data(),
                },
                VkLayerSettingEXT{
                        .pLayerName = layer_name.c_str(),
                        .pSettingName = "printf_to_stdout",
                        .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
                        .valueCount = 1,
                        .pValues = setting_printf_to_stdout.data(),
                },
                VkLayerSettingEXT{
                        .pLayerName = layer_name.c_str(),
                        .pSettingName = "printf_verbose",
                        .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
                        .valueCount = 1,
                        .pValues = setting_printf_verbose.data(),
                },
        };

        const VkLayerSettingsCreateInfoEXT layer_settings_create_info = {
                .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
                .pNext = nullptr,
                .settingCount = static_cast<uint32_t>(settings.size()),
                .pSettings = settings.data(),
        };
#endif

        const VkValidationFeaturesEXT features = {
                .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
#if defined(VK_EXT_layer_settings)
                .pNext = DEBUG_SHADER_PRINTF_CALLBACK ? &layer_settings_create_info : nullptr,
#endif
                .enabledValidationFeatureCount =
                static_cast<uint32_t>(validationFeaturesEnabled.size()),
                .pEnabledValidationFeatures = validationFeaturesEnabled.data(),
        };

        const VkInstanceCreateInfo instanceInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = &features,
                .pApplicationInfo = &applicationInfo_,
                .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
                .ppEnabledLayerNames = instanceLayers.data(),
                .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
                .ppEnabledExtensionNames = instanceExtensions.data(),
        };
        VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance_));
    }

    volkLoadInstance(instance_);

#if defined(VK_EXT_debug_utils)
    if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != enabledInstanceExtensions_.end()) {
        const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .flags = 0,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                               #if defined(VK_EXT_device_address_binding_report)
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
#endif
                ,
                .pfnUserCallback = &debugMessengerCallback,
                .pUserData = nullptr,
        };
        VK_CHECK(
                vkCreateDebugUtilsMessengerEXT(instance_, &messengerInfo, nullptr, &messenger_));
    }
#endif

    surface_ = VkCore::createSurface(instance_, window);

    physicalDevice_ = choosePhysicalDevice(
            enumeratePhysicalDevices(requestedDeviceExtensions, enableRayTracing),
            requestedDeviceExtensions);

    //todo
}

std::vector<VkCore::PhysicalDevice>
VkCore::Context::enumeratePhysicalDevices(const std::vector<std::string> &requestedExtensions,
                                          bool enableRayTracing) const {
    uint32_t deviceCount{0};
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount,
                                        nullptr));

    ASSERT(deviceCount > 0, "No Vulkan devices found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount,
                                        devices.data()));

    if (printEnumerations_) {
        std::cerr << "Found " << deviceCount << " Vulkan capable device(s)" << std::endl;
    }
    std::vector<PhysicalDevice> physicalDevices;
    for (const auto device: devices) {
        physicalDevices.emplace_back(device, surface_, requestedExtensions,
                                                    printEnumerations_, enableRayTracing);
    }

    return physicalDevices;
}

VkCore::PhysicalDevice VkCore::Context::choosePhysicalDevice(std::vector<PhysicalDevice> &&devices,
                                                     const std::vector<std::string> &deviceExtensions) const {

    (void)deviceExtensions;
    ASSERT(!devices.empty(), "The list of devices can't be empty");


//    for (const auto device : devices) {
//        std::string devicename(device.properties().properties.deviceName);
//        const auto result = devicename.find("NVIDIA");
//        if (result != std::string::npos) {
//            return device;
//        }
//    }
    return devices[0];
}
