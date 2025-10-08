/*
 * Vulkan Context 实现文件
 * 
 * 什么是 Context？
 * Context 是整个 Vulkan 应用程序的核心管理类，它负责：
 * 1. 初始化 Vulkan 实例（Instance）- 相当于 Vulkan 的"会话"
 * 2. 选择和创建物理设备（Physical Device）- 你的显卡
 * 3. 创建逻辑设备（Logical Device）- 与显卡通信的接口
 * 4. 管理队列（Queue）- 向 GPU 提交命令的通道
 * 5. 创建各种 Vulkan 资源（缓冲区、纹理、管线等）
 */

#define VOLK_IMPLEMENTATION  // Volk 是一个 Vulkan 函数加载器，帮助动态加载 Vulkan 函数
#include "Context.h"

#include <algorithm>
#include <array>
#include <csignal>    // 添加信号处理头文件，用于 raise() 函数
#include <fstream>
#include <functional>
#include <map>

// 引入相关的 Vulkan 对象类
#include "FrameBuffer.h"  // 帧缓冲区 - 渲染的目标画布
#include "RenderPass.h"   // 渲染通道 - 定义渲染步骤
#include "Sampler.h"      // 采样器 - 控制纹理采样方式
#include "Texture.h"      // 纹理 - GPU 上的图像数据

// 调试开关：是否启用着色器打印调试功能
constexpr bool DEBUG_SHADER_PRINTF_CALLBACK = false;

namespace {
#if defined(VK_EXT_debug_utils)
/*
 * Vulkan 调试消息回调函数
 * 
 * 什么是调试回调？
 * 当 Vulkan 验证层检测到错误、警告或其他信息时，会调用这个函数
 * 这对于开发时发现问题非常有用
 * 
 * 参数说明：
 * - messageSeverity: 消息严重程度（错误、警告、信息等）
 * - messageTypes: 消息类型（一般、验证、性能等）
 * - pCallbackData: 包含具体错误信息的结构体
 * - pUserData: 用户自定义数据（这里未使用）
 */
VkBool32 VKAPI_PTR debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  
  // 如果是错误级别的消息
  if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
    // 输出错误日志
    LOGE("debugMessengerCallback : MessageCode is %s & Message is %s",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
#if defined(_WIN32)
    __debugbreak();  // Windows 下触发调试器断点
#else
    raise(SIGTRAP);   // 其他平台下发送调试信号
#endif
  } 
  // 如果是警告级别的消息
  else if (messageSeverity & (~VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
    LOGW("debugMessengerCallback : MessageCode is %s & Message is %s",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  } 
  // 如果是信息级别的消息
  else {
    LOGI("debugMessengerCallback : MessageCode is %s & Message is %s",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  }

  // 返回 VK_FALSE 表示不中断 Vulkan 调用
  return VK_FALSE;
}
#endif
}  // namespace

namespace VkCore {

/*
 * ========== Context 类的静态成员变量定义 ==========
 * 
 * 以下变量定义了我们希望启用的 Vulkan 设备功能特性
 * 这些特性需要在创建逻辑设备时指定，GPU 必须支持这些功能才能使用
 */

// Vulkan 1.0 基础设备功能特性
VkPhysicalDeviceFeatures Context::physicalDeviceFeatures_ = {
    .independentBlend = VK_TRUE,                    // 独立混合：允许每个颜色附件使用不同的混合设置
    .vertexPipelineStoresAndAtomics = VK_TRUE,      // 顶点着色器中的存储和原子操作
    .fragmentStoresAndAtomics = VK_TRUE,            // 片段着色器中的存储和原子操作
};

// Vulkan 1.1 功能特性（初始化为默认值，具体功能在需要时启用）
VkPhysicalDeviceVulkan11Features Context::enable11Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
};

// Vulkan 1.2 功能特性（包含描述符索引、标量布局等高级功能）
VkPhysicalDeviceVulkan12Features Context::enable12Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
};

// Vulkan 1.3 功能特性（包含动态渲染、同步2.0等最新功能）
VkPhysicalDeviceVulkan13Features Context::enable13Features_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
};

// 光线追踪加速结构功能（用于构建 BVH 等加速数据结构）
VkPhysicalDeviceAccelerationStructureFeaturesKHR Context::accelStructFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
};

// 光线追踪管线功能（用于创建光线追踪着色器管线）
VkPhysicalDeviceRayTracingPipelineFeaturesKHR Context::rayTracingPipelineFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
};

// 光线查询功能（允许在传统着色器中进行光线查询）
VkPhysicalDeviceRayQueryFeaturesKHR Context::rayQueryFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
};

// 多视图渲染功能（VR/AR 应用中一次渲染多个视角）
VkPhysicalDeviceMultiviewFeatures Context::multiviewFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
};

// 片段密度图功能（可变分辨率着色，用于性能优化）
VkPhysicalDeviceFragmentDensityMapFeaturesEXT Context::fragmentDensityMapFeatures_ = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
};

// 高通平台特有的片段密度图偏移功能
VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM
    Context::fragmentDensityMapOffsetFeatures_ = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
};

// 全局标志：是否启用多视图功能
bool Context::enableMultiViewFlag_ = false;

/*
 * ========== Context 主构造函数 ==========
 * 
 * 这是 Context 类的主要构造函数，完成整个 Vulkan 环境的初始化
 * 
 * 参数说明：
 * @param window - 窗口句柄（用于创建渲染表面）
 * @param requestedLayers - 请求的验证层列表（用于调试）
 * @param requestedInstanceExtensions - 请求的实例扩展列表
 * @param requestedDeviceExtensions - 请求的设备扩展列表
 * @param requestedQueueTypes - 请求的队列类型（图形、计算、传输等）
 * @param printEnumerations - 是否打印枚举信息（调试用）
 * @param enableRayTracing - 是否启用光线追踪
 * @param name - Context 的名称（用于调试标记）
 * 
 * 初始化流程：
 * 1. 初始化 Volk（函数加载器）
 * 2. 筛选可用的层和扩展
 * 3. 创建 Vulkan 实例
 * 4. 设置调试回调
 * 5. 创建窗口表面
 * 6. 选择物理设备
 * 7. 创建逻辑设备
 * 8. 获取队列句柄
 * 9. 创建内存分配器
 */
Context::Context(void* window, const std::vector<std::string>& requestedLayers,
                 const std::vector<std::string>& requestedInstanceExtensions,
                 const std::vector<std::string>& requestedDeviceExtensions,
                 VkQueueFlags requestedQueueTypes, bool printEnumerations,
                 bool enableRayTracing, const std::string& name)
    : printEnumerations_{printEnumerations} {
  
  // 第1步：初始化 Volk 函数加载器
  // Volk 负责动态加载 Vulkan 函数，避免静态链接
  VK_CHECK(volkInitialize());

  // 第2步：筛选可用的层和扩展
  // 从系统可用的层中筛选出我们需要且被支持的层
  enabledLayers_ = util::filterExtensions(enumerateInstanceLayers(), requestedLayers);
  // 从系统可用的扩展中筛选出我们需要且被支持的扩展
  enabledInstanceExtensions_ =
      util::filterExtensions(enumerateInstanceExtensions(), requestedInstanceExtensions);

  // 将启用的层名称从 std::string 转换为 const char*（C 字符串）
  // 这是因为 Vulkan C API 需要 C 字符串数组
  std::vector<const char*> instanceLayers(enabledLayers_.size());
  std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                 std::mem_fn(&std::string::c_str));

  {
    // 第3步：准备创建 Vulkan 实例
    // 同样将扩展名称从 std::string 转换为 const char*
    std::vector<const char*> instanceExtensions(enabledInstanceExtensions_.size());
    std::transform(enabledInstanceExtensions_.begin(), enabledInstanceExtensions_.end(),
                   instanceExtensions.begin(), std::mem_fn(&std::string::c_str));

    // 准备验证功能列表（用于调试和开发）
    std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnabled;
#if defined(VK_EXT_layer_settings)
    // 根据调试设置选择验证功能
    if constexpr (!DEBUG_SHADER_PRINTF_CALLBACK) {
      // 启用 GPU 辅助验证：让 GPU 帮助检测错误，性能影响较小
      validationFeaturesEnabled.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
    } else {
      // 启用着色器打印调试：允许在着色器中使用 debugPrintfEXT() 函数
      validationFeaturesEnabled.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
    }
#endif

#if defined(VK_EXT_layer_settings)
    // 配置 Khronos 验证层的详细设置
    const std::string layer_name = "VK_LAYER_KHRONOS_validation";
    
    // 调试动作设置：遇到错误时中断程序（方便调试）
    const std::array<const char*, 1> setting_debug_action = {"VK_DBG_LAYER_ACTION_BREAK"};
    
    // GPU 调试模式设置：使用 GPU 辅助的调试打印
    const std::array<const char*, 1> setting_gpu_based_action = {
        "GPU_BASED_DEBUG_PRINTF"};
    
    // 打印到标准输出：将调试信息输出到控制台
    const std::array<VkBool32, 1> setting_printf_to_stdout = {VK_TRUE};
    
    // 详细打印模式：输出更详细的调试信息
    const std::array<VkBool32, 1> setting_printf_verbose = {VK_TRUE};
    // 创建验证层设置数组，包含4个具体的配置项
    const std::array<VkLayerSettingEXT, 4> settings = {
        // 设置1：调试动作 - 当检测到错误时如何响应
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),           // 指定层名称
            .pSettingName = "debug_action",             // 设置名称
            .type = VK_LAYER_SETTING_TYPE_STRING_EXT,   // 设置类型为字符串
            .valueCount = 1,                            // 值的数量
            .pValues = setting_debug_action.data(),     // 指向值的指针
        },
        // 设置2：GPU 基础验证 - 启用 GPU 辅助调试
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),
            .pSettingName = "validate_gpu_based",       // GPU 验证设置
            .type = VK_LAYER_SETTING_TYPE_STRING_EXT,
            .valueCount = 1,
            .pValues = setting_gpu_based_action.data(),
        },
        // 设置3：标准输出打印 - 是否输出到控制台
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),
            .pSettingName = "printf_to_stdout",         // 打印到标准输出
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,   // 布尔类型设置
            .valueCount = 1,
            .pValues = setting_printf_to_stdout.data(),
        },
        // 设置4：详细打印 - 是否输出详细信息
        VkLayerSettingEXT{
            .pLayerName = layer_name.c_str(),
            .pSettingName = "printf_verbose",           // 详细打印模式
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
            .valueCount = 1,
            .pValues = setting_printf_verbose.data(),
        },
    };

    // 创建层设置信息结构体，用于配置验证层的具体行为
    const VkLayerSettingsCreateInfoEXT layer_settings_create_info = {
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,  // 结构体类型
        .pNext = nullptr,                                           // 链表中的下一个结构体
        .settingCount = static_cast<uint32_t>(settings.size()),     // 设置项数量
        .pSettings = settings.data(),                               // 指向设置数组
    };
#endif

    // 创建验证功能结构体，定义启用哪些验证功能
    const VkValidationFeaturesEXT features = {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,           // 结构体类型
#if defined(VK_EXT_layer_settings)
      // 根据调试模式决定是否链接层设置
      .pNext = DEBUG_SHADER_PRINTF_CALLBACK ? &layer_settings_create_info : nullptr,
#endif
      .enabledValidationFeatureCount =                             // 启用的验证功能数量
          static_cast<uint32_t>(validationFeaturesEnabled.size()),
      .pEnabledValidationFeatures = validationFeaturesEnabled.data(), // 指向功能数组
    };

    // 第4步：创建 Vulkan 实例
    // 实例是整个 Vulkan 应用程序的根对象，管理全局状态
    const VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,            // 结构体类型
        .pNext = &features,                                         // 链接验证功能设置
        .pApplicationInfo = &applicationInfo_,                      // 应用程序信息
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),     // 启用的层数量
        .ppEnabledLayerNames = instanceLayers.data(),               // 层名称数组
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()), // 启用的扩展数量
        .ppEnabledExtensionNames = instanceExtensions.data(),       // 扩展名称数组
    };
    // 调用 Vulkan API 创建实例，VK_CHECK 宏用于检查返回值
    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance_));
  }

  // 第5步：为创建的实例初始化 Volk
  // 现在 Volk 可以加载这个特定实例的函数指针了
  volkLoadInstance(instance_);

#if defined(VK_EXT_debug_utils)
  // 第6步：设置调试消息器（如果启用了调试工具扩展）
  if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != enabledInstanceExtensions_.end()) {
    // 配置调试消息器的创建信息
    const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,  // 结构体类型
      .flags = 0,                                                       // 标志位（默认为0）
      
      // 设置要接收的消息严重程度（所有级别的消息）
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |  // 详细信息
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |     // 一般信息
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |  // 警告
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,     // 错误
      
      // 设置要接收的消息类型
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |      // 一般消息
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |   // 验证层消息
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT    // 性能提示
#if defined(VK_EXT_device_address_binding_report)
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT  // 设备地址绑定报告
#endif
      ,
      .pfnUserCallback = &debugMessengerCallback,  // 回调函数指针
      .pUserData = nullptr,                        // 用户数据（这里不需要）
    };
    // 创建调试消息器对象
    VK_CHECK(
        vkCreateDebugUtilsMessengerEXT(instance_, &messengerInfo, nullptr, &messenger_));
  }
#endif

// 第7步：创建渲染表面（Surface）
// 表面是 Vulkan 与操作系统窗口系统的连接桥梁，用于显示渲染结果
#if defined(VK_USE_PLATFORM_WIN32_KHR) && defined(VK_KHR_win32_surface)
  // 如果在 Windows 平台且启用了 Win32 表面扩展
  if (enabledInstanceExtensions_.find(VK_KHR_WIN32_SURFACE_EXTENSION_NAME) != enabledInstanceExtensions_.end()) {
    if (window != nullptr) {
      // 配置 Windows 表面创建信息
      const VkWin32SurfaceCreateInfoKHR ci = {
          .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // 结构体类型
          .hinstance = GetModuleHandle(NULL),                       // 应用程序实例句柄
          .hwnd = (HWND)window,                                     // 窗口句柄
      };
      // 创建 Windows 平台的 Vulkan 表面
      VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_));
    }
  }
#endif

  // 第8步：选择物理设备（显卡）
  // 枚举系统中所有支持 Vulkan 的显卡，然后选择最合适的一个
  physicalDevice_ = choosePhysicalDevice(
      enumeratePhysicalDevices(requestedDeviceExtensions, enableRayTracing),  // 获取所有物理设备
      requestedDeviceExtensions);  // 传入请求的设备扩展

  // 第9步：为物理设备预留队列
  // 总是请求图形队列，因为我们需要进行渲染操作
  // 同时根据用户需求添加其他类型的队列（计算、传输等）
  physicalDevice_.reserveQueues(requestedQueueTypes | VK_QUEUE_GRAPHICS_BIT, surface_);

  // 第10步：创建逻辑设备（Logical Device）
  // 逻辑设备是我们与物理设备通信的接口，定义了我们要使用的功能和队列
  {
    // 将设备扩展名称从 std::string 转换为 const char*
    std::vector<const char*> deviceExtensions(physicalDevice_.enabledExtensions().size());
    std::transform(physicalDevice_.enabledExtensions().begin(),
                   physicalDevice_.enabledExtensions().end(), deviceExtensions.begin(),
                   std::mem_fn(&std::string::c_str));

    const auto familyIndices = physicalDevice_.queueFamilyIndexAndCount();

    // 准备队列创建信息列表
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    // 为每个队列族创建优先级数组（优先级范围 0.0-1.0）
    std::vector<std::vector<float>> prioritiesForAllFamilies(familyIndices.size());
    size_t index = 0;  // 将索引初始化移到循环外，兼容 C++17
    for (const auto& [queueFamilyIndex, queueCount] : familyIndices) {
      // 为每个队列设置相同的优先级 1.0（最高优先级）
      prioritiesForAllFamilies[index] = std::vector<float>(queueCount, 1.0f);
      
      // 创建队列创建信息
      queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,    // 结构体类型
          .queueFamilyIndex = queueFamilyIndex,                   // 队列族索引
          .queueCount = queueCount,                               // 该族中要创建的队列数量
          .pQueuePriorities = prioritiesForAllFamilies[index].data(), // 队列优先级数组
      });
      ++index;
    }

    // 创建设备功能结构体，指定我们要启用的基础功能
    const VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,  // 结构体类型
        .features = physicalDeviceFeatures_,                   // 基础 Vulkan 1.0 功能
    };

    // 创建功能链，用于管理多个版本的 Vulkan 功能
    // 这是一种链表结构，通过 pNext 指针连接不同版本的功能结构体
    VulkanFeatureChain<> featureChain;

    // 将基础功能添加到链中
    featureChain.pushBack(deviceFeatures);

    // 添加不同版本的 Vulkan 功能到链中
    featureChain.pushBack(enable11Features_);  // Vulkan 1.1 功能
    featureChain.pushBack(enable12Features_);  // Vulkan 1.2 功能
    featureChain.pushBack(enable13Features_);  // Vulkan 1.3 功能

    if (physicalDevice_.isRayTracingSupported()) {
      featureChain.pushBack(accelStructFeatures_);
      featureChain.pushBack(rayTracingPipelineFeatures_);
      featureChain.pushBack(rayQueryFeatures_);
    }

    if (physicalDevice_.isMultiviewSupported() && enableMultiViewFlag_) {
      enable11Features_.multiview = VK_TRUE;
    }

    if (physicalDevice_.isFragmentDensityMapSupported()) {
      featureChain.pushBack(fragmentDensityMapFeatures_);
    }

    if (physicalDevice_.isFragmentDensityMapOffsetSupported()) {
      featureChain.pushBack(fragmentDensityMapOffsetFeatures_);
    }

    const VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = featureChain.firstNextPtr(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    VK_CHECK(vkCreateDevice(physicalDevice_.vkPhysicalDevice(), &dci, nullptr, &device_));
    setVkObjectname(device_, VK_OBJECT_TYPE_DEVICE, "Device");
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

  // 第11步：为创建的设备初始化 Volk
  // 现在可以加载设备特定的 Vulkan 函数了
  volkLoadDevice(device_);

  // 第12步：创建内存分配器
  // VMA（Vulkan Memory Allocator）用于简化 GPU 内存管理
  createMemoryAllocator();

  // 第13步：为之前创建的对象设置调试名称
  // 这有助于在调试工具中识别不同的 Vulkan 对象
  setVkObjectname(surface_, VK_OBJECT_TYPE_SURFACE_KHR, "Surface: " + name);
}  // Context 主构造函数结束

/*
 * ========== Context 简化构造函数 ==========
 * 
 * 这是一个简化版本的构造函数，用于不需要窗口表面的场景
 * 例如：计算着色器应用、离屏渲染、服务器端渲染等
 * 
 * 参数说明：
 * @param appInfo - 应用程序信息结构体
 * @param requestedLayers - 请求的验证层列表
 * @param requestedInstanceExtensions - 请求的实例扩展列表
 * @param printEnumerations - 是否打印枚举信息
 * @param name - Context 的名称
 * 
 * 与主构造函数的区别：
 * - 不创建窗口表面
 * - 不需要窗口句柄
 * - 更简单的验证层设置
 */
Context::Context(const VkApplicationInfo& appInfo,
                 const std::vector<std::string>& requestedLayers,
                 const std::vector<std::string>& requestedInstanceExtensions,
                 bool printEnumerations, const std::string& name)
    : applicationInfo_{appInfo}, printEnumerations_{printEnumerations} {
  VK_CHECK(volkInitialize());

  enabledLayers_ = util::filterExtensions(enumerateInstanceLayers(), requestedLayers);
  enabledInstanceExtensions_ =
      util::filterExtensions(enumerateInstanceExtensions(), requestedInstanceExtensions);

  // Transform list of enabled Instance layers from std::string to const char*
  std::vector<const char*> instanceLayers(enabledLayers_.size());
  std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                 std::mem_fn(&std::string::c_str));

  {
    // Transform list of enabled extensions from std::string to const char*
    std::vector<const char*> instanceExtensions(enabledInstanceExtensions_.size());
    std::transform(enabledInstanceExtensions_.begin(), enabledInstanceExtensions_.end(),
                   instanceExtensions.begin(), std::mem_fn(&std::string::c_str));

    // Create the instance
    const VkValidationFeatureEnableEXT validationFeaturesEnabled[] = {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
    };  // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

    const VkValidationFeaturesEXT features = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .pNext = nullptr,
        .enabledValidationFeatureCount =
            sizeof(validationFeaturesEnabled) / sizeof(VkValidationFeatureEnableEXT),
        .pEnabledValidationFeatures = validationFeaturesEnabled,
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

    ASSERT(instance_ != VK_NULL_HANDLE, "Error creating VkInstance");
  }

  // Initialize volk for this instance
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

  if (device_) {
    setVkObjectname(instance_, VK_OBJECT_TYPE_INSTANCE, "Instance: " + name);
  }
}

void Context::createVkDevice(VkPhysicalDevice vkPhysicalDevice,
                             const std::vector<std::string>& requestedDeviceExtensions,
                             VkQueueFlags requestedQueueTypes, const std::string& name) {
  physicalDevice_ = PhysicalDevice(vkPhysicalDevice, VK_NULL_HANDLE,
                                   requestedDeviceExtensions, printEnumerations_, false);

  // Always request a graphics queue
  physicalDevice_.reserveQueues(requestedQueueTypes | VK_QUEUE_GRAPHICS_BIT,
                                VK_NULL_HANDLE);

  // Create the device
  {
    // Transform list of enabled extensions from std::string to const char *
    std::vector<const char*> deviceExtensions(physicalDevice_.enabledExtensions().size());
    std::transform(physicalDevice_.enabledExtensions().begin(),
                   physicalDevice_.enabledExtensions().end(), deviceExtensions.begin(),
                   std::mem_fn(&std::string::c_str));

    // 获取物理设备支持的队列族信息（索引和数量）
    const auto familyIndices = physicalDevice_.queueFamilyIndexAndCount();

    // 准备队列创建信息列表
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    // 为每个队列族创建优先级数组（优先级范围 0.0-1.0）
    std::vector<std::vector<float>> prioritiesForAllFamilies(familyIndices.size());
    size_t index = 0;  // 将索引初始化移到循环外，兼容 C++17
    for (const auto& [queueFamilyIndex, queueCount] : familyIndices) {
      // 为每个队列设置相同的优先级 1.0（最高优先级）
      prioritiesForAllFamilies[index] = std::vector<float>(queueCount, 1.0f);
      
      // 创建队列创建信息
      queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,    // 结构体类型
          .queueFamilyIndex = queueFamilyIndex,                   // 队列族索引
          .queueCount = queueCount,                               // 该族中要创建的队列数量
          .pQueuePriorities = prioritiesForAllFamilies[index].data(), // 队列优先级数组
      });
      ++index;
    }

    VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = physicalDeviceFeatures_,
    };

    VulkanFeatureChain<> featureChain;

    featureChain.pushBack(deviceFeatures);

    featureChain.pushBack(enable11Features_);
    featureChain.pushBack(enable12Features_);
#if defined(_WIN32)
    featureChain.pushBack(enable13Features_);
#else
    // this should be only done if we are on vulkan 1.1 & buffer address is
    // enabled
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddrFeature = {};
    bufferDeviceAddrFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddrFeature.bufferDeviceAddress = VK_TRUE;
    bufferDeviceAddrFeature.bufferDeviceAddressCaptureReplay = VK_TRUE;
    deviceFeatures.pNext = &bufferDeviceAddrFeature;
#endif

    if (physicalDevice_.isRayTracingSupported()) {
      featureChain.pushBack(accelStructFeatures_);
      featureChain.pushBack(rayTracingPipelineFeatures_);
      featureChain.pushBack(rayQueryFeatures_);
    }

    if (physicalDevice_.isMultiviewSupported() && enableMultiViewFlag_) {
      enable11Features_.multiview = VK_TRUE;
    }

    if (physicalDevice_.isFragmentDensityMapSupported()) {
      featureChain.pushBack(fragmentDensityMapFeatures_);
    }

    if (physicalDevice_.isFragmentDensityMapOffsetSupported()) {
      featureChain.pushBack(fragmentDensityMapOffsetFeatures_);
    }

    std::vector<const char*> instanceLayers(enabledLayers_.size());
    std::transform(enabledLayers_.begin(), enabledLayers_.end(), instanceLayers.begin(),
                   std::mem_fn(&std::string::c_str));

    const VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = featureChain.firstNextPtr(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    VK_CHECK(vkCreateDevice(physicalDevice_.vkPhysicalDevice(), &dci, nullptr, &device_));
    setVkObjectname(device_, VK_OBJECT_TYPE_DEVICE, "Device");
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

  setVkObjectname(device_, VK_OBJECT_TYPE_DEVICE, "Device: " + name);

  setVkObjectname(instance_, VK_OBJECT_TYPE_INSTANCE, "Instance: " + name);
}

/*
 * ========== Context 析构函数 ==========
 * 
 * 负责清理所有 Vulkan 资源，确保没有内存泄漏
 * 销毁顺序很重要：必须先销毁依赖的对象，最后销毁基础对象
 */
Context::~Context() {
  // 等待设备完成所有操作，确保没有正在进行的工作
  vkDeviceWaitIdle(device_);

  // 销毁交换链（如果存在）
  swapchain_.reset();
  
  // 销毁内存分配器
  vmaDestroyAllocator(allocator_);
  
  // 销毁逻辑设备
  vkDestroyDevice(device_, nullptr);
  
  // 销毁表面（如果存在）
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  }
  
  // 销毁调试消息器（如果启用了调试扩展）
#if defined(VK_EXT_debug_utils)
  if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != enabledInstanceExtensions_.end()) {
    vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);
  }
#endif

  // 最后销毁 Vulkan 实例
  vkDestroyInstance(instance_, nullptr);
}

/*
 * ========== 功能启用方法 ==========
 * 
 * 以下方法用于启用特定的 Vulkan 功能特性
 * 这些方法必须在创建逻辑设备之前调用
 */

/*
 * 启用默认推荐的功能特性
 * 主要包含描述符索引相关功能，这些功能对现代渲染管线很有用
 */
void Context::enableDefaultFeatures() {
  // 启用着色器中非均匀索引功能
  enable12Features_.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;  // 采样图像数组非均匀索引
  enable12Features_.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;  // 存储图像数组非均匀索引
  
  // 注意：以下功能在某些旧显卡（如 GTX 1060）上可能导致设备创建失败
  // enable12Features_.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
  
  // 启用描述符绑定相关功能，允许更灵活的资源管理
  enable12Features_.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;    // 绑定后更新采样图像
  enable12Features_.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;   // 绑定后更新存储缓冲区
  enable12Features_.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;       // 挂起时更新未使用的描述符
  enable12Features_.descriptorBindingPartiallyBound = VK_TRUE;                 // 部分绑定描述符
  enable12Features_.descriptorBindingVariableDescriptorCount = VK_TRUE;        // 可变描述符数量
  enable12Features_.descriptorIndexing = VK_TRUE;                             // 描述符索引
  enable12Features_.runtimeDescriptorArray = VK_TRUE;                         // 运行时描述符数组
}

/*
 * 启用标量块布局功能
 * 允许着色器使用更紧凑的内存布局，类似于 C 语言的结构体布局
 */
void Context::enableScalarLayoutFeatures() {
  enable12Features_.scalarBlockLayout = VK_TRUE;
}

/*
 * 启用动态渲染功能（Vulkan 1.3）
 * 允许不使用传统的渲染通道，直接开始渲染，简化渲染管线
 */
void Context::enableDynamicRenderingFeature() {
  enable13Features_.dynamicRendering = VK_TRUE;
}

/*
 * 启用缓冲区设备地址功能
 * 允许着色器直接访问 GPU 内存地址，用于更高级的内存管理
 */
void Context::enableBufferDeviceAddressFeature() {
  enable12Features_.bufferDeviceAddress = VK_TRUE;              // 基础设备地址功能
  enable12Features_.bufferDeviceAddressCaptureReplay = VK_TRUE; // 地址捕获重放（调试用）
}

/*
 * 启用间接渲染功能
 * 允许 GPU 自主决定渲染参数，减少 CPU-GPU 同步
 */
void Context::enableIndirectRenderingFeature() {
  enable11Features_.shaderDrawParameters = VK_TRUE;        // 着色器绘制参数
  enable12Features_.drawIndirectCount = VK_TRUE;           // 间接绘制计数
  physicalDeviceFeatures_.multiDrawIndirect = VK_TRUE;     // 多重间接绘制
  physicalDeviceFeatures_.drawIndirectFirstInstance = VK_TRUE; // 间接绘制首实例
}

/*
 * 启用 16 位浮点数功能
 * 在保持精度的前提下减少内存使用和提高性能
 */
void Context::enable16bitFloatFeature() {
  enable11Features_.storageBuffer16BitAccess = VK_TRUE; // 存储缓冲区 16 位访问
  enable12Features_.shaderFloat16 = VK_TRUE;            // 着色器 16 位浮点
}

/*
 * 启用独立混合功能
 * 允许每个颜色附件使用不同的混合设置
 */
void Context::enableIndependentBlending() {
  physicalDeviceFeatures_.independentBlend = VK_TRUE;
}

/*
 * 启用维护4功能（Vulkan 1.3）
 * 提供一些小的 API 改进和优化
 */
void Context::enableMaintenance4Feature() { 
  enable13Features_.maintenance4 = VK_TRUE; 
}

/*
 * 启用同步2.0功能（Vulkan 1.3）
 * 提供更简单、更强大的同步原语
 */
void Context::enableSynchronization2Feature() {
  enable13Features_.synchronization2 = VK_TRUE;
}

/*
 * 启用光线追踪功能
 * 支持硬件加速光线追踪渲染
 */
void Context::enableRayTracingFeatures() {
  accelStructFeatures_.accelerationStructure = VK_TRUE;     // 加速结构
  rayTracingPipelineFeatures_.rayTracingPipeline = VK_TRUE; // 光线追踪管线
  rayQueryFeatures_.rayQuery = VK_TRUE;                     // 光线查询
}

/*
 * 启用多视图功能
 * 用于 VR/AR 应用，可以一次渲染多个视角
 */
void Context::enableMultiView() { 
  enableMultiViewFlag_ = true; 
}

/*
 * 检查多视图功能是否已启用
 */
bool Context::isMultiviewEnabled() { 
  return enableMultiViewFlag_; 
}

/*
 * 启用片段密度图功能
 * 可变分辨率着色技术，允许屏幕不同区域使用不同的着色密度
 */
void Context::enableFragmentDensityMapFeatures() {
  fragmentDensityMapFeatures_.fragmentDensityMap = VK_TRUE;
}

/*
 * 启用片段密度图偏移功能（高通平台特有）
 * 提供更精细的片段密度控制
 */
void Context::enableFragmentDensityMapOffsetFeatures() {
  fragmentDensityMapOffsetFeatures_.fragmentDensityMapOffset = VK_TRUE;
}

/*
 * ========== 访问器方法 ==========
 */

/*
 * 获取物理设备引用
 * 用于查询设备能力和属性
 */
const PhysicalDevice& Context::physicalDevice() const { 
  return physicalDevice_; 
}

/*
 * ========== 资源创建方法 ==========
 */

/*
 * 创建交换链
 * 交换链管理屏幕上的图像缓冲区，用于显示渲染结果
 * 
 * @param format - 图像格式（如 RGBA8）
 * @param colorSpace - 颜色空间（如 sRGB）
 * @param presentMode - 显示模式（如垂直同步）
 * @param extent - 交换链图像尺寸
 */
void Context::createSwapchain(VkFormat format, VkColorSpaceKHR colorSpace,
                              VkPresentModeKHR presentMode, const VkExtent2D& extent) {
  // 确保已创建表面，否则无法创建交换链
  ASSERT(surface_ != VK_NULL_HANDLE,
         "You are trying to create a swapchain without a surface. The Context "
         "must be provided a valid surface for it to be able to create a "
         "swapchain");

  // 创建交换链对象
  swapchain_ =
      std::make_unique<Swapchain>(*this, physicalDevice_, surface_, presentationQueue_,
                                  format, colorSpace, presentMode, extent);
}

/*
 * 获取交换链指针
 */
Swapchain* Context::swapchain() const { 
  return swapchain_.get(); 
}

std::shared_ptr<Buffer> Context::createBuffer(size_t size, VkBufferUsageFlags flags,
                                              VmaMemoryUsage memoryUsage,
                                              const std::string& name) const {
  return std::make_shared<Buffer>(
      this, memoryAllocator(),
      VkBufferCreateInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = size,
          .usage = flags,
      },
      VmaAllocationCreateInfo{
          .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
          .usage = memoryUsage,
          .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      },
      name);
}

std::shared_ptr<Buffer> Context::createPersistentBuffer(size_t size,
                                                        VkBufferUsageFlags flags,
                                                        const std::string& name) const {
  const VkMemoryPropertyFlags cpuVisibleMemoryFlags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  return std::make_shared<Buffer>(this, memoryAllocator(),
                                  VkBufferCreateInfo{
                                      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size = size,
                                      .usage = flags,
                                      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  },
                                  VmaAllocationCreateInfo{
                                      .flags = 0,
                                      .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                      .requiredFlags = cpuVisibleMemoryFlags,
                                  },
                                  name);
}

std::shared_ptr<Buffer> Context::createStagingBuffer(VkDeviceSize size,
                                                     VkBufferUsageFlags flags,
                                                     const std::string& name) const {
  const VkBufferCreateInfo stagingBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  };

  const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
               VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_CPU_ONLY,
  };

  return std::make_shared<Buffer>(this, memoryAllocator(), stagingBufferCreateInfo,
                                  stagingAllocationCreateInfo, name);
}

std::shared_ptr<Buffer> Context::createStagingBuffer(VkDeviceSize size,
                                                     VkBufferUsageFlags usage,
                                                     Buffer* actualBuffer,
                                                     const std::string& name) const {
  return std::make_shared<Buffer>(this, memoryAllocator(), size, usage, actualBuffer,
                                  name);
}

// creates staging buffer & upload data to gpubuffer
void Context::uploadToGPUBuffer(VkCore::CommandQueueManager& queueMgr,
                                VkCommandBuffer commandBuffer,
                                VkCore::Buffer* gpuBuffer, const void* data,
                                long totalSize, uint64_t gpuBufferOffset) const {
  auto stagingBuffer = createStagingBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           gpuBuffer, "staging buffer");
  stagingBuffer->copyDataToBuffer(data, totalSize);

  stagingBuffer->uploadStagingBufferToGPU(commandBuffer, 0, gpuBufferOffset);
  queueMgr.disposeWhenSubmitCompletes(std::move(stagingBuffer));
}

std::shared_ptr<Texture> Context::createTexture(
    VkImageType type, VkFormat format, VkImageCreateFlags flags,
    VkImageUsageFlags usageFlags, VkExtent3D extents, uint32_t numMipLevels,
    uint32_t layerCount, VkMemoryPropertyFlags memoryFlags, bool generateMips,
    VkSampleCountFlagBits msaaSamples, const std::string& name) const {
  return std::make_shared<Texture>(*this, type, format, flags, usageFlags, extents,
                                   numMipLevels, layerCount, memoryFlags, generateMips,
                                   msaaSamples, name);
}

std::shared_ptr<Sampler> Context::createSampler(VkFilter minFilter, VkFilter magFilter,
                                                VkSamplerAddressMode addressModeU,
                                                VkSamplerAddressMode addressModeV,
                                                VkSamplerAddressMode addressModeW,
                                                float maxLod,
                                                const std::string& name) const {
  return std::make_shared<Sampler>(*this, minFilter, magFilter, addressModeU,
                                   addressModeV, addressModeW, maxLod, name);
}

std::shared_ptr<VkCore::Sampler> Context::createSampler(
    VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressModeU,
    VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW, float maxLod,
    bool compareEnable, VkCompareOp compareOp, const std::string& name /*= ""*/) const {
  return std::make_shared<Sampler>(*this, minFilter, magFilter, addressModeU,
                                   addressModeV, addressModeW, maxLod, compareEnable,
                                   compareOp, name);
}

CommandQueueManager Context::createGraphicsCommandQueue(uint32_t count,
                                                        uint32_t concurrentNumCommands,
                                                        const std::string& name,
                                                        int graphicsQueueIndex) {
  if (graphicsQueueIndex != -1) {
    ASSERT(graphicsQueueIndex < graphicsQueues_.size(),
           "Don't have enough graphics queue, specify smaller queue index");
  }
  return CommandQueueManager(
      *this, device_, count, concurrentNumCommands,
      physicalDevice_.graphicsFamilyIndex().value(),
      graphicsQueueIndex != -1 ? graphicsQueues_[graphicsQueueIndex] : graphicsQueues_[0],
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, name);
}

VkCore::CommandQueueManager Context::createTransferCommandQueue(
    uint32_t count, uint32_t concurrentNumCommands, const std::string& name,
    int transferQueueIndex) {
  if (transferQueueIndex != -1) {
    ASSERT(transferQueueIndex < transferQueues_.size(),
           "Don't have enough transfer queue, specify smaller queue index");
  }
  return CommandQueueManager(
      *this, device_, count, concurrentNumCommands,
      physicalDevice_.transferFamilyIndex().value(),
      transferQueueIndex != -1 ? transferQueues_[transferQueueIndex] : transferQueues_[0],
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, name);
}

std::shared_ptr<ShaderModule> Context::createShaderModule(const std::string& filePath,
                                                          VkShaderStageFlagBits stages,
                                                          const std::string& name) {
  return std::make_shared<ShaderModule>(this, filePath, stages, name);
}

std::shared_ptr<ShaderModule> Context::createShaderModule(const std::string& filePath,
                                                          const std::string& entryPoint,
                                                          VkShaderStageFlagBits stages,
                                                          const std::string& name) {
  return std::make_shared<ShaderModule>(this, filePath, entryPoint, stages, name);
}

std::shared_ptr<ShaderModule> Context::createShaderModule(const std::vector<char>& shader,
                                                          const std::string& entryPoint,
                                                          VkShaderStageFlagBits stages,
                                                          const std::string& name) {
  return std::make_shared<ShaderModule>(this, shader, entryPoint, stages, name);
}

std::shared_ptr<Pipeline> Context::createGraphicsPipeline(
    const Pipeline::GraphicsPipelineDescriptor& desc, VkRenderPass renderPass,
    const std::string& name) {
  return std::make_shared<Pipeline>(this, desc, renderPass, name);
}

std::shared_ptr<VkCore::Pipeline> Context::createComputePipeline(
    const Pipeline::ComputePipelineDescriptor& desc, const std::string& name /*= ""*/) {
  return std::make_shared<Pipeline>(this, desc, name);
}

std::shared_ptr<VkCore::Pipeline> Context::createRayTracingPipeline(
    const Pipeline::RayTracingPipelineDescriptor& desc,
    const std::string& name /*= ""*/) {
  return std::make_shared<Pipeline>(this, desc, name);
}

std::shared_ptr<RenderPass> Context::createRenderPass(
    const std::vector<std::shared_ptr<Texture>>& attachments,
    const std::vector<VkAttachmentLoadOp>& loadOp,
    const std::vector<VkAttachmentStoreOp>& storeOp,
    const std::vector<VkImageLayout>& layout, VkPipelineBindPoint bindPoint,
    const std::vector<std::shared_ptr<Texture>>& resolveAttachments,
    const std::string& name) const {
  return std::make_shared<RenderPass>(*this, attachments, resolveAttachments, loadOp,
                                      storeOp, layout, bindPoint, name);
}

std::unique_ptr<Framebuffer> Context::createFramebuffer(
    VkRenderPass renderPass,
    const std::vector<std::shared_ptr<Texture>>& colorAttachments,
    std::shared_ptr<Texture> depthAttachment, std::shared_ptr<Texture> stencilAttachment,
    const std::string& name) const {
  return std::make_unique<Framebuffer>(*this, device_, renderPass, colorAttachments,
                                       depthAttachment, stencilAttachment, name);
}

void Context::beginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name,
                                   const glm::vec4& color) const {
#if defined(VK_EXT_debug_utils) && defined(_WIN32)
  VkDebugUtilsLabelEXT utilsLabelInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pLabelName = name.c_str(),
  };
  memcpy(utilsLabelInfo.color, &color[0], sizeof(float) * 4);
  vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &utilsLabelInfo);
#endif
}

void Context::endDebugUtilsLabel(VkCommandBuffer commandBuffer) const {
#if defined(VK_EXT_debug_utils) && defined(_WIN32)
  vkCmdEndDebugUtilsLabelEXT(commandBuffer);
#endif
}

void Context::createMemoryAllocator() {
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

void Context::dumpMemoryStats(const std::string& fileName) const {
  char* memoryStats{nullptr};
  ASSERT(allocator_, "Allocator must be initialized");
  vmaBuildStatsString(allocator_, &memoryStats, true);

  std::ofstream out(fileName);
  out << std::string(memoryStats);
  out.close();

  vmaFreeStatsString(allocator_, memoryStats);
}

std::vector<std::string> Context::enumerateInstanceLayers(bool printEnumerations) {
  uint32_t instanceLayerCount{0};
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
  std::vector<VkLayerProperties> layers(instanceLayerCount);
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, layers.data()));

  std::vector<std::string> returnValues;
  std::transform(
      layers.begin(), layers.end(), std::back_inserter(returnValues),
      [](const VkLayerProperties& properties) { return properties.layerName; });

  if (printEnumerations) {
    std::cerr << "Found " << instanceLayerCount << " available layer(s)" << std::endl;
    for (const auto& layer : returnValues) {
      std::cerr << "\t" << layer << std::endl;
    }
  }

  return returnValues;
}

[[nodiscard]] std::vector<std::string> Context::enumerateInstanceExtensions() {
  uint32_t extensionsCount{0};
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);
  std::vector<VkExtensionProperties> extensionProperties(extensionsCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount,
                                         extensionProperties.data());

  std::vector<std::string> returnValues;
  std::transform(
      extensionProperties.begin(), extensionProperties.end(),
      std::back_inserter(returnValues),
      [](const VkExtensionProperties& properties) { return properties.extensionName; });

  if (printEnumerations_) {
    std::cerr << "Found " << extensionsCount << " extension(s) for the instance"
              << std::endl;
    for (const auto& layer : returnValues) {
      std::cerr << "\t" << layer << std::endl;
    }
  }

  return returnValues;
}

std::vector<PhysicalDevice> Context::enumeratePhysicalDevices(
    const std::vector<std::string>& requestedExtensions, bool enableRayTracing) const {
  uint32_t deviceCount{0};
  VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr));
  ASSERT(deviceCount > 0, "No Vulkan devices found");
  std::vector<VkPhysicalDevice> devices(deviceCount);
  VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()));

  if (printEnumerations_) {
    std::cerr << "Found " << deviceCount << " Vulkan capable device(s)" << std::endl;
  }

  std::vector<PhysicalDevice> physicalDevices;
  for (const auto device : devices) {
    physicalDevices.emplace_back(PhysicalDevice(device, surface_, requestedExtensions,
                                                printEnumerations_, enableRayTracing));
  }
  return physicalDevices;
}

PhysicalDevice Context::choosePhysicalDevice(
    std::vector<PhysicalDevice>&& devices,
    const std::vector<std::string>& deviceExtensions) const {
  (void)deviceExtensions;
  ASSERT(!devices.empty(), "The list of devices can't be empty");

  for (const auto device : devices) {
    std::string devicename(device.properties().properties.deviceName);
    const auto result = devicename.find("NVIDIA");
    if (result != std::string::npos) {
      return device;
    }
  }
  return devices[0];
}

}  // namespace VkCore