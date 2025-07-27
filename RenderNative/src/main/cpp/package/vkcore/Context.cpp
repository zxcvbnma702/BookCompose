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
        for (const auto &layer: extensionNames) {
            std::cerr << "\t" << layer << std::endl;
        }
    }

    return extensionNames;
}

VkCore::Context::Context(const VkApplicationInfo &appInfo,
                         const std::vector<std::string> &requestedLayers,
                         const std::vector<std::string> &requestedInstanceExtensions,
                         bool printEnumerations, const std::string &name)
        : applicationInfo_{appInfo}, printEnumerations_{printEnumerations} {
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
                .enabledValidationFeatureCount = sizeof(validationFeaturesEnabled) /
                                                 sizeof(VkValidationFeatureEnableEXT),
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
    if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
        enabledInstanceExtensions_.end()) {
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
}

VkCore::Context::Context(Window &window, const std::vector<std::string> &requestedLayers,
                         const std::vector<std::string> &requestedInstanceExtensions,
                         const std::vector<std::string> &requestedDeviceExtensions,
                         VkQueueFlags requestedQueueTypes, bool printEnumerations,
                         bool enableRayTracing, const std::string &name) :
        printEnumerations_{printEnumerations} {

    VK_CHECK(volkInitialize());

    enabledLayers_ = util::filterExtensions(enumerateInstanceLayers(), requestedLayers);
    enabledInstanceExtensions_ =
            util::filterExtensions(enumerateInstanceExtensions(), requestedInstanceExtensions);

    std::vector<const char *> instanceLayers(enabledLayers_.size());
    std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                   std::mem_fn(&std::string::c_str));

    {
        std::vector<const char *> instanceExtensions(enabledInstanceExtensions_.size());
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
        const std::array<const char *, 1> setting_debug_action = {"VK_DBG_LAYER_ACTION_BREAK"};
        const std::array<const char *, 1> setting_gpu_based_action = {
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
    if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
        enabledInstanceExtensions_.end()) {
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

    physicalDevice_.reserveQueues(requestedQueueTypes | VK_QUEUE_GRAPHICS_BIT, surface_);

    {
        // create VkDevice
        std::vector<const char *> deviceExtensions(physicalDevice_.enabledExtensions().size());
        std::transform(physicalDevice_.enabledExtensions().begin(),
                       physicalDevice_.enabledExtensions().end(), deviceExtensions.begin(),
                       std::mem_fn(&std::string::c_str));

        const auto familyIndices = physicalDevice_.queueFamilyIndexAndCount();

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        std::vector<std::vector<float>> prioritiesForAllFamilies(familyIndices.size());
        size_t index = 0;
        for (const auto &[familyIndex, queueCount]: familyIndices) {
            // 对这个队列族，创建一个长度为 queueCount 的 float 数组，所有值都设为 1.0f，表示 最高优先级。
            prioritiesForAllFamilies[index] = std::vector<float>(queueCount, 1.0f);
            queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .queueFamilyIndex = familyIndex,
                    .queueCount = queueCount,
                    .pQueuePriorities = prioritiesForAllFamilies[index].data()
            });
            ++index;
        }

        // todo VulkanFeatureChain

        const VkDeviceCreateInfo deviceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos = queueCreateInfos.data(),
                .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
                .ppEnabledLayerNames = instanceLayers.data(),
                .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                .ppEnabledExtensionNames = deviceExtensions.data(),
        };

        VK_CHECK(vkCreateDevice(physicalDevice_.vkPhysicalDevice(), &deviceCreateInfo, nullptr,
                                &device_))
    }

    if (physicalDevice_.graphicsFamilyIndex().has_value()) {
        if (physicalDevice_.graphicsFamilyCount() > 0) {
            graphicsQueues_.resize(physicalDevice_.graphicsFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < graphicsQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.graphicsFamilyIndex().value(),
                                 uint32_t(i), &graphicsQueues_[i]);
            }
        }
    }
    if (physicalDevice_.computeFamilyIndex().has_value()) {
        if (physicalDevice_.computeFamilyCount() > 0) {
            computeQueues_.resize(physicalDevice_.computeFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < computeQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.computeFamilyIndex().value(),
                                 uint32_t(i), &computeQueues_[i]);
            }
        }
    }
    if (physicalDevice_.transferFamilyIndex().has_value()) {
        if (physicalDevice_.transferFamilyCount() > 0) {
            transferQueues_.resize(physicalDevice_.transferFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < transferQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.transferFamilyIndex().value(),
                                 uint32_t(i), &transferQueues_[i]);
            }
        }
    }
    if (physicalDevice_.sparseFamilyIndex().has_value()) {
        if (physicalDevice_.sparseFamilyCount() > 0) {
            sparseQueues_.resize(physicalDevice_.sparseFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < sparseQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.sparseFamilyIndex().value(),
                                 uint32_t(i), &sparseQueues_[i]);
            }
        }
    }

    if (physicalDevice_.presentationFamilyIndex().has_value()) {
        vkGetDeviceQueue(device_, physicalDevice_.presentationFamilyIndex().value(), 0,
                         &presentationQueue_);
    }

    // Initialize volk for this device
    volkLoadDevice(device_);

    // Create the allocator
    createMemoryAllocator();
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

    (void) deviceExtensions;
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

void VkCore::Context::createVkDevice(VkPhysicalDevice vkPhysicalDevice,
                                     const std::vector<std::string> &requestedDeviceExtensions,
                                     VkQueueFlags requestedQueueTypes, const std::string &name) {
    physicalDevice_ = PhysicalDevice(vkPhysicalDevice, VK_NULL_HANDLE, requestedDeviceExtensions, printEnumerations_,
                                     false);

    physicalDevice_.reserveQueues(requestedQueueTypes | VK_QUEUE_GRAPHICS_BIT, VK_NULL_HANDLE);

    {
        // create VkDevice
        std::vector<const char *> deviceExtensions(physicalDevice_.enabledExtensions().size());
        std::transform(physicalDevice_.enabledExtensions().begin(),
                       physicalDevice_.enabledExtensions().end(), deviceExtensions.begin(),
                       std::mem_fn(&std::string::c_str));

        const auto familyIndices = physicalDevice_.queueFamilyIndexAndCount();

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        std::vector<std::vector<float>> prioritiesForAllFamilies(familyIndices.size());
        size_t index = 0;
        for (const auto &[familyIndex, queueCount]: familyIndices) {
            // 对这个队列族，创建一个长度为 queueCount 的 float 数组，所有值都设为 1.0f，表示 最高优先级。
            prioritiesForAllFamilies[index] = std::vector<float>(queueCount, 1.0f);
            queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .queueFamilyIndex = familyIndex,
                    .queueCount = queueCount,
                    .pQueuePriorities = prioritiesForAllFamilies[index].data()
            });
            ++index;
        }

        // todo VulkanFeatureChain

        std::vector<const char*> instanceLayers(enabledLayers_.size());
        std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                       std::mem_fn(&std::string::c_str));

        const VkDeviceCreateInfo deviceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos = queueCreateInfos.data(),
                .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
                .ppEnabledLayerNames = instanceLayers.data(),
                .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                .ppEnabledExtensionNames = deviceExtensions.data(),
        };

        VK_CHECK(vkCreateDevice(physicalDevice_.vkPhysicalDevice(), &deviceCreateInfo, nullptr,
                                &device_))
    }

    if (physicalDevice_.graphicsFamilyIndex().has_value()) {
        if (physicalDevice_.graphicsFamilyCount() > 0) {
            graphicsQueues_.resize(physicalDevice_.graphicsFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < graphicsQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.graphicsFamilyIndex().value(),
                                 uint32_t(i), &graphicsQueues_[i]);
            }
        }
    }
    if (physicalDevice_.computeFamilyIndex().has_value()) {
        if (physicalDevice_.computeFamilyCount() > 0) {
            computeQueues_.resize(physicalDevice_.computeFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < computeQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.computeFamilyIndex().value(),
                                 uint32_t(i), &computeQueues_[i]);
            }
        }
    }
    if (physicalDevice_.transferFamilyIndex().has_value()) {
        if (physicalDevice_.transferFamilyCount() > 0) {
            transferQueues_.resize(physicalDevice_.transferFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < transferQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.transferFamilyIndex().value(),
                                 uint32_t(i), &transferQueues_[i]);
            }
        }
    }
    if (physicalDevice_.sparseFamilyIndex().has_value()) {
        if (physicalDevice_.sparseFamilyCount() > 0) {
            sparseQueues_.resize(physicalDevice_.sparseFamilyCount(), VK_NULL_HANDLE);

            for (int i = 0; i < sparseQueues_.size(); ++i) {
                vkGetDeviceQueue(device_, physicalDevice_.sparseFamilyIndex().value(),
                                 uint32_t(i), &sparseQueues_[i]);
            }
        }
    }

    if (physicalDevice_.presentationFamilyIndex().has_value()) {
        vkGetDeviceQueue(device_, physicalDevice_.presentationFamilyIndex().value(), 0,
                         &presentationQueue_);
    }

    // Initialize volk for this device
    volkLoadDevice(device_);

    // Create the allocator
    createMemoryAllocator();
}

VkCore::Context::~Context() {
    vkDeviceWaitIdle(device_);

//    swapchain_.reset();
    vmaDestroyAllocator(allocator_);
    vkDestroyDevice(device_, nullptr);
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
#if defined(VK_EXT_debug_utils)
    if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != enabledInstanceExtensions_.end()) {
        vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);
    }
#endif

    vkDestroyInstance(instance_, nullptr);
}

void VkCore::Context::createMemoryAllocator() {
    const VmaVulkanFunctions vulkanFunctions = {
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory = vkAllocateMemory,
            .vkFreeMemory = vkFreeMemory,
            .vkMapMemory = vkMapMemory,
            .vkUnmapMemory = vkUnmapMemory,
            .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory = vkBindBufferMemory,
            .vkBindImageMemory = vkBindImageMemory,
            .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
            .vkCreateBuffer = vkCreateBuffer,
            .vkDestroyBuffer = vkDestroyBuffer,
            .vkCreateImage = vkCreateImage,
            .vkDestroyImage = vkDestroyImage,
            .vkCmdCopyBuffer = vkCmdCopyBuffer,
#if VMA_VULKAN_VERSION >= 1001000
            .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
    .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
    .vkBindBufferMemory2KHR = vkBindBufferMemory2,
    .vkBindImageMemory2KHR = vkBindImageMemory2,
    .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
#endif
#if VMA_VULKAN_VERSION >= 1003000
            .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
    .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
#endif
    };

    const VmaAllocatorCreateInfo allocInfo = {
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
#endif
            .physicalDevice = physicalDevice_.vkPhysicalDevice(),
            .device = device_,
            .pVulkanFunctions = &vulkanFunctions,
            .instance = instance_,
            .vulkanApiVersion = applicationInfo_.apiVersion,
    };
    vmaCreateAllocator(&allocInfo, &allocator_);
}

void VkCore::Context::dumpMemoryStats(const std::string &fileName) const {
    char* memoryStats{nullptr};
    ASSERT(allocator_, "Allocator must be initialized");
    vmaBuildStatsString(allocator_, &memoryStats, true);

    std::ofstream out(fileName);
    out << std::string(memoryStats);
    out.close();

    vmaFreeStatsString(allocator_, memoryStats);
}
