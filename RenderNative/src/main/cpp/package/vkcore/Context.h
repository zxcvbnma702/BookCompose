#pragma once

#include <any>              // std::any 用于类型擦除，存储任意类型对象
#include <array>            // std::array 固定大小数组容器
#include "glm/glm/glm.hpp"      // GLM 数学库，用于向量和矩阵运算
#include <memory>           // 智能指针 std::shared_ptr, std::unique_ptr
#include <string>           // std::string 字符串类
#include <unordered_set>    // std::unordered_set 哈希集合
#include <vector>           // std::vector 动态数组

// Vulkan 核心组件头文件
#include "Buffer.h"           // GPU 缓冲区管理
#include "CommandQueueManager.h" // 命令队列管理器
#include "Common.h"          // 通用定义和宏
#include "PhysicalDevice.h"  // 物理设备（GPU）管理
#include "Pipeline.h"        // 渲染管线管理
#include "ShaderModule.h"    // 着色器模块管理
#include "Surface.h"         // 跨平台表面创建功能
#include "SwapChain.h"       // 交换链管理（用于显示）
#include "Utils.h"           // 实用工具函数
#include "vk_mem_alloc.h"    // Vulkan 内存分配器（VMA）

namespace VkCore {

// 前向声明 - 避免头文件循环依赖
class Framebuffer;   // 帧缓冲区：渲染目标的集合
class RenderPass;    // 渲染通道：定义渲染操作的结构
class Sampler;       // 采样器：控制纹理采样方式
class Texture;       // 纹理：存储图像数据

/**
 * Vulkan 功能链管理模板类
 * 
 * 什么是 Vulkan 功能链 (Feature Chain)？
 * 
 * Vulkan 使用一种叫做 "结构体链" 的设计模式来扩展功能。想象这就像一串珠子：
 * - 每个珠子代表一个功能或配置
 * - 珠子通过 pNext 指针连接在一起
 * - GPU 驱动会沿着这条链读取所有需要的功能设置
 * 
 * 为什么需要功能链？
 * 1. 向后兼容性：新功能不会破坏旧代码
 * 2. 模块化设计：每个功能独立配置
 * 3. 扩展性：厂商可以添加自定义功能而不修改核心 API
 * 
 * 功能链的工作原理：
 * - 基础结构体（如 VkDeviceCreateInfo）有一个 pNext 字段
 * - pNext 指向第一个扩展结构体
 * - 每个扩展结构体也有 pNext，指向下一个扩展
 * - 最后一个结构体的 pNext 为 nullptr
 * 
 * 使用场景示例：
 * 1. 启用光线追踪功能
 * 2. 配置多视图渲染（VR/AR）
 * 3. 启用可变速率着色
 * 4. 设置内存优先级
 * 
 * 模板参数：
 * @tparam CHAIN_SIZE 功能链的最大长度，默认为 10 个功能
 */
template <size_t CHAIN_SIZE = 10>
class VulkanFeatureChain {
 public:
  /**
   * 默认构造函数
   * 创建一个空的功能链，准备接收功能配置
   */
  VulkanFeatureChain() = default;
  
  /**
   * 只允许移动构造和移动赋值，禁止拷贝
   * 这是因为功能链包含指针，拷贝会导致指针混乱
   */
  MOVABLE_ONLY(VulkanFeatureChain);

  /**
   * 向功能链中添加新的功能结构体
   * 
   * 这个方法的工作原理：
   * 1. 将新的功能结构体存储到内部数组中
   * 2. 获取存储结构体的引用
   * 3. 将新结构体插入到链的开头（反向链接）
   * 4. 更新链的头指针
   * 
   * 为什么采用反向插入？
   * - 简化链的构建过程
   * - 避免需要遍历整个链来找到末尾
   * - 新功能总是在链的前面，便于调试
   * 
   * @tparam T 要添加的 Vulkan 功能结构体类型
   * @param nextVulkanChainStruct 要添加的 Vulkan 功能结构体
   * @return 返回添加到链中的结构体的引用，便于进一步配置
   * 
   * 使用示例：
   * ```cpp
   * VulkanFeatureChain<> chain;
   * auto& rtFeatures = chain.pushBack(VkPhysicalDeviceRayTracingPipelineFeaturesKHR{
   *     .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
   *     .rayTracingPipeline = VK_TRUE
   * });
   * ```
   */
  template<typename T>
  auto& pushBack(T nextVulkanChainStruct) {
    ASSERT(currentIndex_ < CHAIN_SIZE, "Chain is full");  // 检查链是否已满
    data_[currentIndex_] = nextVulkanChainStruct;          // 存储结构体到数组中

    // 获取存储在数组中的结构体引用
    // std::any_cast 用于从类型擦除的容器中恢复原始类型
    auto& next = std::any_cast<T&>(data_[currentIndex_]);

    // 将新结构体链接到现有链的前面
    // std::exchange 原子地设置 firstNext_ 为 &next，并返回旧值
    next.pNext = std::exchange(firstNext_, &next);
    currentIndex_++;  // 移动到下一个可用位置

    return next;  // 返回引用，允许调用者进一步配置
  }

  /**
   * 获取功能链的头指针
   * 
   * 这个指针会被传递给 Vulkan API（如 vkCreateDevice）的 pNext 字段。
   * GPU 驱动会从这个指针开始，沿着链读取所有启用的功能。
   * 
   * @return 指向功能链第一个元素的指针，如果链为空则返回 nullptr
   */
  [[nodiscard]] void* firstNextPtr() const { return firstNext_; };

 private:
  /**
   * 功能结构体存储数组
   * 
   * 使用 std::any 进行类型擦除，允许存储不同类型的 Vulkan 结构体：
   * - VkPhysicalDeviceRayTracingPipelineFeaturesKHR
   * - VkPhysicalDeviceMultiviewFeatures
   * - VkPhysicalDeviceVulkan12Features
   * - 等等...
   * 
   * 为什么使用 std::any？
   * 1. 类型安全：编译时检查类型转换
   * 2. 内存效率：避免虚函数和继承开销
   * 3. 灵活性：可以存储任意 Vulkan 结构体类型
   */
  std::array<std::any, CHAIN_SIZE> data_;
  
  VkBaseInStructure* root_ = nullptr;  ///< 根节点指针（保留字段，当前未使用）
  int currentIndex_ = 0;               ///< 当前可用数组位置的索引
  void* firstNext_ = VK_NULL_HANDLE;   ///< 指向功能链第一个元素的指针
};

/**
 * Vulkan 功能链使用示例：
 * 
 * ```cpp
 * // 创建功能链
 * VulkanFeatureChain<5> featureChain;
 * 
 * // 添加光线追踪功能
 * auto& rtFeatures = featureChain.pushBack(
 *     VkPhysicalDeviceRayTracingPipelineFeaturesKHR{
 *         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
 *         .rayTracingPipeline = VK_TRUE
 *     });
 * 
 * // 添加多视图功能（用于 VR）
 * auto& multiviewFeatures = featureChain.pushBack(
 *     VkPhysicalDeviceMultiviewFeatures{
 *         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
 *         .multiview = VK_TRUE
 *     });
 * 
 * // 在设备创建时使用功能链
 * VkDeviceCreateInfo deviceInfo{
 *     .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
 *     .pNext = featureChain.firstNextPtr(),  // 传递功能链
 *     // ... 其他字段
 * };
 * ```
 */

/**
 * Vulkan 上下文（Context）类
 * 
 * 什么是 Vulkan 上下文？
 * 
 * 上下文类是整个 Vulkan 应用程序的核心和管理中心。想象它就像一个“总指挥官”：
 * - 管理所有的 GPU 资源（缓冲区、纹理、管线等）
 * - 协调不同组件之间的交互
 * - 提供统一的资源创建和管理接口
 * - 处理 Vulkan 的复杂性，对外提供简化的 API
 * 
 * Context 的主要职责：
 * 
 * 1. **Vulkan 实例管理**：
 *    - 创建和管理 VkInstance（Vulkan 实例）
 *    - 启用所需的层（Layer）和扩展（Extension）
 *    - 配置调试和验证功能
 * 
 * 2. **物理设备选择**：
 *    - 枚举系统中的所有 GPU
 *    - 根据需求选择最合适的 GPU
 *    - 检查 GPU 支持的功能和限制
 * 
 * 3. **逻辑设备创建**：
 *    - 创建 VkDevice（逻辑设备）
 *    - 配置所需的功能特性
 *    - 创建命令队列（图形、计算、传输等）
 * 
 * 4. **资源管理**：
 *    - 内存分配器（VMA）的创建和管理
 *    - 缓冲区、纹理、采样器的创建
 *    - 管线和着色器的管理
 * 
 * 5. **显示系统**：
 *    - 交换链（Swapchain）的创建和管理
 *    - 与窗口系统的集成
 *    - 帧速率和同步控制
 * 
 * 6. **调试和监控**：
 *    - 对象命名和标记
 *    - 内存使用情况统计
 *    - 性能分析和问题诊断
 * 
 * Context 的设计理念：
 * 
 * 1. **RAII 资源管理**：所有 Vulkan 资源都在析构函数中自动清理
 * 2. **工厂模式**：提供统一的资源创建接口
 * 3. **只能移动**：禁止拷贝，确保资源所有权唯一
 * 4. **类型安全**：使用强类型参数和 RAII 智能指针
 * 
 * 使用场景：
 * - 现代游戏引擎：管理复杂的 3D 渲柕管线
 * - 图形工作站：高性能图像和视频处理
 * - 科学计算： GPU 加速的并行计算
 * - VR/AR 应用：低延迟的实时渲柕
 * - 移动游戏：电池友好的高质量图形
 * 
 * 设计优势：
 * 1. **简化 API**：隐藏 Vulkan 的复杂性，提供直观的接口
 * 2. **错误处理**：集中化的错误检查和诊断
 * 3. **性能优化**：内置最佳实践和优化策略
 * 4. **可维护性**：清晰的代码结构和丰富的文档
 */
class Context final {
 public:
  /**
   * 只允许移动构造和移动赋值，禁止拷贝
   * 
   * 为什么禁止拷贝？
   * 1. **资源所有权**：Context 拥有 Vulkan 资源的所有权，拷贝会导致重复释放
   * 2. **性能考虑**：Context 包含大量数据，拷贝操作昂贵
   * 3. **状态一致性**：多个 Context 拷贝会导致状态不同步
   */
  MOVABLE_ONLY(Context);

  /**
   * 完整的 Vulkan 上下文构造函数（包含显示系统）
   * 
   * 这是最常用的构造函数，创建一个完整的 Vulkan 上下文，包括：
   * 1. Vulkan 实例（Instance）创建
   * 2. 物理设备选择和评估
   * 3. 逻辑设备创建
   * 4. 显示表面（Surface）创建
   * 5. 命令队列设置
   * 6. 内存分配器初始化
   * 
   * 适用场景：
   * - 游戏引擎：需要完整的图形渲柕和显示功能
   * - GUI 应用：需要在窗口中显示图形内容
   * - VR/AR 应用：需要与显示系统的集成
   * 
   * 参数说明：
   * 
   * @param window 窗口句柄：指向原生窗口的指针（如 HWND、Window*、wl_surface* 等）
   *                这个窗口将用于创建 Vulkan 显示表面
   * 
   * @param requestedLayers Vulkan 层列表：验证层、调试层、性能分析层等
   *                         常见的层：
   *                         - "VK_LAYER_KHRONOS_validation" - Vulkan 验证层，开发时强烈建议启用
   *                         - "VK_LAYER_RENDERDOC_Capture" - RenderDoc 捕获层，用于图形调试
   * 
   * @param requestedInstanceExtensions 实例扩展列表：提供实例级别的额外功能
   *                                     必需的扩展：
   *                                     - "VK_KHR_surface" - 显示表面支持
   *                                     - 平台相关："VK_KHR_win32_surface" (Windows)
   *                                                   "VK_KHR_xlib_surface" (Linux X11)
   *                                                   "VK_KHR_android_surface" (Android)
   * 
   * @param requestedDeviceExtensions 设备扩展列表：提供设备级别的额外功能
   *                                   常用扩展：
   *                                   - "VK_KHR_swapchain" - 交换链支持（显示必需）
   *                                   - "VK_KHR_dynamic_rendering" - 动态渲柕
   *                                   - "VK_KHR_ray_tracing_pipeline" - 光线追踪
   * 
   * @param requestedQueueTypes 所需的命令队列类型标志：
   *                            - VK_QUEUE_GRAPHICS_BIT - 图形队列（绘制、渲柕）
   *                            - VK_QUEUE_COMPUTE_BIT - 计算队列（通用计算、着色器）
   *                            - VK_QUEUE_TRANSFER_BIT - 传输队列（数据拷贝、缓冲区操作）
   *                            可以用 | 操作符组合多个标志
   * 
   * @param printEnumerations 是否打印枚举信息：用于调试和学习
   *                          - true: 打印可用的层、扩展、GPU 信息等
   *                          - false: 静默初始化（生产环境推荐）
   * 
   * @param enableRayTracing 是否启用光线追踪功能：现代图形渲柕技术
   *                         - true: 启用 GPU 加速的实时光线追踪
   *                         - false: 传统的光栅化渲柕
   * 
   * @param name 上下文名称：用于调试和日志输出的友好名称
   * 
   * 使用示例：
   * ```cpp
   * // Windows 下的基本游戏引擎初始化
   * Context context(
   *     windowHandle,                    // HWND 窗口句柄
   *     {"VK_LAYER_KHRONOS_validation"}, // 开发时启用验证
   *     {"VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_utils"},
   *     {"VK_KHR_swapchain", "VK_KHR_dynamic_rendering"},
   *     VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
   *     true,  // 打印调试信息
   *     false, // 不启用光线追踪
   *     "MyGameEngine"
   * );
   * ```
   */
  explicit Context(void* window, const std::vector<std::string>& requestedLayers,
                   const std::vector<std::string>& requestedInstanceExtensions,
                   const std::vector<std::string>& requestedDeviceExtensions,
                   VkQueueFlags requestedQueueTypes, bool printEnumerations = false,
                   bool enableRayTracing = false, const std::string& name = "");

  /**
   * 简化的 Vulkan 上下文构造函数（仅创建实例）
   * 
   * 这个构造函数只创建 Vulkan 实例，不创建设备和显示系统。
   * 需要在后续手动调用 createVkDevice() 来完成设备创建。
   * 
   * 适用场景：
   * 1. **服务器端计算**：不需要显示系统，只做 GPU 计算
   * 2. **多 GPU 选择**：需要在多个 GPU 中手动选择
   * 3. **定制初始化**：需要更精细的控制初始化过程
   * 4. **离屏渲柕**：渲柕到文件而不是屏幕
   * 
   * 参数说明：
   * 
   * @param appInfo 应用程序信息结构体：
   *                包含应用名称、版本、引擎信息等
   *                这些信息会被 GPU 驱动用于优化和统计
   * 
   * @param requestedLayers 同上面的构造函数
   * 
   * @param requestedInstanceExtensions 同上面的构造函数
   * 
   * @param printEnumerations 同上面的构造函数
   * 
   * @param name 同上面的构造函数
   * 
   * 使用示例：
   * ```cpp
   * // GPU 计算应用（无显示）
   * VkApplicationInfo appInfo{
   *     .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
   *     .pApplicationName = "My Compute App",
   *     .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
   *     .pEngineName = "My Engine",
   *     .engineVersion = VK_MAKE_VERSION(1, 0, 0),
   *     .apiVersion = VK_API_VERSION_1_3
   * };
   * 
   * Context context(
   *     appInfo,
   *     {},  // 不需要层
   *     {},  // 不需要实例扩展
   *     false, // 不打印调试信息
   *     "ComputeContext"
   * );
   * 
   * // 后续手动选择设备并创建
   * auto physicalDevice = selectBestComputeGPU();
   * context.createVkDevice(physicalDevice, {"VK_KHR_shader_float16_int8"}, VK_QUEUE_COMPUTE_BIT);
   * ```
   */
  explicit Context(const VkApplicationInfo& appInfo,
                   const std::vector<std::string>& requestedLayers,
                   const std::vector<std::string>& requestedInstanceExtensions,
                   bool printEnumerations = false, const std::string& name = "");

  /**
   * 手动创建 Vulkan 逻辑设备
   * 
   * 这个方法用于在第二个构造函数后手动完成设备创建。
   * 它提供了更精细的控制，允许你：
   * 1. 手动选择特定的 GPU
   * 2. 精确指定所需的扩展
   * 3. 定制队列配置
   * 
   * 使用场景：
   * - 多 GPU 系统：需要选择最适合的 GPU
   * - 性能优化：根据具体任务选择 GPU 特性
   * - 云计算：需要检查和选择可用资源
   * 
   * @param vkPhysicalDevice 所选的物理设备（GPU）句柄
   *                         可以通过 vkEnumeratePhysicalDevices 获取
   * 
   * @param requestedDeviceExtensions 设备扩展列表，同构造函数的参数
   * 
   * @param requestedQueueTypes 所需的队列类型，同构造函数的参数
   * 
   * @param name 设备名称，用于调试标识
   * 
   * 注意事项：
   * 1. 必须在第二个构造函数后调用
   * 2. 只能调用一次，重复调用会导致错误
   * 3. 调用前需要确认物理设备支持所需扩展
   * 
   * 使用示例：
   * ```cpp
   * // 首先使用第二个构造函数创建 Context
   * Context context(appInfo, {}, {}, false, "MyContext");
   * 
   * // 枚举并选择 GPU
   * uint32_t deviceCount;
   * vkEnumeratePhysicalDevices(context.instance(), &deviceCount, nullptr);
   * std::vector<VkPhysicalDevice> devices(deviceCount);
   * vkEnumeratePhysicalDevices(context.instance(), &deviceCount, devices.data());
   * 
   * // 选择最强的 GPU
   * VkPhysicalDevice bestGPU = selectBestGPU(devices);
   * 
   * // 创建设备
   * context.createVkDevice(
   *     bestGPU,
   *     {"VK_KHR_ray_tracing_pipeline", "VK_KHR_acceleration_structure"},
   *     VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,
   *     "RayTracingDevice"
   * );
   * ```
   */
  void createVkDevice(VkPhysicalDevice vkPhysicalDevice,
                      const std::vector<std::string>& requestedDeviceExtensions,
                      VkQueueFlags requestedQueueTypes, const std::string& name = "");

  /**
   * Context 析构函数
   * 
   * 自动清理所有 Vulkan 资源，包括：
   * 1. **设备和实例**：销毁 VkDevice 和 VkInstance
   * 2. **内存分配器**：释放 VMA 分配器和所有分配的内存
   * 3. **显示系统**：清理交换链和显示表面
   * 4. **调试资源**：移除调试回调和验证层
   * 
   * RAII 原则保证：
   * - 无需手动调用清理函数
   * - 异常安全：即使构造函数失败也不会泄露资源
   * - 正确的清理顺序：按 Vulkan 规范的要求清理资源
   * 
   * 注意事项：
   * 1. **线程安全**：不要在多个线程中同时访问正在析构的 Context
   * 2. **命令缓冲区**：清理前会等待所有 GPU 操作完成
   * 3. **依赖关系**：确保所有使用 Context 的对象都在 Context 之前清理
   * 
   * 清理顺序（符合 Vulkan 规范）：
   * 1. 等待设备空闲（vkDeviceWaitIdle）
   * 2. 清理交换链和帧缓冲区
   * 3. 销毁内存分配器（VMA）
   * 4. 清理调试和验证系统
   * 5. 销毁设备和实例
   */
  ~Context();

  // ========================================
  // Vulkan 功能特性启用方法
  // ========================================
  
  /**
   * 这些静态方法用于在 Context 创建之前启用各种 Vulkan 功能特性。
   * 它们使用静态变量存储功能配置，在设备创建时传递给 Vulkan。
   * 
   * 使用模式：
   * 1. 在创建 Context 之前调用需要的启用方法
   * 2. 创建 Context 时会自动应用这些功能
   * 3. 所有 Context 实例共享相同的功能配置
   */
  
  /**
   * 启用默认功能特性
   * 
   * 启用 Vulkan 的基本功能特性，包括：
   * - 基本的图形渲柕功能
   * - 着色器的基本操作
   * - 纹理采样和过滤
   * 
   * 适用场景：所有项目都应该调用这个方法作为基础
   */
  static void enableDefaultFeatures();

  /**
   * 启用标量布局功能
   * 
   * 允许传统的 GLSL 标量布局方式（类似 C 结构体）。
   * 这简化了从传统 OpenGL/DirectX 迁移到 Vulkan 的过程。
   * 
   * 技术细节：
   * - 启用 scalarBlockLayout 功能
   * - 允许数据在缓冲区中以紧密排列
   * - 减少内存浪费和提高缓存命中率
   * 
   * 适用场景：
   * - 从 OpenGL/DirectX 迁移的项目
   * - 需要优化内存使用的应用
   * - 大量 uniform buffer 操作的场景
   */
  static void enableScalarLayoutFeatures();

  /**
   * 启用动态渲柕功能
   * 
   * 启用 Vulkan 1.3 的动态渲柕特性，允许不创建 VkRenderPass 和 VkFramebuffer。
   * 这是现代 Vulkan 应用的推荐做法。
   * 
   * 动态渲柕的优势：
   * - 简化 API 使用，减少样板代码
   * - 运行时决定渲柕目标，提高灵活性
   * - 减少驱动开销，提高性能
   * 
   * 适用场景：
   * - Vulkan 1.3+ 的现代应用
   * - 需要动态改变渲柕配置的应用
   * - 复杂的渲柕管线（如延迟渲柕）
   */
  static void enableDynamicRenderingFeature();

  /**
   * 启用缓冲区设备地址功能
   * 
   * 允许着色器直接访问缓冲区的 GPU 地址，类似 C 语言的指针。
   * 这是 GPU 高级编程和性能优化的基础。
   * 
   * 功能特点：
   * - 着色器可以像使用指针一样访问缓冲区
   * - 支持动态索引和间接访问
   * - 允许更灵活的数据结构设计
   * 
   * 适用场景：
   * - 光线追踪：需要访问加速结构数据
   * - GPU 驱动的渲柕：动态生成绘制命令
   * - 复杂的数据结构：链表、树等动态结构
   * - 大型世界渲柕：流式加载和 LOD 管理
   */
  static void enableBufferDeviceAddressFeature();

  /**
   * 启用间接渲柕功能
   * 
   * 允许 GPU 从缓冲区中读取绘制命令参数，实现 GPU 驱动的渲柕。
   * 这是现代游戏引擎的关键技术之一。
   * 
   * 间接渲柕的优势：
   * - 减少 CPU-GPU 同步，提高并行性
   * - 支持 GPU 端的遮挡剩除（culling）
   * - 允许动态生成绘制命令
   * - 提高 draw call 的批处理效率
   * 
   * 适用场景：
   * - 现代游戏引擎：大量对象的高效渲柕
   * - 粒子系统：动态生成粒子数据
   * - 程序化生成：动态生成几何体
   * - LOD 系统：GPU 端的精度控制
   */
  static void enableIndirectRenderingFeature();

  /**
   * 启用 16 位浮点数功能
   * 
   * 允许着色器使用 16 位浮点数（half float），提高性能并减少内存使用。
   * 在移动设备和现代 GPU 上特别有用。
   * 
   * 16 位浮点数的优势：
   * - 内存使用量减半：提高缓存命中率
   * - 带宽使用减半：提高数据传输效率
   * - GPU 原生支持：现代 GPU 的优化设计
   * - 电池友好：移动设备上显著减少功耗
   * 
   * 适用场景：
   * - 移动游戏：电池和带宽优化
   * - VR/AR 应用：高刷新率下的性能优化
   * - AI 推理：神经网络的高效计算
   * - 图像处理：大分辨率纹理处理
   */
  static void enable16bitFloatFeature();

  /**
   * 启用独立混合功能
   * 
   * 允许每个颜色附件使用不同的混合配置。
   * 这对于复杂的渲柕效果和延迟渲柕特别有用。
   * 
   * 独立混合的作用：
   * - 每个颜色附件可以有不同的混合模式
   * - 允许更精细的渲柕控制
   * - 支持复杂的后处理效果
   * 
   * 适用场景：
   * - 延迟渲柕：不同 G-Buffer 附件的不同混合
   * - HDR 渲柕：不同亮度范围的混合
   * - 特效渲柕：复杂的视觉效果合成
   * - UI 渲柕：不同元素的不同透明度处理
   */
  static void enableIndependentBlending();

  /**
   * 启用维护 4 功能
   * 
   * 启用 VK_KHR_maintenance4 扩展的功能，提供一些 API 改进和修复。
   * 这些是 Vulkan 规范的持续改进，提高稳定性和性能。
   * 
   * 主要改进包括：
   * - 更好的错误检查和验证
   * - 优化的内存管理
   * - 更精确的同步控制
   * 
   * 适用场景：所有现代 Vulkan 应用都应该启用
   */
  static void enableMaintenance4Feature();

  /**
   * 启用同步 2 功能
   * 
   * 启用 Vulkan 1.3 的新同步原语，提供更简单和高效的同步控制。
   * 这是替代传统 barrier 和 event 的现代方法。
   * 
   * 同步 2 的优势：
   * - 更直观的 API 设计
   * - 更精细的同步控制
   * - 更好的性能特性
   * - 更容易的调试
   * 
   * 适用场景：
   * - Vulkan 1.3+ 的现代应用
   * - 需要精细同步控制的应用
   * - 高性能多线程渲柕
   * - 异步计算和渲柕管线
   */
  static void enableSynchronization2Feature();

  /**
   * 启用光线追踪功能
   * 
   * 启用 GPU 硬件加速的实时光线追踪功能。
   * 这是下一代图形渲柕技术的核心。
   * 
   * 光线追踪的功能：
   * - 实时全局光照计算
   * - 物理准确的阴影和反射
   * - 动态环境光照
   * - 透明和折射效果
   * 
   * 适用场景：
   * - AAA 游戏：电影级的视觉质量
   * - 建筑可视化：照片级的光照效果
   * - 影视制作：实时的电影级渲柕
   * - VR 应用：沉浸式的现实体验
   * 
   * 注意：需要 RTX 20系列以上或同等级别的 GPU
   */
  static void enableRayTracingFeatures();

  static bool enableMultiViewFlag_;  ///< 多视图功能的全局启用标志
  
  /**
   * 启用多视图功能
   * 
   * 允许一次渲柕调用生成多个视图（如 VR 的左右眼）。
   * 这是 VR/AR 应用的关键性能优化技术。
   * 
   * 多视图渲柕的优势：
   * - 减少 CPU 开销：一次 draw call 生成多个视图
   * - 减少带宽使用：共享顶点数据
   * - 提高 VR 性能：达到 90Hz 或 120Hz 的高刷新率
   * 
   * 适用场景：
   * - VR 应用：左右眼同时渲柕
   * - AR 应用：多相机视图
   * - 立体显示：3D 显示器的多视角
   * - 安全监控：多相机同时渲柕
   */
  static void enableMultiView();

  /**
   * 检查多视图功能是否已启用
   * 
   * @return true 如果多视图功能已经启用
   */
  static bool isMultiviewEnabled();

  /**
   * 启用片段密度映射功能
   * 
   * 允许根据屏幕区域的重要性动态调整渲柕精度。
   * 这是移动 VR 和高分辨率显示器的关键优化技术。
   * 
   * 片段密度映射的作用：
   * - 根据视角中心距离调整精度
   * - 在不重要区域降低渲柕精度
   * - 在重要区域保持高精度
   * 
   * 适用场景：
   * - 移动 VR：电池优化和温度控制
   * - 4K/8K 显示：在边缘区域降低精度
   * - 手机游戏：平衡画质和性能
   */
  static void enableFragmentDensityMapFeatures();

  /**
   * 启用片段密度映射偏移功能
   * 
   * 高通 (Qualcomm) 特有的片段密度映射扩展功能。
   * 提供更精细的密度控制和偏移调整。
   * 
   * 适用场景：高通芝片的 Android 设备
   */
  static void enableFragmentDensityMapOffsetFeatures();

  // ========================================
  // Vulkan 对象访问方法
  // ========================================
  
  /**
   * 获取 Vulkan 逻辑设备句柄
   * 
   * VkDevice 是 Vulkan 中最重要的对象之一，代表一个 GPU 的逻辑连接。
   * 所有的资源创建和命令提交都需要通过这个设备句柄。
   * 
   * 使用场景：
   * - 传递给其他 Vulkan API 函数
   * - 创建低级 Vulkan 对象
   * - 与第三方库集成
   * 
   * @return VkDevice 句柄，不为 null（除非 Context 构造失败）
   */
  VkDevice device() const { return device_; }

  /**
   * 获取 Vulkan 实例句柄
   * 
   * VkInstance 是 Vulkan 应用程序和 Vulkan 库之间的连接。
   * 它管理全局状态和平台相关的功能。
   * 
   * 使用场景：
   * - 枚举物理设备
   * - 创建显示表面
   * - 查询实例级别的信息
   * - 调试和验证功能
   * 
   * @return VkInstance 句柄，不为 null
   */
  VkInstance instance() const { return instance_; }

  /**
   * 获取 Vulkan 内存分配器
   * 
   * VmaAllocator 是 Vulkan Memory Allocator 的核心对象。
   * 它简化了 Vulkan 的内存管理，提供了高效的内存分配和管理。
   * 
   * VMA 的优势：
   * - 自动的内存类型选择
   * - 内存池和块管理
   * - 片段化减少和内存统计
   * - 线程安全的内存分配
   * 
   * @return VmaAllocator 句柄，用于内存分配和释放
   */
  [[nodiscard]] inline VmaAllocator memoryAllocator() const { return allocator_; }

  /**
   * 获取物理设备信息
   * 
   * PhysicalDevice 封装了物理 GPU 的所有信息和能力。
   * 包括硬件规格、支持的功能、限制和性能参数。
   * 
   * 使用场景：
   * - 检查 GPU 能力和限制
   * - 选择最优的设置参数
   * - 性能估算和调优
   * - 特性兼容性检查
   * 
   * @return PhysicalDevice 对象的引用，包含所有 GPU 信息
   */
  const PhysicalDevice& physicalDevice() const;

  // ========================================
  // 显示系统管理方法
  // ========================================
  
  /**
   * 创建 Vulkan 交换链
   * 
   * 交换链是 Vulkan 与窗口系统的桥梁，负责将渲柕结果显示到屏幕上。
   * 它管理一个图像池，在GPU渲柕和屏幕显示之间提供缓冲。
   * 
   * 交换链的工作原理：
   * 1. 维护多个图像（通常 2-3 个）
   * 2. GPU 渲柕到当前图像
   * 3. 显示系统显示另一个完成的图像
   * 4. 完成后交换图像角色
   * 
   * @param format 像素格式：定义颜色的存储方式
   *               常用格式：
   *               - VK_FORMAT_B8G8R8A8_SRGB - 8位 sRGB 颜色（最常用）
   *               - VK_FORMAT_R8G8B8A8_UNORM - 8位线性颜色
   *               - VK_FORMAT_A2B10G10R10_UNORM_PACK32 - 10位 HDR 颜色
   * 
   * @param colorSpace 颜色空间：定义颜色的解释方式
   *                   常用空间：
   *                   - VK_COLOR_SPACE_SRGB_NONLINEAR_KHR - 标准 sRGB 颜色空间
   *                   - VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT - P3 广色域
   *                   - VK_COLOR_SPACE_HDR10_ST2084_EXT - HDR10 高动态范围
   * 
   * @param presentMode 显示模式：控制图像显示的时机和方式
   *                    模式对比：
   *                    - VK_PRESENT_MODE_IMMEDIATE_KHR - 立即显示，最低延迟但可能有撤裂
   *                    - VK_PRESENT_MODE_FIFO_KHR - 垂直同步，无撤裂但可能有延迟
   *                    - VK_PRESENT_MODE_FIFO_RELAXED_KHR - 自适应同步
   *                    - VK_PRESENT_MODE_MAILBOX_KHR - 低延迟 + 无撤裂（最理想）
   * 
   * @param extent 图像分辨率：交换链图像的像素大小
   *               通常与窗口大小一致，但可以不同（用于渲柕缩放）
   * 
   * 注意事项：
   * 1. 必须在创建 Context 后调用
   * 2. 窗口大小改变时需要重新创建
   * 3. 参数必须与 GPU 和显示系统兼容
   * 
   * 使用示例：
   * ```cpp
   * // 标准 PC 游戏设置
   * context.createSwapchain(
   *     VK_FORMAT_B8G8R8A8_SRGB,           // sRGB 颜色
   *     VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, // 标准颜色空间
   *     VK_PRESENT_MODE_MAILBOX_KHR,        // 低延迟高质量
   *     {1920, 1080}                        // 1080p 分辨率
   * );
   * 
   * // 移动设备优化设置
   * context.createSwapchain(
   *     VK_FORMAT_B8G8R8A8_SRGB,
   *     VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
   *     VK_PRESENT_MODE_FIFO_KHR,           // 节电模式
   *     {screenWidth, screenHeight}
   * );
   * ```
   */
  void createSwapchain(VkFormat format, VkColorSpaceKHR colorSpace,
                       VkPresentModeKHR presentMode, const VkExtent2D& extent);

  /**
   * 获取当前的交换链对象
   * 
   * 返回对当前活跃交换链的指针，允许访问交换链的所有功能。
   * 
   * 交换链的主要操作：
   * - 获取下一帧用于渲柕的图像
   * - 将渲柕完成的图像提交显示
   * - 管理多重缓冲和同步
   * 
   * @return Swapchain* 指针，如果尚未创建则为 nullptr
   * 
   * 使用示例：
   * ```cpp
   * auto* swapchain = context.swapchain();
   * if (swapchain) {
   *     // 获取下一帧图像
   *     auto imageIndex = swapchain->acquireImage();
   *     
   *     // 渲柕...
   *     
   *     // 显示渲柕结果
   *     swapchain->present(imageIndex);
   * }
   * ```
   */
  Swapchain* swapchain() const;

  /**
   * 获取指定索引的图形队列
   * 
   * 图形队列是专用于图形渲柕命令的 GPU 执行队列。
   * 它支持所有类型的图形操作：绘制、计算、传输、显示。
   * 
   * 图形队列的作用：
   * - 提交渲柕命令（vkCmdDraw*）
   * - 提交计算命令（vkCmdDispatch*）
   * - 管理内存屏障和同步
   * - 交换链的图像提交
   * 
   * @param index 队列索引，默认为 0（主图形队列）
   *              多队列用于并行渲柕和负载平衡
   * 
   * @return VkQueue 句柄，用于提交命令到 GPU
   * 
   * 使用示例：
   * ```cpp
   * // 提交主渲柕命令
   * auto mainQueue = context.graphicsQueue(0);
   * vkQueueSubmit(mainQueue, 1, &submitInfo, fence);
   * 
   * // 并行渲柕（如果有多个队列）
   * auto secondaryQueue = context.graphicsQueue(1);
   * vkQueueSubmit(secondaryQueue, 1, &secondarySubmit, secondaryFence);
   * ```
   */
  VkQueue graphicsQueue(int index = 0) const { return graphicsQueues_[index]; }

  // ========================================
  // GPU 缓冲区创建方法
  // ========================================
  
  /**
   * 创建通用 GPU 缓冲区
   * 
   * 缓冲区是 GPU 上的一块连续内存，用于存储各种数据。
   * 它是 Vulkan 中最基础和最重要的资源类型之一。
   * 
   * 缓冲区的常见用途：
   * - 顶点缓冲区：存储 3D 模型的顶点数据
   * - 索引缓冲区：存储顶点的连接顺序
   * - 统一缓冲区：传递全局数据给着色器
   * - 存储缓冲区：大量数据的结构化存储
   * 
   * @param size 缓冲区大小（字节）。注意对齐要求：
   *             - 顶点数据：通常按 4 字节对齐
   *             - Uniform 数据：可能需要 256 字节对齐
   *             - 存储缓冲区：可能需要特定对齐
   * 
   * @param flags 缓冲区用途标志，可以组合多个：
   *              - VK_BUFFER_USAGE_VERTEX_BUFFER_BIT - 顶点缓冲区
   *              - VK_BUFFER_USAGE_INDEX_BUFFER_BIT - 索引缓冲区
   *              - VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT - 统一缓冲区
   *              - VK_BUFFER_USAGE_STORAGE_BUFFER_BIT - 存储缓冲区
   *              - VK_BUFFER_USAGE_TRANSFER_SRC_BIT - 传输源
   *              - VK_BUFFER_USAGE_TRANSFER_DST_BIT - 传输目标
   * 
   * @param memoryUsage VMA 内存使用类型，影响性能和可访问性：
   *                    - VMA_MEMORY_USAGE_GPU_ONLY - 仅 GPU 访问，最高性能
   *                    - VMA_MEMORY_USAGE_CPU_ONLY - 仅 CPU 访问，用于暂存
   *                    - VMA_MEMORY_USAGE_CPU_TO_GPU - CPU 写入，GPU 读取
   *                    - VMA_MEMORY_USAGE_GPU_TO_CPU - GPU 写入，CPU 读取
   *                    - VMA_MEMORY_USAGE_CPU_COPY - CPU 端的快速拷贝
   * 
   * @param name 调试名称，用于 GPU 调试器中的标识
   * 
   * @return 智能指针，自动管理缓冲区的生命周期
   * 
   * 使用示例：
   * ```cpp
   * // 创建顶点缓冲区
   * auto vertexBuffer = context.createBuffer(
   *     vertices.size() * sizeof(Vertex),
   *     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
   *     VMA_MEMORY_USAGE_GPU_ONLY,
   *     "MainVertexBuffer"
   * );
   * 
   * // 创建动态 Uniform 缓冲区
   * auto uniformBuffer = context.createBuffer(
   *     sizeof(UniformData),
   *     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
   *     VMA_MEMORY_USAGE_CPU_TO_GPU,  // 每帧更新
   *     "PerFrameUniforms"
   * );
   * ```
   */
  std::shared_ptr<Buffer> createBuffer(size_t size, VkBufferUsageFlags flags,
                                       VmaMemoryUsage memoryUsage,
                                       const std::string& name = "") const;

  /**
   * 创建持久性缓冲区（CPU 可访问）
   * 
   * 这是 createBuffer 的便捷版本，专门用于创建 CPU 可直接访问的缓冲区。
   * 适用于需要频繁更新或读取的数据。
   * 
   * 持久性缓冲区的特点：
   * - 内存映射保持活跃，无需反复 map/unmap
   * - CPU 和 GPU 都可以直接访问
   * - 适中的性能，但便于使用
   * - 自动处理内存一致性
   * 
   * @param size 缓冲区大小（字节）
   * @param flags 缓冲区用途标志，同 createBuffer
   * @param name 调试名称
   * 
   * @return 智能指针，内存已经映射可直接使用
   * 
   * 使用场景：
   * - 每帧更新的 Uniform 数据
   * - 动态生成的顶点数据
   * - 实时的参数调整
   * - CPU-GPU 共享的状态数据
   * 
   * 使用示例：
   * ```cpp
   * // 创建可直接写入的 Uniform 缓冲区
   * auto persistentUBO = context.createPersistentBuffer(
   *     sizeof(CameraData),
   *     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
   *     "CameraUniforms"
   * );
   * 
   * // 直接写入数据，无需 map/unmap
   * auto* mappedData = static_cast<CameraData*>(persistentUBO->mapped());
   * mappedData->viewMatrix = camera.getViewMatrix();
   * mappedData->projMatrix = camera.getProjectionMatrix();
   * ```
   */
  std::shared_ptr<Buffer> createPersistentBuffer(size_t size, VkBufferUsageFlags flags,
                                                 const std::string& name = "") const;

  /**
   * 创建暂存缓冲区
   * 
   * 暂存缓冲区是用于从 CPU 向 GPU 传输数据的中间缓冲区。
   * 它们通常用于一次性的数据上传，之后就可以释放。
   * 
   * 暂存缓冲区的作用：
   * 1. CPU 写入数据到暂存缓冲区
   * 2. GPU 从暂存缓冲区拷贝到目标缓冲区
   * 3. 暂存缓冲区可以被释放
   * 
   * @param size 暂存缓冲区大小（字节）
   * @param usage 缓冲区用途，通常包含 VK_BUFFER_USAGE_TRANSFER_SRC_BIT
   * @param name 调试名称
   * 
   * @return 智能指针，内存已映射可直接写入
   * 
   * 使用示例：
   * ```cpp
   * // 加载顶点数据
   * std::vector<Vertex> vertices = loadModel("model.obj");
   * 
   * // 创建暂存缓冲区
   * auto stagingBuffer = context.createStagingBuffer(
   *     vertices.size() * sizeof(Vertex),
   *     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
   *     "VertexStaging"
   * );
   * 
   * // 拷贝数据到暂存缓冲区
   * memcpy(stagingBuffer->mapped(), vertices.data(), vertices.size() * sizeof(Vertex));
   * 
   * // GPU 端拷贝到最终缓冲区...
   * ```
   */
  std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                              const std::string& name = "") const;

  /**
   * 为特定目标缓冲区创建暂存缓冲区
   * 
   * 这个版本会确保暂存缓冲区的大小和对齐与目标缓冲区完全匹配。
   * 这避免了手动计算大小和处理对齐问题。
   * 
   * @param size 暂存缓冲区大小（字节）
   * @param usage 缓冲区用途
   * @param actualBuffer 目标缓冲区指针，用于获取对齐和其他属性
   * @param name 调试名称
   * 
   * @return 智能指针，与目标缓冲区完全兼容
   * 
   * 使用示例：
   * ```cpp
   * // 先创建目标 GPU 缓冲区
   * auto gpuBuffer = context.createBuffer(
   *     dataSize,
   *     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
   *     VMA_MEMORY_USAGE_GPU_ONLY
   * );
   * 
   * // 为该缓冲区创建匹配的暂存缓冲区
   * auto stagingBuffer = context.createStagingBuffer(
   *     dataSize,
   *     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
   *     gpuBuffer.get(),
   *     "MatchedStaging"
   * );
   * 
   * // 传输数据...
   * ```
   */
  std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                              Buffer* actualBuffer,
                                              const std::string& name = "") const;

  /**
   * 上传数据到 GPU 缓冲区
   * 
   * 这是一个便捷方法，自动处理从 CPU 内存到 GPU 缓冲区的数据传输。
   * 它内部会创建暂存缓冲区、拷贝数据并执行 GPU 端的传输。
   * 
   * 数据传输的工作流程：
   * 1. 创建临时暂存缓冲区
   * 2. CPU 端拷贝数据到暂存缓冲区
   * 3. 录制 GPU 端的拷贝命令
   * 4. 提交并等待拷贝完成
   * 5. 自动清理暂存缓冲区
   * 
   * @param queueMgr 命令队列管理器，用于提交传输命令
   *                 建议使用专用的传输队列以获得最佳性能
   * 
   * @param commandBuffer 命令缓冲区，用于录制拷贝命令
   *                      必须处于录制状态（begin 之后）
   * 
   * @param gpuBuffer 目标 GPU 缓冲区，必须支持 TRANSFER_DST 用途
   *                  并且在 GPU 上可访问
   * 
   * @param data 源数据指针，指向要传输的 CPU 内存
   *             数据必须在命令执行期间保持有效
   * 
   * @param totalSize 要传输的总字节数
   *                  不能超过目标缓冲区的剩余空间
   * 
   * @param gpuBufferOffset 目标缓冲区内的偏移量（字节）
   *                        允许部分更新缓冲区内容
   * 
   * 性能考虑：
   * - 将多个小传输合并为一个大传输可以提高效率
   * - 使用专用的传输队列可以与渲柕并行
   * - 对于频繁更新的数据，考虑使用 CPU 可访问的缓冲区
   * 
   * 使用示例：
   * ```cpp
   * // 加载顶点数据
   * std::vector<Vertex> vertices = loadMeshData();
   * 
   * // 创建 GPU 缓冲区
   * auto vertexBuffer = context.createBuffer(
   *     vertices.size() * sizeof(Vertex),
   *     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
   *     VMA_MEMORY_USAGE_GPU_ONLY
   * );
   * 
   * // 创建传输队列和命令缓冲区
   * auto transferQueue = context.createTransferCommandQueue(1, 1, "Upload");
   * auto cmdBuffer = transferQueue.getCmdBufferToBegin();
   * 
   * // 上传数据
   * context.uploadToGPUBuffer(
   *     transferQueue,
   *     cmdBuffer,
   *     vertexBuffer.get(),
   *     vertices.data(),
   *     vertices.size() * sizeof(Vertex)
   * );
   * 
   * transferQueue.endCmdBuffer(cmdBuffer);
   * transferQueue.submit();
   * transferQueue.waitForQueueIdle();
   * ```
   */
  void uploadToGPUBuffer(VkCore::CommandQueueManager& queueMgr,
                         VkCommandBuffer commandBuffer, VkCore::Buffer* gpuBuffer,
                         const void* data, long totalSize,
                         uint64_t gpuBufferOffset = 0) const;

  // ========================================
  // 纹理和采样器创建方法
  // ========================================
  
  /**
   * 创建 Vulkan 纹理
   * 
   * 纹理是 GPU 上的多维图像数据，用于存储和处理视觉内容。
   * 它们可以是纹理贴图、渲柕目标、深度缓冲区等。
   * 
   * 纹理的常见用途：
   * - 漫反射贴图：物体表面的颜色和细节
   * - 法线贴图：表面的凹凸信息
   * - 高度图：地形或位移贴图
   * - 立方体贴图：环境光照和反射
   * - 渲柕目标：离屏渲柕的输出
   * - 深度/模板缓冲区：3D 渲柕的测试
   * 
   * @param type 纹理类型，定义维度和结构：
   *             - VK_IMAGE_TYPE_1D - 一维纹理（如渐变条带）
   *             - VK_IMAGE_TYPE_2D - 二维纹理（最常用）
   *             - VK_IMAGE_TYPE_3D - 三维纹理（体纹理、体数据）
   * 
   * @param format 像素格式，定义数据的存储方式：
   *               颜色格式：
   *               - VK_FORMAT_R8G8B8A8_UNORM - 8位 RGBA（最常用）
   *               - VK_FORMAT_R8G8B8A8_SRGB - 8位 sRGB 伽马校正
   *               - VK_FORMAT_R16G16B16A16_SFLOAT - 16位 HDR 浮点
   *               - VK_FORMAT_R32G32B32A32_SFLOAT - 32位高精度浮点
   *               深度/模板格式：
   *               - VK_FORMAT_D32_SFLOAT - 32位深度
   *               - VK_FORMAT_D24_UNORM_S8_UINT - 24位深度 + 8位模板
   * 
   * @param flags 特殊创建标志：
   *              - 0 - 标准纹理
   *              - VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT - 可用作立方体贴图
   *              - VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT - 3D 纹理可作 2D 数组访问
   * 
   * @param usageFlags 纹理的使用方式，可组合多个：
   *                   - VK_IMAGE_USAGE_SAMPLED_BIT - 在着色器中采样
   *                   - VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT - 作为颜色附件渲柕
   *                   - VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT - 深度/模板附件
   *                   - VK_IMAGE_USAGE_TRANSFER_SRC_BIT - 作为传输源
   *                   - VK_IMAGE_USAGE_TRANSFER_DST_BIT - 作为传输目标
   *                   - VK_IMAGE_USAGE_STORAGE_BIT - 计算着色器的读写访问
   * 
   * @param extents 纹理尺寸（像素）：
   *                width - 宽度（最大 16384 或更高）
   *                height - 高度（二维/三维纹理）
   *                depth - 深度（三维纹理）
   * 
   * @param numMipLevels Mipmap 级别数，用于纹理过滤优化：
   *                     1 - 无 mipmap，最快但可能有闪烁
   *                     log2(max(width, height)) + 1 - 完整 mipmap 链
   *                     自定义数量 - 部分 mipmap
   * 
   * @param layerCount 数组层数，用于特殊类型：
   *                   1 - 单一纹理
   *                   6 - 立方体贴图（六个面）
   *                   n - 纹理数组（多个纹理的集合）
   * 
   * @param memoryFlags 内存属性标志：
   *                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - GPU 本地内存（最优）
   *                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT - CPU 可访问
   * 
   * @param generateMips 是否自动生成 mipmap：
   *                     true - 自动从基级纹理生成所有级别
   *                     false - 手动提供或不使用 mipmap
   * 
   * @param msaaSamples 多采样数量，用于抗锯齿：
   *                    VK_SAMPLE_COUNT_1_BIT - 无多采样
   *                    VK_SAMPLE_COUNT_4_BIT - 4x MSAA
   *                    VK_SAMPLE_COUNT_8_BIT - 8x MSAA
   * 
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理纹理的生命周期
   * 
   * 使用示例：
   * ```cpp
   * // 创建 2D 漫反射贴图
   * auto diffuseTexture = context.createTexture(
   *     VK_IMAGE_TYPE_2D,
   *     VK_FORMAT_R8G8B8A8_SRGB,
   *     0,
   *     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
   *     {1024, 1024, 1},        // 1024x1024 分辨率
   *     std::log2(1024) + 1,    // 完整 mipmap 链
   *     1,                      // 单层
   *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
   *     true,                   // 自动生成 mipmap
   *     VK_SAMPLE_COUNT_1_BIT,
   *     "DiffuseTexture"
   * );
   * 
   * // 创建 HDR 环境立方体贴图
   * auto skyboxTexture = context.createTexture(
   *     VK_IMAGE_TYPE_2D,
   *     VK_FORMAT_R16G16B16A16_SFLOAT,  // HDR 格式
   *     VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
   *     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
   *     {512, 512, 1},
   *     1,                      // 环境贴图通常不用 mipmap
   *     6,                      // 立方体的 6 个面
   *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
   *     false,
   *     VK_SAMPLE_COUNT_1_BIT,
   *     "HDRSkybox"
   * );
   * 
   * // 创建渲柕目标（帧缓冲区）
   * auto renderTarget = context.createTexture(
   *     VK_IMAGE_TYPE_2D,
   *     VK_FORMAT_R8G8B8A8_UNORM,
   *     0,
   *     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
   *     {screenWidth, screenHeight, 1},
   *     1,
   *     1,
   *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
   *     false,
   *     VK_SAMPLE_COUNT_4_BIT,  // 4x 抗锯齿
   *     "OffscreenRenderTarget"
   * );
   * ```
   */
  std::shared_ptr<Texture> createTexture(
      VkImageType type, VkFormat format, VkImageCreateFlags flags,
      VkImageUsageFlags usageFlags, VkExtent3D extents, uint32_t numMipLevels,
      uint32_t layerCount, VkMemoryPropertyFlags memoryFlags, bool generateMips = false,
      VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
      const std::string& name = "") const;

  /**
   * 创建标准采样器
   * 
   * 采样器控制着色器如何从纹理中读取像素数据。
   * 它们定义了过滤方式、地址模式和级别的细节控制。
   * 
   * 采样器的作用：
   * 1. **过滤（Filtering）**：当纹理坐标不是整数时如何计算像素值
   * 2. **地址包装（Address Wrapping）**：当纹理坐标超出 [0,1] 范围时如何处理
   * 3. **Mipmap 选择**：根据距离选择合适的纹理精度级别
   * 4. **各向异性过滤**：减少倾斜视角下的模糊
   * 
   * @param minFilter 缩小过滤器（纹理比像素更大时用）：
   *                  - VK_FILTER_NEAREST - 最近邻过滤，像素风格，性能最好
   *                  - VK_FILTER_LINEAR - 线性过滤，平滑结果，中等性能
   * 
   * @param magFilter 放大过滤器（纹理比像素更小时用）：
   *                  参数同 minFilter，但作用相反
   * 
   * @param addressModeU U 轴（横向）地址模式：
   *                     - VK_SAMPLER_ADDRESS_MODE_REPEAT - 重复平铺，用于地面纹理
   *                     - VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT - 镜像重复，减少接缝
   *                     - VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE - 多延伸边缘像素
   *                     - VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER - 使用边框颜色
   * 
   * @param addressModeV V 轴（纵向）地址模式，参数同 addressModeU
   * 
   * @param addressModeW W 轴（深度）地址模式，用于 3D 纹理，参数同 addressModeU
   * 
   * @param maxLod 最大 LOD（Level of Detail）级别：
   *               0.0 - 仅使用基级 mipmap，最高精度
   *               1000.0 - 使用所有可用 mipmap 级别
   *               自定义值 - 限制最大模糊级别
   * 
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理采样器的生命周期
   * 
   * 使用示例：
   * ```cpp
   * // 高质量的漫反射贴图采样器
   * auto diffuseSampler = context.createSampler(
   *     VK_FILTER_LINEAR,                      // 缩小时平滑
   *     VK_FILTER_LINEAR,                      // 放大时平滑
   *     VK_SAMPLER_ADDRESS_MODE_REPEAT,        // U 轴重复
   *     VK_SAMPLER_ADDRESS_MODE_REPEAT,        // V 轴重复
   *     VK_SAMPLER_ADDRESS_MODE_REPEAT,        // W 轴重复
   *     1000.0f,                               // 使用所有 mipmap
   *     "DiffuseSampler"
   * );
   * 
   * // 像素风格的采样器（如 Minecraft）
   * auto pixelArtSampler = context.createSampler(
   *     VK_FILTER_NEAREST,                     // 保持像素化效果
   *     VK_FILTER_NEAREST,
   *     VK_SAMPLER_ADDRESS_MODE_REPEAT,
   *     VK_SAMPLER_ADDRESS_MODE_REPEAT,
   *     VK_SAMPLER_ADDRESS_MODE_REPEAT,
   *     0.0f,                                  // 不使用 mipmap
   *     "PixelArtSampler"
   * );
   * 
   * // UI 元素的采样器
   * auto uiSampler = context.createSampler(
   *     VK_FILTER_LINEAR,
   *     VK_FILTER_LINEAR,
   *     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // 避免边缘重复
   *     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
   *     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
   *     0.0f,                                  // UI 不需要 mipmap
   *     "UISampler"
   * );
   * ```
   */
  std::shared_ptr<Sampler> createSampler(VkFilter minFilter, VkFilter magFilter,
                                         VkSamplerAddressMode addressModeU,
                                         VkSamplerAddressMode addressModeV,
                                         VkSamplerAddressMode addressModeW, float maxLod,
                                         const std::string& name = "") const;

  /**
   * 创建带深度比较的采样器（用于阴影映射）
   * 
   * 这个版本的采样器允许在采样时执行深度值比较。
   * 这是阴影映射技术的关键组件，用于实时阴影计算。
   * 
   * 深度比較的工作原理：
   * 1. 从深度纹理中采样得到存储的深度值
   * 2. 将该深度值与提供的参考值进行比较
   * 3. 按照比较结果返回 0.0 或 1.0
   * 
   * @param minFilter 缩小过滤器，同上面的采样器
   * @param magFilter 放大过滤器，同上面的采样器
   * @param addressModeU U 轴地址模式，同上面的采样器
   * @param addressModeV V 轴地址模式，同上面的采样器
   * @param addressModeW W 轴地址模式，同上面的采样器
   * @param maxLod 最大 LOD 级别，同上面的采样器
   * 
   * @param compareEnable 是否启用深度比较：
   *                      true - 启用深度比较，用于阴影映射
   *                      false - 禁用深度比较，作为普通采样器
   * 
   * @param compareOp 深度比较操作：
   *                  - VK_COMPARE_OP_LESS_OR_EQUAL - 小于等于（最常用于阴影）
   *                  - VK_COMPARE_OP_GREATER_OR_EQUAL - 大于等于
   *                  - VK_COMPARE_OP_LESS - 小于
   *                  - VK_COMPARE_OP_GREATER - 大于
   *                  - VK_COMPARE_OP_EQUAL - 等于
   *                  - VK_COMPARE_OP_NOT_EQUAL - 不等于
   * 
   * @param name 调试名称
   * 
   * @return 智能指针，支持深度比较的采样器
   * 
   * 使用场景：
   * - **阴影映射**：实时阴影的核心技术
   * - **阴影体积**：3D 阴影效果
   * - **深度测试**：特殊的深度检查
   * - **反射探针**：环境反射效果
   * 
   * 使用示例：
   * ```cpp
   * // 阴影映射采样器
   * auto shadowMapSampler = context.createSampler(
   *     VK_FILTER_LINEAR,                      // 平滑阴影边缘
   *     VK_FILTER_LINEAR,
   *     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // 边缘处使用白色（无阴影）
   *     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
   *     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
   *     0.0f,                                  // 阴影贴图不用 mipmap
   *     true,                                  // 启用深度比较
   *     VK_COMPARE_OP_LESS_OR_EQUAL,           // 深度小于等于时不在阴影中
   *     "ShadowMapSampler"
   * );
   * 
   * // 在着色器中使用
   * // float shadowFactor = texture(shadowMapSampler, shadowCoord);
   * // // shadowFactor 为 0.0（在阴影中）或 1.0（不在阴影中）
   * ```
   */
  std::shared_ptr<Sampler> createSampler(VkFilter minFilter, VkFilter magFilter,
                                         VkSamplerAddressMode addressModeU,
                                         VkSamplerAddressMode addressModeV,
                                         VkSamplerAddressMode addressModeW, float maxLod,
                                         bool compareEnable, VkCompareOp compareOp,
                                         const std::string& name = "") const;

  // ========================================
  // 命令队列管理方法
  // ========================================
  
  /**
   * 创建图形命令队列管理器
   * 
   * 命令队列是 GPU 执行工作的核心机制。这个管理器封装了
   * 命令缓冲区的分配、录制、提交和同步等复杂操作。
   * 
   * 图形队列的能力：
   * - 支持所有类型的操作：绘制、计算、传输、显示
   * - 与交换链的交互：可以直接提交显示结果
   * - 并行执行：多个命令缓冲区可以同时工作
   * - 自动管理：处理复杂的资源生命周期
   * 
   * @param count 命令缓冲区池大小：
   *              同时存在的命令缓冲区数量。更多缓冲区可以：
   *              - 提高并行性：多线程同时录制
   *              - 减少阻塞：避免等待缓冲区重用
   *              - 但会增加内存开销
   *              建议值：2-4 个（双缓冲或三缓冲）
   * 
   * @param concurrentNumCommands 并发命令缓冲区数：
   *                              可以同时录制的命令缓冲区数量。用于：
   *                              - CPU 多线程并行录制
   *                              - 复杂场景的分块渲柕
   *                              - 异步计算和渲柕
   *                              建议值：CPU 核心数 或 2-8 个
   * 
   * @param name 调试名称，用于性能分析和问题追踪
   * 
   * @param graphicsQueueIndex 图形队列索引：
   *                           -1 - 自动选择最适合的队列（推荐）
   *                           其他 - 指定特定的队列索引
   * 
   * @return CommandQueueManager 对象，管理命令缓冲区的整个生命周期
   * 
   * 使用场景：
   * - **主渲柕队列**：处理所有的绘制命令
   * - **后处理队列**：并行处理特效和滞后
   * - **UI 渲柕队列**：独立的用户界面渲柕
   * - **并行计算**：同时进行 GPU 计算和渲柕
   * 
   * 使用示例：
   * ```cpp
   * // 主渲柕队列（高性能）
   * auto mainRenderQueue = context.createGraphicsCommandQueue(
   *     3,    // 三缓冲，平衡延迟和性能
   *     4,    // 四线程并行录制
   *     "MainRender",
   *     0     // 使用主图形队列
   * );
   * 
   * // 轻量级后处理队列
   * auto postProcessQueue = context.createGraphicsCommandQueue(
   *     2,    // 双缓冲，减少内存开销
   *     1,    // 单线程，后处理通常不复杂
   *     "PostProcess",
   *     -1    // 自动选择队列
   * );
   * 
   * // 使用队列渲柕
   * auto cmdBuffer = mainRenderQueue.getCmdBufferToBegin();
   * // ... 录制渲柕命令 ...
   * mainRenderQueue.endCmdBuffer(cmdBuffer);
   * mainRenderQueue.submit();
   * ```
   */
  CommandQueueManager createGraphicsCommandQueue(uint32_t count,
                                                 uint32_t concurrentNumCommands,
                                                 const std::string& name = "",
                                                 int graphicsQueueIndex = -1);

  // ========================================
  // 着色器模块创建方法
  // ========================================
  
  /**
   * 从文件创建着色器模块（简化版）
   * 
   * 着色器是在 GPU 上执行的小程序，控制渲柕管线的各个阶段。
   * 这个简化版本使用默认的 "main" 入口点。
   * 
   * 着色器的作用：
   * - **顶点着色器**：处理 3D 模型的顶点数据，进行变换和投影
   * - **片段着色器**：计算每个像素的最终颜色
   * - **计算着色器**：执行通用 GPU 计算任务
   * - **几何着色器**：生成或修改几何体
   * 
   * @param filePath 着色器文件路径，支持格式：
   *                 - .glsl - GLSL 源代码（自动编译）
   *                 - .spv - 已编译的 SPIR-V 字节码
   *                 - .vert/.frag/.comp/.geom/.tesc/.tese - GLSL 源代码
   * 
   * @param stages 着色器阶段标志：
   *               - VK_SHADER_STAGE_VERTEX_BIT - 顶点着色器
   *               - VK_SHADER_STAGE_FRAGMENT_BIT - 片段着色器
   *               - VK_SHADER_STAGE_COMPUTE_BIT - 计算着色器
   *               - VK_SHADER_STAGE_GEOMETRY_BIT - 几何着色器
   *               - VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT - 细分控制
   *               - VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT - 细分评估
   * 
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理着色器模块的生命周期
   * 
   * 使用示例：
   * ```cpp
   * // 加载顶点着色器
   * auto vertexShader = context.createShaderModule(
   *     "shaders/basic.vert",
   *     VK_SHADER_STAGE_VERTEX_BIT,
   *     "BasicVertex"
   * );
   * 
   * // 加载片段着色器
   * auto fragmentShader = context.createShaderModule(
   *     "shaders/pbr.frag",
   *     VK_SHADER_STAGE_FRAGMENT_BIT,
   *     "PBRFragment"
   * );
   * ```
   */
  std::shared_ptr<ShaderModule> createShaderModule(const std::string& filePath,
                                                   VkShaderStageFlagBits stages,
                                                   const std::string& name = "");
                                                   
  /**
   * 从文件创建着色器模块（指定入口点）
   * 
   * 这个版本允许指定自定义的入口点函数名。
   * 在一个文件中包含多个着色器函数时特别有用。
   * 
   * 自定义入口点的优势：
   * 1. **代码复用**：多个相似着色器共享公共函数
   * 2. **条件编译**：根据不同入口点生成不同变体
   * 3. **参数化着色器**：一个源文件生成多种效果
   * 4. **源码组织**：更好的代码结构和管理
   * 
   * @param filePath 着色器文件路径，同上面的方法
   * @param entryPoint 入口点函数名，必须与着色器中的函数名一致
   * @param stages 着色器阶段标志，同上面的方法
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理着色器模块的生命周期
   * 
   * 使用示例：
   * ```glsl
   * // shared_lighting.frag - 共享的光照着色器文件
   * 
   * vec3 calculateLighting(vec3 albedo, vec3 normal, vec3 lightDir) {
   *     // 公共光照计算...
   * }
   * 
   * void phongMain() {   // Phong 光照模型
   *     gl_FragColor = vec4(calculateLighting(albedo, normal, lightDir), 1.0);
   * }
   * 
   * void pbrMain() {     // PBR 光照模型  
   *     // 复杂的 PBR 计算...
   *     gl_FragColor = vec4(pbrResult, 1.0);
   * }
   * ```
   * 
   * ```cpp
   * // 从同一个文件创建不同的着色器变体
   * auto phongShader = context.createShaderModule(
   *     "shaders/shared_lighting.frag",
   *     "phongMain",  // 使用 Phong 入口点
   *     VK_SHADER_STAGE_FRAGMENT_BIT,
   *     "PhongLighting"
   * );
   * 
   * auto pbrShader = context.createShaderModule(
   *     "shaders/shared_lighting.frag",
   *     "pbrMain",    // 使用 PBR 入口点
   *     VK_SHADER_STAGE_FRAGMENT_BIT,
   *     "PBRLighting"
   * );
   * ```
   */
  std::shared_ptr<ShaderModule> createShaderModule(const std::string& filePath,
                                                   const std::string& entryPoint,
                                                   VkShaderStageFlagBits stages,
                                                   const std::string& name = "");
                                                   
  /**
   * 从内存字节数组创建着色器模块
   * 
   * 这个版本直接从内存中的 SPIR-V 字节码创建着色器。
   * 适用于运行时生成或动态加载的着色器。
   * 
   * 使用场景：
   * - **嵌入式着色器**：编译到执行文件中的着色器
   * - **网络加载**：从服务器动态下载的着色器
   * - **程序化生成**：运行时生成的着色器代码
   * - **加密保护**：保护着色器源代码不被逆向工程
   * - **热重载**：开发时动态更新着色器
   * 
   * @param shader SPIR-V 字节码数组：
   *               必须是有效的 SPIR-V 二进制数据
   *               可以通过在线编译器或预编译获得
   * 
   * @param entryPoint 入口点函数名，同上面的方法
   * @param stages 着色器阶段标志，同上面的方法
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理着色器模块的生命周期
   * 
   * 使用示例：
   * ```cpp
   * // 从资源文件中加载嵌入式着色器
   * extern const std::vector<char> embedded_vertex_shader;
   * 
   * auto vertexShader = context.createShaderModule(
   *     embedded_vertex_shader,
   *     "main",
   *     VK_SHADER_STAGE_VERTEX_BIT,
   *     "EmbeddedVertex"
   * );
   * 
   * // 从网络加载动态着色器
   * std::vector<char> networkShader = downloadShaderFromServer();
   * auto dynamicShader = context.createShaderModule(
   *     networkShader,
   *     "dynamicMain",
   *     VK_SHADER_STAGE_FRAGMENT_BIT,
   *     "NetworkShader"
   * );
   * 
   * // 程序化生成的着色器
   * auto generatedCode = generateShaderCode(materialParams);
   * auto proceduralShader = context.createShaderModule(
   *     generatedCode,
   *     "generated",
   *     VK_SHADER_STAGE_FRAGMENT_BIT,
   *     "ProceduralMaterial"
   * );
   * ```
   */
  std::shared_ptr<ShaderModule> createShaderModule(const std::vector<char>& shader,
                                                   const std::string& entryPoint,
                                                   VkShaderStageFlagBits stages,
                                                   const std::string& name = "");

  // ========================================
  // 渲柕管线创建方法
  // ========================================
  
  /**
   * 创建图形渲柕管线
   * 
   * 图形管线是 Vulkan 中最复杂但也是最重要的对象之一。
   * 它定义了整个 3D 渲柕的完整流程，从顶点处理到像素输出。
   * 
   * 图形管线的主要阶段：
   * 1. **顶点输入**：定义如何读取顶点数据
   * 2. **顶点着色器**：变换 3D 坐标到屏幕坐标
   * 3. **细分阶段**：可选，增加几何体的精细度
   * 4. **几何着色器**：可选，生成新的几何体
   * 5. **光栅化**：将几何体转换为像素
   * 6. **片段着色器**：计算每个像素的最终颜色
   * 7. **深度/模板测试**：处理遭挡关系
   * 8. **颜色混合**：将新颜色与现有颜色混合
   * 
   * @param desc 管线描述结构体，包含所有配置参数：
   *             - 着色器模块列表（顶点、片段等）
   *             - 顶点输入格式和布局
   *             - 视口和裁剪区域设置
   *             - 光栅化状态（线框、填充、背面剪裁）
   *             - 深度模板测试配置
   *             - 颜色混合设置
   *             - 多采样抗锯齿配置
   * 
   * @param renderPass 渲柕通道，定义渲柕目标和操作：
   *                   传统 Vulkan 中必须提供，动态渲柕中可为 VK_NULL_HANDLE
   * 
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理管线的生命周期
   * 
   * 使用示例：
   * ```cpp
   * // 基本的 3D 渲柕管线
   * Pipeline::GraphicsPipelineDescriptor pipelineDesc{
   *     .shaderModules = {vertexShader, fragmentShader},
   *     .vertexInputFormat = {
   *         // 顶点属性：位置、法线、UV 等
   *     },
   *     .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
   *     .viewport = {0, 0, width, height, 0.0f, 1.0f},
   *     .cullMode = VK_CULL_MODE_BACK_BIT,
   *     .depthTest = true,
   *     .blendEnable = false
   * };
   * 
   * auto pipeline = context.createGraphicsPipeline(
   *     pipelineDesc,
   *     renderPass,
   *     "Main3DPipeline"
   * );
   * ```
   */
  std::shared_ptr<Pipeline> createGraphicsPipeline(
      const Pipeline::GraphicsPipelineDescriptor& desc, VkRenderPass renderPass,
      const std::string& name = "");

  /**
   * 创建计算管线
   * 
   * 计算管线用于 GPU 通用计算，不涉及传统的图形渲柕。
   * 它只包含一个计算着色器阶段，但功能非常强大。
   * 
   * 计算管线的应用：
   * - **图像处理**：滤镜、色彩校正、后处理效果
   * - **物理仿真**：粒子系统、流体仿真、布料仿真
   * - **AI 计算**：神经网络推理、机器学习
   * - **科学计算**：数值求解、数据分析
   * - **游戏逻辑**：AI 寻路、地形生成、碰撞检测
   * - **区块链**：加密货币挖矿、哈希计算
   * 
   * @param desc 计算管线描述结构体，相对简单：
   *             - 计算着色器模块
   *             - 特殊化常数（可选）
   *             - 推送常数配置（可选）
   *             - 资源绑定布局
   * 
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理管线的生命周期
   * 
   * 使用示例：
   * ```cpp
   * // 图像模糊处理管线
   * Pipeline::ComputePipelineDescriptor computeDesc{
   *     .computeShader = blurShader,
   *     .pushConstants = {
   *         .size = sizeof(BlurParams),
   *         .data = &blurParams
   *     }
   * };
   * 
   * auto blurPipeline = context.createComputePipeline(
   *     computeDesc,
   *     "GaussianBlur"
   * );
   * 
   * // 使用计算管线
   * blurPipeline->bind(commandBuffer);
   * vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);
   * ```
   */
  std::shared_ptr<Pipeline> createComputePipeline(
      const Pipeline::ComputePipelineDescriptor& desc, const std::string& name = "");

  /**
   * 创建光线追踪管线
   * 
   * 光线追踪管线是下一代图形渲柕技术的核心。
   * 它利用 GPU 硬件加速实现实时的物理准确光照计算。
   * 
   * 光线追踪的优势：
   * - **物理准确**：真实的光照、阴影和反射
   * - **全局光照**：间接光照和颜色渗透
   * - **动态环境**：实时变化的光照和物体
   * - **复杂材质**：透明、折射、体积散射
   * 
   * 光线追踪的阶段：
   * 1. **光线生成**：从相机或光源发出光线
   * 2. **相交测试**：将光线与场景几何体求交
   * 3. **材质评估**：计算光线与表面的交互
   * 4. **光线追踪**：递归或迭代地追踪反射光线
   * 5. **着色合成**：将所有贡献合成为最终颜色
   * 
   * @param desc 光线追踪管线描述结构体：
   *             - 光线生成着色器（raygen）
   *             - 相交着色器（intersection）
   *             - 最近命中着色器（closest hit）
   *             - 任意命中着色器（any hit）
   *             - 未命中着色器（miss）
   *             - 调用着色器（callable）
   * 
   * @param name 调试名称
   * 
   * @return 智能指针，自动管理管线的生命周期
   * 
   * 硬件要求：
   * - NVIDIA RTX 20系列及以上
   * - AMD RX 6000系列及以上  
   * - Intel Arc 系列
   * - 支持 VK_KHR_ray_tracing_pipeline 扩展
   * 
   * 使用示例：
   * ```cpp
   * // 实时光线追踪管线
   * Pipeline::RayTracingPipelineDescriptor rtDesc{
   *     .raygenShader = raygenShader,        // 主光线生成
   *     .missShaders = {skyboxMissShader},   // 环境光照
   *     .hitGroups = {
   *         {diffuseHitShader, nullptr},     // 漫反射材质
   *         {reflectiveHitShader, nullptr},  // 反射材质
   *         {glassHitShader, nullptr}        // 透明材质
   *     },
   *     .maxRecursionDepth = 4              // 最大反射次数
   * };
   * 
   * auto rtPipeline = context.createRayTracingPipeline(
   *     rtDesc,
   *     "RealtimeRayTracing"
   * );
   * ```
   */
  std::shared_ptr<Pipeline> createRayTracingPipeline(
      const Pipeline::RayTracingPipelineDescriptor& desc, const std::string& name = "");

  CommandQueueManager createTransferCommandQueue(uint32_t count,
                                                 uint32_t concurrentNumCommands,
                                                 const std::string& name,
                                                 int transferQueueIndex = -1);

  std::shared_ptr<RenderPass> createRenderPass(
      const std::vector<std::shared_ptr<Texture>>& attachments,
      const std::vector<VkAttachmentLoadOp>& loadOp,
      const std::vector<VkAttachmentStoreOp>& storeOp,
      const std::vector<VkImageLayout>& layout, VkPipelineBindPoint bindPoint,
      const std::vector<std::shared_ptr<Texture>>& resolveAttachments = {},
      const std::string& name = "") const;

  std::unique_ptr<Framebuffer> createFramebuffer(
      VkRenderPass renderPass,
      const std::vector<std::shared_ptr<Texture>>& colorAttachments,
      std::shared_ptr<Texture> depthAttachment,
      std::shared_ptr<Texture> stencilAttachment, const std::string& name = "") const;

  /// <summary>
  /// Exports the current internal state of VMA to a file, which can be
  /// inspected graphically. More information:
  /// https://chromium.googlesource.com/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/+/refs/tags/upstream/v2.1.0/tools/VmaDumpVis/README.md
  /// </summary>
  /// <param name="fileName">The information will be written into the the
  /// file with this name</param>
  void dumpMemoryStats(const std::string& fileName) const;

  template <typename T>
  void setVkObjectname(T handle, VkObjectType type, const std::string& name) const {
#if defined(VK_EXT_debug_utils)
    if (enabledInstanceExtensions_.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
        enabledInstanceExtensions_.end()) {
      const VkDebugUtilsObjectNameInfoEXT objectNameInfo = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
          .objectType = type,
          .objectHandle = reinterpret_cast<uint64_t>(handle),
          .pObjectName = name.c_str(),
      };
      VK_CHECK(vkSetDebugUtilsObjectNameEXT(device_, &objectNameInfo));
    }
#else
    (void)handle;
    (void)type;
    (void)name;
#endif
  }

  void beginDebugUtilsLabel(VkCommandBuffer commandBuffer, const std::string& name,
                            const glm::vec4& color) const;

  void endDebugUtilsLabel(VkCommandBuffer commandBuffer) const;

 private:
  // ========================================
  // 内部初始化和管理方法
  // ========================================
  
  /**
   * 创建内存分配器（VMA）
   * 初始化 Vulkan Memory Allocator，简化内存管理的复杂性
   */
  void createMemoryAllocator();

  /**
   * 枚举可用的 Vulkan 实例层
   * 查找系统中安装的所有验证层、调试层等
   */
  [[nodiscard]] static std::vector<std::string> enumerateInstanceLayers(
      bool printEnumerations_ = false);

  /**
   * 枚举可用的 Vulkan 实例扩展
   * 查找系统支持的所有实例级别扩展
   */
  [[nodiscard]] std::vector<std::string> enumerateInstanceExtensions();

  /**
   * 枚举系统中的所有物理设备（GPU）
   * 查找并评估所有可用的 GPU，检查其能力和支持的功能
   */
  [[nodiscard]] std::vector<PhysicalDevice> enumeratePhysicalDevices(
      const std::vector<std::string>& requestedExtensions, bool enableRayTracing) const;

  /**
   * 从多个 GPU 中选择最适合的一个
   * 基于性能、功能支持、内存大小等因素进行选择
   */
  [[nodiscard]] PhysicalDevice choosePhysicalDevice(
      std::vector<PhysicalDevice>&& devices,
      const std::vector<std::string>& deviceExtensions) const;

 private:
  // ========================================
  // Context 核心成员变量
  // ========================================
  /**
   * 应用程序信息结构体
   * 包含应用名称、版本和 Vulkan API 版本信息
   * 这些信息会被 GPU 驱动用于优化和错误报告
   */
  const VkApplicationInfo applicationInfo_ = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Modern Vulkan Cookbook",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };
  
  VkInstance instance_ = VK_NULL_HANDLE;           ///< Vulkan 实例句柄，应用程序与 Vulkan 的连接
  PhysicalDevice physicalDevice_;                  ///< 物理设备对象，封装了 GPU 的所有信息
  VkDevice device_ = VK_NULL_HANDLE;               ///< 逻辑设备句柄，用于创建所有 Vulkan 资源
  VmaAllocator allocator_ = nullptr;               ///< VMA 内存分配器，简化内存管理
  bool printEnumerations_ = false;                 ///< 是否打印调试信息的标志
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;          ///< 显示表面句柄，连接 Vulkan 和窗口系统
  std::vector<VkSurfaceFormatKHR> surfaceFormats_; ///< 支持的表面颜色格式列表
  
  // 主要队列（用于显示和基本操作）
  VkQueue presentationQueue_ = VK_NULL_HANDLE;     ///< 显示队列，用于将渲染结果显示到屏幕

  // ========================================
  // 静态功能特性配置（全局共享）
  // ========================================
  
  /**
   * 这些静态成员变量存储了所有 Context 实例共享的功能配置。
   * 它们在程序开始时通过静态方法设置，在设备创建时传递给 Vulkan。
   */
  static VkPhysicalDeviceFeatures physicalDeviceFeatures_;                    ///< Vulkan 1.0 基础功能特性
  static VkPhysicalDeviceVulkan11Features enable11Features_;                  ///< Vulkan 1.1 新增功能特性
  static VkPhysicalDeviceVulkan12Features enable12Features_;                  ///< Vulkan 1.2 新增功能特性
  static VkPhysicalDeviceVulkan13Features enable13Features_;                  ///< Vulkan 1.3 新增功能特性

  // 光线追踪相关功能特性
  static VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures_;      ///< 加速结构功能（光线追踪基础）
  static VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures_; ///< 光线追踪管线功能
  static VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures_;                      ///< 光线查询功能（在普通着色器中使用）
  
  // 其他高级功能特性
  static VkPhysicalDeviceMultiviewFeatures multiviewFeatures_;                       ///< 多视图渲柕功能（VR/AR）
  static VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures_;  ///< 片段密度映射功能
  static VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM                        ///< 高通特有的片段密度映射偏移功能
      fragmentDensityMapOffsetFeatures_;

  // ========================================
  // GPU 命令队列集合
  // ========================================
  
  /**
   * 这些队列用于高级的并行操作和异步处理。
   * 它们不包含主显示队列，可以用于额外的工作负载。
   */
  std::vector<VkQueue> graphicsQueues_;    ///< 图形队列列表，用于并行渲柕和计算
  std::vector<VkQueue> computeQueues_;     ///< 计算队列列表，用于专用的 GPU 计算任务
  std::vector<VkQueue> transferQueues_;    ///< 传输队列列表，用于高效的数据传输
  std::vector<VkQueue> sparseQueues_;      ///< 稀疏资源队列，用于稀疏纹理和缓冲区

  // ========================================
  // 高级系统组件
  // ========================================
  
  std::unique_ptr<Swapchain> swapchain_;                      ///< 交换链对象，管理显示系统
  std::unordered_set<std::string> enabledLayers_;             ///< 已启用的 Vulkan 层列表
  std::unordered_set<std::string> enabledInstanceExtensions_; ///< 已启用的实例扩展列表
  
#if defined(VK_EXT_debug_utils)
  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;       ///< 调试信息回调对象（仅在调试模式下）
#endif
};

}  // namespace VkCore

/*
 * ========================================
 * Vulkan Context 类的学习指南和系统总结
 * ========================================
 * 
 * 一、Context 类的核心作用
 * 
 * Context 类是整个 Vulkan 应用程序的“大脑”，它负责：
 * 
 * 1. **系统初始化**：
 *    - 创建和配置 Vulkan 实例
 *    - 选择和初始化 GPU 设备
 *    - 设置调试和验证功能
 * 
 * 2. **资源管理**：
 *    - 内存分配和释放
 *    - 缓冲区、纹理、采样器的创建
 *    - 着色器和管线的管理
 * 
 * 3. **命令系统**：
 *    - 命令队列和缓冲区的管理
 *    - GPU 和 CPU 之间的同步
 *    - 并行处理和优化
 * 
 * 4. **显示系统**：
 *    - 交换链的创建和管理
 *    - 与窗口系统的集成
 *    - 帧速率和同步控制
 * 
 * 二、Vulkan 的核心概念
 * 
 * 1. **实例 (Instance)**：
 *    - Vulkan 应用的全局上下文
 *    - 管理全局设置和扩展
 *    - 连接应用程序和 Vulkan 驱动
 * 
 * 2. **物理设备 (Physical Device)**：
 *    - 代表系统中的一个 GPU
 *    - 提供 GPU 的所有能力和限制信息
 *    - 用于选择最适合的 GPU
 * 
 * 3. **逻辑设备 (Logical Device)**：
 *    - 与 GPU 的逻辑连接
 *    - 所有资源创建的入口
 *    - 定义启用的功能和队列
 * 
 * 4. **命令队列 (Command Queues)**：
 *    - GPU 执行任务的通道
 *    - 分为图形、计算、传输等类型
 *    - 支持并行执行和异步处理
 * 
 * 三、Vulkan 的优势和挑战
 * 
 * **优势：**
 * - **高性能**：直接控制 GPU，最小化驱动开销
 * - **并行性**：多线程和多队列的天然支持
 * - **可预测**：显式的资源管理和同步控制
 * - **跨平台**：支持所有主流平台和 GPU 厂商
 * - **未来导向**：支持光线追踪、网格着色器等新技术
 * 
 * **挑战：**
 * - **复杂性**：API 设计复杂，学习曲线陡峭
 * - **样板代码**：初始设置需要大量代码
 * - **调试困难**：错误信息可能不够直观
 * - **兼容性**：不同 GPU 和驱动的细微差别
 * 
 * 四、现代 Vulkan 应用的最佳实践
 * 
 * 1. **动态渲柕**：
 *    - 使用 Vulkan 1.3 的动态渲柕特性
 *    - 避免传统的 RenderPass 和 Framebuffer
 *    - 提高灵活性和性能
 * 
 * 2. **资源管理**：
 *    - 使用 VMA 简化内存管理
 *    - 采用 RAII 模式自动管理生命周期
 *    - 合理使用对象池和缓存
 * 
 * 3. **并行优化**：
 *    - 多线程命令录制
 *    - 异步计算和渲柕
 *    - GPU 驱动的渲柕技术
 * 
 * 4. **调试和工具**：
 *    - 启用验证层进行开发
 *    - 使用 RenderDoc 等工具进行图形调试
 *    - 合理使用对象命名和标签
 * 
 * 五、适用场景和选型指南
 * 
 * **适合使用 Vulkan 的场景：**
 * - AAA 游戏和高端游戏引擎
 * - VR/AR 应用（对延迟敏感）
 * - 科学计算和 GPU 加速
 * - 实时光线追踪应用
 * - 高性能图形工作站
 * 
 * **不适合使用 Vulkan 的场景：**
 * - 简单的2D 游戏或工具
 * - 快速原型开发
 * - 对性能要求不高的应用
 * - 团队缺乏 Vulkan 经验
 * 
 * 六、学习路径建议
 * 
 * 1. **基础阶段**：
 *    - 理解图形管线和 GPU 架构
 *    - 掌握 Vulkan 的核心概念
 *    - 学会使用验证层和调试工具
 * 
 * 2. **实践阶段**：
 *    - 从简单的三角形渲柕开始
 *    - 逐步添加纹理、光照、阴影
 *    - 学会使用计算着色器
 * 
 * 3. **高级阶段**：
 *    - 多线程渲柕和并行优化
 *    - 光线追踪和现代渲柕技术
 *    - 性能分析和调优
 * 
 * 这个 Context 类的设计遵循了现代 C++ 和 Vulkan 的最佳实践，
 * 为初学者提供了一个相对简化但功能强大的 Vulkan 开发入口。
 */