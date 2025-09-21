/**
 * @file PhysicalDevice.h
 * @brief Vulkan物理设备管理类 - 初学者详细指南
 * @author nio
 * @date 2025/7/7
 * 
 * ## 什么是PhysicalDevice（物理设备）？
 * 
 * 在Vulkan中，PhysicalDevice代表系统中实际的GPU硬件。可以把它想象成：
 * - 一张显卡（如NVIDIA RTX 4090、AMD RX 7900 XTX）
 * - 或者集成显卡（如Intel UHD Graphics）
 * - 甚至是软件渲染器（用CPU模拟GPU）
 * 
 * ## 为什么需要PhysicalDevice？
 * 
 * 与OpenGL不同，Vulkan不会自动选择GPU。应用程序必须：
 * 1. **枚举所有可用的GPU**：系统可能有多个GPU（独显+集显）
 * 2. **查询GPU能力**：了解GPU支持哪些功能和扩展
 * 3. **选择合适的GPU**：根据需求选择最适合的硬件
 * 4. **配置队列族**：确定如何使用GPU的不同执行单元
 * 
 * ## 核心概念解释
 * 
 * ### 1. 队列族（Queue Families）
 * GPU内部有不同的执行单元，每个单元专门处理特定类型的任务：
 * - **Graphics队列**：处理3D渲染、几何处理
 * - **Compute队列**：处理计算着色器、通用计算
 * - **Transfer队列**：处理数据传输（CPU↔GPU）
 * - **Sparse队列**：处理稀疏资源（高级特性）
 * - **Present队列**：将渲染结果显示到屏幕
 * 
 * ### 2. 设备扩展（Extensions）
 * GPU可能支持额外的功能，通过扩展提供：
 * - **VK_KHR_swapchain**：交换链支持（必需，用于显示）
 * - **VK_KHR_ray_tracing_pipeline**：光线追踪支持
 * - **VK_EXT_fragment_density_map**：可变分辨率着色
 * - **VK_NV_mesh_shader**：网格着色器（NVIDIA专用）
 * 
 * ### 3. 设备特性（Features）
 * GPU支持的具体功能开关：
 * - **几何着色器**：支持几何着色器阶段
 * - **曲面细分**：支持曲面细分着色器
 * - **多重采样**：支持抗锯齿渲染
 * - **各向异性过滤**：支持高质量纹理过滤
 * 
 * ### 4. 设备属性（Properties）
 * GPU的硬件限制和性能参数：
 * - **最大纹理尺寸**：支持的最大纹理分辨率
 * - **最大uniform缓冲大小**：着色器常量缓冲的大小限制
 * - **计算工作组大小**：计算着色器的并行度限制
 * - **内存堆信息**：GPU显存的类型和大小
 * 
 * ## 使用流程
 * 
 * ### 典型的设备选择流程：
 * ```cpp
 * // 1. 枚举所有物理设备
 * uint32_t deviceCount;
 * vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
 * std::vector<VkPhysicalDevice> devices(deviceCount);
 * vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
 * 
 * // 2. 评估每个设备的适用性
 * for (auto device : devices) {
 *     PhysicalDevice physicalDevice(device, surface, requiredExtensions);
 *     
 *     // 检查是否支持所需功能
 *     if (physicalDevice.graphicsFamilyIndex() && 
 *         physicalDevice.presentationFamilyIndex()) {
 *         // 找到合适的设备，可以使用
 *         selectedDevice = std::move(physicalDevice);
 *         break;
 *     }
 * }
 * 
 * // 3. 使用选定的设备创建逻辑设备
 * VkDevice logicalDevice = createLogicalDevice(selectedDevice);
 * ```
 * 
 * ## 本类的设计目标
 * 
 * ### 1. 简化设备查询
 * - 自动查询所有设备属性、特性、扩展
 * - 提供易用的接口检查设备能力
 * - 隐藏复杂的Vulkan结构体操作
 * 
 * ### 2. 智能队列管理
 * - 自动查找最佳的队列族配置
 * - 支持专用队列和共享队列的选择
 * - 优化队列分配策略
 * 
 * ### 3. 扩展管理
 * - 验证请求的扩展是否可用
 * - 管理扩展之间的依赖关系
 * - 提供扩展功能的便捷查询
 * 
 * @note 本类只负责查询和管理物理设备信息，不创建GPU资源
 * @warning 在创建逻辑设备之前，必须先选择合适的物理设备
 */

#pragma once

#include <list>              // 链表容器
#include <optional>          // 可选值类型
#include <string>            // 字符串类型
#include <unordered_set>     // 无序集合
#include <vector>            // 动态数组

#include "Common.h"          // Vulkan通用定义
#include "Utils.h"           // 实用工具函数

namespace VkCore {

    /**
     * @class PhysicalDevice
     * @brief Vulkan物理设备管理类 - 初学者完整指南
     * 
     * ## 类功能概述
     * 
     * PhysicalDevice类封装了Vulkan物理设备的所有查询和管理功能。它就像是GPU的"说明书"，
     * 详细记录了GPU的所有能力、限制和特性。
     * 
     * ## 主要职责
     * 
     * ### 1. 设备能力查询
     * - 查询GPU支持的功能特性（Features）
     * - 获取GPU的硬件属性和限制（Properties）
     * - 检查可用的设备扩展（Extensions）
     * - 获取内存配置信息
     * 
     * ### 2. 队列族管理
     * - 自动发现所有可用的队列族
     * - 智能分配队列资源
     * - 优化队列族的使用策略
     * - 支持多种队列类型的并发使用
     * 
     * ### 3. 表面兼容性
     * - 检查设备与显示表面的兼容性
     * - 查询支持的表面格式和呈现模式
     * - 获取交换链相关的能力信息
     * 
     * ### 4. 高级特性支持
     * - 光线追踪功能检测和配置
     * - 多视图渲染支持检查
     * - 片段密度图功能验证
     * - 网格着色器等现代GPU特性
     * 
     * ## 设计特点
     * 
     * ### 1. 自动化查询
     * 构造时自动查询所有相关信息，避免重复的API调用：
     * - 一次性获取所有设备属性
     * - 自动构建特性链表
     * - 批量验证扩展可用性
     * 
     * ### 2. 类型安全
     * 使用强类型和现代C++特性：
     * - std::optional处理可能不存在的队列族
     * - const成员函数保证查询操作的安全性
     * - RAII管理资源生命周期
     * 
     * ### 3. 性能优化
     * - 缓存查询结果避免重复API调用
     * - 智能队列分配策略
     * - 最小化状态变更
     * 
     * ## 使用模式
     * 
     * ### 基础设备选择
     * ```cpp
     * // 创建物理设备包装器
     * std::vector<std::string> requiredExtensions = {
     *     VK_KHR_SWAPCHAIN_EXTENSION_NAME
     * };
     * 
     * PhysicalDevice physicalDevice(vkPhysicalDevice, surface, 
     *                               requiredExtensions, true);
     * 
     * // 检查基本渲染能力
     * if (physicalDevice.graphicsFamilyIndex() && 
     *     physicalDevice.presentationFamilyIndex()) {
     *     // 设备适用于图形渲染
     * }
     * ```
     * 
     * ### 高级特性检测
     * ```cpp
     * PhysicalDevice device(vkDevice, surface, extensions, false, true);
     * 
     * if (device.isRayTracingSupported()) {
     *     // 启用光线追踪渲染管线
     *     auto rtProperties = device.rayTracingProperties();
     *     // 配置光线追踪参数...
     * }
     * 
     * if (device.isMultiviewSupported()) {
     *     // 启用VR多视图渲染
     * }
     * ```
     * 
     * @note 使用final关键字防止继承，确保设计的完整性
     * @warning 物理设备对象不拥有VkPhysicalDevice句柄，不负责其生命周期
     */
    class PhysicalDevice final {
    public:
        /**
         * @brief 默认构造函数 - 创建空的物理设备对象
         * 
         * 创建一个未初始化的物理设备对象。这个构造函数主要用于：
         * - 容器中的占位符对象
         * - 延迟初始化的场景
         * - 移动语义的实现
         * 
         * @note 默认构造的对象在使用前必须重新赋值
         * @warning 未初始化的对象调用成员函数会导致未定义行为
         */
        explicit PhysicalDevice(){};

        /**
         * @brief 主构造函数 - 初始化物理设备并查询所有能力
         * 
         * 这是PhysicalDevice类的核心构造函数，它会执行完整的设备能力查询和配置。
         * 整个过程包括设备属性查询、特性检测、扩展验证、队列族发现等。
         * 
         * ## 执行流程：
         * 1. **保存设备句柄**：存储VkPhysicalDevice引用
         * 2. **查询设备属性**：获取GPU的硬件参数和限制
         * 3. **检测设备特性**：查询支持的功能开关
         * 4. **验证扩展支持**：检查请求的扩展是否可用
         * 5. **发现队列族**：枚举所有可用的队列族及其能力
         * 6. **配置表面支持**：查询与显示表面的兼容性
         * 7. **初始化高级特性**：配置光线追踪、多视图等功能
         * 
         * ## 队列族自动配置：
         * 构造函数会自动查找最优的队列族配置：
         * - **专用队列优先**：优先使用专门的Transfer/Compute队列
         * - **回退策略**：如果没有专用队列，使用Graphics队列
         * - **呈现兼容性**：确保找到支持呈现的队列族
         * 
         * ## 扩展验证：
         * - 检查每个请求的扩展是否被设备支持
         * - 构建已启用扩展的集合
         * - 处理扩展之间的依赖关系
         * 
         * ## 调试输出：
         * 当printEnumerations为true时，会输出详细的设备信息：
         * - 设备名称和驱动版本
         * - 支持的扩展列表
         * - 队列族配置
         * - 内存堆信息
         * 
         * @param device Vulkan物理设备句柄，代表具体的GPU硬件
         * @param surface 显示表面句柄，用于查询呈现兼容性（可以是VK_NULL_HANDLE）
         * @param requestedExtensions 应用程序请求的扩展列表
         * @param printEnumerations 是否打印详细的设备枚举信息（调试用）
         * @param enableRayTracing 是否启用光线追踪相关功能检测
         * 
         * @note 构造过程中会进行大量的Vulkan API调用，可能比较耗时
         * @warning 如果设备不支持请求的关键扩展，构造可能失败
         * 
         * ## 使用示例：
         * ```cpp
         * // 基础图形应用
         * std::vector<std::string> basicExtensions = {
         *     VK_KHR_SWAPCHAIN_EXTENSION_NAME
         * };
         * PhysicalDevice basicDevice(vkDevice, surface, basicExtensions);
         * 
         * // 光线追踪应用
         * std::vector<std::string> rtExtensions = {
         *     VK_KHR_SWAPCHAIN_EXTENSION_NAME,
         *     VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
         *     VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
         * };
         * PhysicalDevice rtDevice(vkDevice, surface, rtExtensions, true, true);
         * ```
         */
        explicit PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                                const std::vector<std::string>& requestedExtensions,
                                bool printEnumerations = false, bool enableRayTracing = false);

        /**
         * @brief 获取底层VkPhysicalDevice句柄
         * 
         * 返回原生的Vulkan物理设备句柄，用于直接与Vulkan API交互。
         * 这个函数提供了对底层Vulkan对象的直接访问。
         * 
         * ## 常见用途：
         * - 创建逻辑设备时需要指定物理设备
         * - 查询设备特定的扩展功能
         * - 与其他Vulkan API函数交互
         * 
         * @return VkPhysicalDevice 原生Vulkan物理设备句柄
         * 
         * @note [[nodiscard]]属性提醒调用者不要忽略返回值
         * @warning 返回的句柄不由此对象拥有，不要尝试销毁它
         */
        [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;

        /**
         * @brief 获取设备支持的扩展列表
         * 
         * 返回此物理设备支持的所有Vulkan扩展名称。这些扩展提供了
         * 超出Vulkan核心规范的额外功能。
         * 
         * ## 扩展类型示例：
         * - **VK_KHR_swapchain**：交换链支持（显示到屏幕）
         * - **VK_KHR_ray_tracing_pipeline**：光线追踪管线
         * - **VK_EXT_fragment_density_map**：可变分辨率着色
         * - **VK_NV_mesh_shader**：网格着色器（NVIDIA专用）
         * 
         * ## 使用场景：
         * ```cpp
         * auto& extensions = physicalDevice.extensions();
         * bool hasRayTracing = std::find(extensions.begin(), extensions.end(),
         *     VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) != extensions.end();
         * ```
         * 
         * @return const std::vector<std::string>& 设备支持的扩展名称列表
         * 
         * @note 返回的是引用，避免不必要的复制
         * @note 扩展列表在构造时确定，运行期间不会改变
         */
        [[nodiscard]] const std::vector<std::string>& extensions() const;

        /**
         * @brief 预留队列资源 - 高级队列管理功能
         * 
         * 根据指定的队列类型和表面需求，智能分配和预留队列资源。
         * 这个函数会优化队列族的使用，避免资源浪费。
         * 
         * ## 队列类型说明：
         * - **VK_QUEUE_GRAPHICS_BIT**：图形渲染队列
         * - **VK_QUEUE_COMPUTE_BIT**：计算着色器队列
         * - **VK_QUEUE_TRANSFER_BIT**：数据传输队列
         * - **VK_QUEUE_SPARSE_BINDING_BIT**：稀疏资源绑定队列
         * 
         * ## 智能分配策略：
         * 1. **专用队列优先**：如果存在专用的Transfer或Compute队列，优先使用
         * 2. **共享队列回退**：如果没有专用队列，使用Graphics队列（通常支持所有操作）
         * 3. **呈现兼容性**：如果指定了surface，确保分配支持呈现的队列
         * 
         * ## 使用示例：
         * ```cpp
         * // 为图形渲染和呈现预留队列
         * physicalDevice.reserveQueues(VK_QUEUE_GRAPHICS_BIT, surface);
         * 
         * // 为异步计算预留专用队列
         * physicalDevice.reserveQueues(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, 
         *                              VK_NULL_HANDLE);
         * ```
         * 
         * @param requestedQueueTypes 请求的队列类型标志位组合
         * @param surface 显示表面句柄，VK_NULL_HANDLE表示不需要呈现支持
         * 
         * @note surface可以是VK_NULL_HANDLE，适用于离屏渲染场景
         * @warning 必须在创建逻辑设备之前调用此函数
         */
        // surface may be VK_NULL_HANDLE, as we may be rendering offscreen
        void reserveQueues(VkQueueFlags requestedQueueTypes, VkSurfaceKHR surface);

        /**
         * @brief 获取队列族索引和数量配对
         * 
         * 返回所有已配置队列族的索引和对应的队列数量。这个信息用于
         * 创建逻辑设备时指定队列创建参数。
         * 
         * ## 返回格式：
         * 每个pair包含：
         * - **first**：队列族索引（用于VkDeviceQueueCreateInfo.queueFamilyIndex）
         * - **second**：该队列族中要创建的队列数量
         * 
         * ## 使用场景：
         * ```cpp
         * auto queueConfigs = physicalDevice.queueFamilyIndexAndCount();
         * std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
         * 
         * for (auto& [familyIndex, queueCount] : queueConfigs) {
         *     VkDeviceQueueCreateInfo queueCreateInfo = {
         *         .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         *         .queueFamilyIndex = familyIndex,
         *         .queueCount = queueCount,
         *         .pQueuePriorities = priorities
         *     };
         *     queueCreateInfos.push_back(queueCreateInfo);
         * }
         * ```
         * 
         * @return std::vector<std::pair<uint32_t, uint32_t>> 队列族索引和数量的配对列表
         * 
         * @note 必须在调用reserveQueues()之后调用
         * @note 返回的配置直接用于VkDeviceCreateInfo
         */
        [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> queueFamilyIndexAndCount()
        const;

        /**
         * @brief 获取图形队列族索引
         * 
         * 返回支持图形操作的队列族索引。图形队列是GPU中最重要的队列类型，
         * 负责处理所有的3D渲染、2D绘制和几何处理任务。
         * 
         * ## 图形队列的功能：
         * - **顶点处理**：顶点着色器的执行
         * - **几何处理**：几何着色器、曲面细分
         * - **光栅化**：将几何图形转换为像素
         * - **片段着色**：像素着色器的执行
         * - **渲染通道**：执行渲染通道和子通道
         * 
         * @return std::optional<uint32_t> 图形队列族索引，如果不存在则为std::nullopt
         * 
         * @note 几乎所有GPU都支持图形队列，返回nullopt的情况极其罕见
         * @note 图形队列通常也支持计算和传输操作
         */
        [[nodiscard]] std::optional<uint32_t> graphicsFamilyIndex() const;

        /**
         * @brief 获取计算队列族索引
         * 
         * 返回支持计算操作的队列族索引。计算队列专门用于执行计算着色器，
         * 进行通用GPU计算（GPGPU）任务。
         * 
         * ## 计算队列的用途：
         * - **计算着色器**：并行数值计算
         * - **后处理效果**：图像滤镜、模糊、锐化
         * - **物理模拟**：粒子系统、流体模拟
         * - **机器学习**：神经网络推理
         * - **数据处理**：大规模并行数据变换
         * 
         * ## 队列类型说明：
         * - **专用计算队列**：只支持计算，可以与图形队列并行工作
         * - **共享队列**：图形队列通常也支持计算操作
         * 
         * @return std::optional<uint32_t> 计算队列族索引，如果不存在则为std::nullopt
         * 
         * @note 现代GPU通常都支持计算队列
         * @note 专用计算队列可以与图形渲染并行执行
         */
        [[nodiscard]] std::optional<uint32_t> computeFamilyIndex() const;

        /**
         * @brief 获取传输队列族索引
         * 
         * 返回支持数据传输操作的队列族索引。传输队列专门用于在不同
         * 内存区域之间移动数据，是GPU数据管理的重要组件。
         * 
         * ## 传输队列的功能：
         * - **缓冲区复制**：在不同Buffer之间复制数据
         * - **图像复制**：复制纹理和图像数据
         * - **内存传输**：CPU到GPU、GPU到GPU的数据传输
         * - **Mipmap生成**：纹理细节级别的生成
         * - **格式转换**：不同数据格式之间的转换
         * 
         * ## 性能优势：
         * 专用传输队列可以与图形/计算队列并行工作：
         * ```cpp
         * // 并行执行：传输队列加载纹理，图形队列渲染前一帧
         * transferQueue.copyBuffer(stagingBuffer, textureBuffer);
         * graphicsQueue.renderFrame();  // 同时执行
         * ```
         * 
         * @return std::optional<uint32_t> 传输队列族索引，如果不存在则为std::nullopt
         * 
         * @note 如果没有专用传输队列，可以使用图形队列进行传输操作
         * @note 专用传输队列在数据密集型应用中可以显著提升性能
         */
        [[nodiscard]] std::optional<uint32_t> transferFamilyIndex() const;

        /**
         * @brief 获取稀疏资源队列族索引
         * 
         * 返回支持稀疏资源绑定的队列族索引。稀疏资源是Vulkan的高级特性，
         * 允许创建比实际分配内存更大的资源，按需绑定内存页面。
         * 
         * ## 稀疏资源的应用：
         * - **虚拟纹理**：超大纹理的流式加载
         * - **地形渲染**：大规模地形的内存优化
         * - **体积渲染**：3D体积数据的高效处理
         * - **稀疏体素**：体素化场景的内存管理
         * 
         * ## 技术原理：
         * 稀疏资源允许：
         * 1. 创建大型虚拟资源（如8K x 8K纹理）
         * 2. 只为实际使用的部分分配物理内存
         * 3. 运行时动态绑定/解绑内存页面
         * 4. 实现流式资源加载
         * 
         * @return std::optional<uint32_t> 稀疏资源队列族索引，如果不支持则为std::nullopt
         * 
         * @note 稀疏资源是可选特性，不是所有GPU都支持
         * @warning 稀疏资源的使用需要特殊的内存管理策略
         */
        [[nodiscard]] std::optional<uint32_t> sparseFamilyIndex() const;

        /**
         * @brief 获取呈现队列族索引
         * 
         * 返回支持呈现操作的队列族索引。呈现队列负责将渲染结果显示到屏幕上，
         * 是图形应用与显示系统之间的桥梁。
         * 
         * ## 呈现队列的职责：
         * - **交换链呈现**：将渲染结果提交给交换链
         * - **垂直同步**：与显示器刷新率同步
         * - **多显示器支持**：处理多显示器配置
         * - **全屏切换**：处理窗口和全屏模式切换
         * 
         * ## 重要概念：
         * 呈现队列必须与特定的显示表面兼容：
         * - 不同的表面可能需要不同的队列族
         * - 某些队列族可能不支持特定的显示器
         * - 呈现能力与GPU驱动和显示系统相关
         * 
         * ## 性能考虑：
         * ```cpp
         * // 理想情况：图形和呈现使用同一队列族（减少同步开销）
         * if (graphicsFamilyIndex() == presentationFamilyIndex()) {
         *     // 最优配置，无需队列间同步
         * }
         * ```
         * 
         * @return std::optional<uint32_t> 呈现队列族索引，如果不支持则为std::nullopt
         * 
         * @note 呈现支持取决于显示表面和GPU驱动
         * @warning 离屏渲染应用可能不需要呈现队列
         */
        [[nodiscard]] std::optional<uint32_t> presentationFamilyIndex() const;

        /**
         * @brief 获取图形队列族中的队列数量
         * @return uint32_t 图形队列族中可用的队列数量
         * @note 多个队列可以并行处理不同的渲染任务
         */
        [[nodiscard]] uint32_t graphicsFamilyCount() const { return graphicsQueueCount_; }

        /**
         * @brief 获取计算队列族中的队列数量
         * @return uint32_t 计算队列族中可用的队列数量
         * @note 多个计算队列可以并行执行不同的计算任务
         */
        [[nodiscard]] uint32_t computeFamilyCount() const { return computeQueueCount_; }

        /**
         * @brief 获取传输队列族中的队列数量
         * @return uint32_t 传输队列族中可用的队列数量
         * @note 多个传输队列可以并行处理数据传输任务
         */
        [[nodiscard]] uint32_t transferFamilyCount() const { return transferQueueCount_; }

        /**
         * @brief 获取稀疏资源队列族中的队列数量
         * @return uint32_t 稀疏资源队列族中可用的队列数量
         * @note 稀疏资源操作通常不需要多个队列
         */
        [[nodiscard]] uint32_t sparseFamilyCount() const { return sparseQueueCount_; }

        /**
         * @brief 获取呈现队列族中的队列数量
         * @return uint32_t 呈现队列族中可用的队列数量
         * @note 通常一个呈现队列就足够了
         */
        [[nodiscard]] uint32_t presentationFamilyCount() const {
            return presentationQueueCount_;
        }

        /**
         * @brief 获取表面能力信息
         * 
         * 返回GPU与指定显示表面的兼容性信息，包括交换链的各种限制和能力。
         * 这些信息用于创建和配置交换链。
         * 
         * ## 包含的信息：
         * - **图像数量限制**：交换链中最少/最多图像数量
         * - **图像尺寸限制**：支持的最小/最大分辨率
         * - **变换能力**：支持的图像旋转和翻转
         * - **合成模式**：与窗口系统的混合模式
         * - **用法标志**：图像可以用于哪些操作
         * 
         * @return const VkSurfaceCapabilitiesKHR& 表面能力信息的常引用
         * 
         * @note 这些信息在构造时查询并缓存
         * @warning 如果构造时没有提供表面，此信息可能无效
         */
        const VkSurfaceCapabilitiesKHR& surfaceCapabilities() const;

        /**
         * @brief 获取设备功能特性
         * 
         * 返回GPU支持的所有功能特性。特性是可以启用或禁用的功能开关，
         * 控制着GPU的各种高级功能。
         * 
         * ## 特性类型包括：
         * - **基础特性**：几何着色器、曲面细分、多重采样等
         * - **Vulkan 1.2特性**：时间线信号量、缓冲设备地址等
         * - **光线追踪特性**：光线追踪管线、加速结构等
         * - **扩展特性**：各种Vulkan扩展提供的功能
         * 
         * ## 使用示例：
         * ```cpp
         * auto& features = physicalDevice.features();
         * if (features.features.geometryShader) {
         *     // 可以使用几何着色器
         * }
         * ```
         * 
         * @return const VkPhysicalDeviceFeatures2& 设备特性信息的常引用
         * 
         * @note VkPhysicalDeviceFeatures2包含特性链，支持扩展特性
         */
        const VkPhysicalDeviceFeatures2& features() const { return features_; }

        /**
         * @brief 获取设备属性信息
         * 
         * 返回GPU的硬件属性和各种限制参数。这些属性描述了GPU的能力上限
         * 和硬件特征，用于优化应用程序的资源使用。
         * 
         * ## 属性类型包括：
         * - **基础属性**：设备名称、驱动版本、设备类型
         * - **限制参数**：最大纹理尺寸、最大uniform缓冲大小
         * - **内存属性**：显存大小、内存类型
         * - **计算属性**：工作组大小、本地内存大小
         * - **扩展属性**：光线追踪、片段密度图等扩展的属性
         * 
         * ## 使用示例：
         * ```cpp
         * auto& props = physicalDevice.properties();
         * uint32_t maxTextureSize = props.properties.limits.maxImageDimension2D;
         * std::string deviceName = props.properties.deviceName;
         * ```
         * 
         * @return const VkPhysicalDeviceProperties2& 设备属性信息的常引用
         * 
         * @note VkPhysicalDeviceProperties2包含属性链，支持扩展属性
         */
        const VkPhysicalDeviceProperties2& properties() const { return properties_; }

        /**
         * @brief 获取已启用的扩展集合
         * 
         * 返回构造时成功验证并启用的扩展名称集合。这些扩展在创建
         * 逻辑设备时会被启用。
         * 
         * ## 扩展验证过程：
         * 1. 检查请求的扩展是否被设备支持
         * 2. 验证扩展之间的依赖关系
         * 3. 构建最终的启用扩展集合
         * 
         * ## 使用场景：
         * ```cpp
         * auto& enabled = physicalDevice.enabledExtensions();
         * bool hasSwapchain = enabled.count(VK_KHR_SWAPCHAIN_EXTENSION_NAME) > 0;
         * ```
         * 
         * @return const std::unordered_set<std::string>& 已启用扩展名称的集合
         * 
         * @note 使用unordered_set提供O(1)的查找性能
         */
        const std::unordered_set<std::string>& enabledExtensions() const {
            return enabledExtensions_;
        }

        /**
         * @brief 检查是否支持光线追踪
         * 
         * 检查GPU是否完全支持Vulkan光线追踪功能。光线追踪需要三个核心组件
         * 的同时支持才能正常工作。
         * 
         * ## 光线追踪组件：
         * - **加速结构**：用于加速光线-几何体相交测试的数据结构
         * - **光线追踪管线**：专门的着色器管线，支持光线生成、相交、命中着色器
         * - **光线查询**：在任意着色器中进行光线追踪查询的能力
         * 
         * ## 应用场景：
         * - **实时光线追踪**：反射、阴影、全局光照
         * - **混合渲染**：传统光栅化+光线追踪
         * - **离线渲染**：高质量预计算光照
         * 
         * @return bool 如果完全支持光线追踪则返回true
         * 
         * @note 光线追踪需要现代GPU（RTX 20系列以上，RDNA2以上）
         * @warning 即使硬件支持，也需要相应的驱动程序支持
         */
        bool isRayTracingSupported() const {
            return (accelStructFeature_.accelerationStructure &&
                    rayTracingFeature_.rayTracingPipeline && rayQueryFeature_.rayQuery);
        }

        /**
         * @brief 获取光线追踪属性
         * 
         * 返回GPU光线追踪功能的详细属性和限制参数。这些参数用于
         * 优化光线追踪应用程序的性能。
         * 
         * ## 包含的属性：
         * - **着色器组句柄大小**：光线追踪着色器的句柄大小
         * - **递归深度限制**：光线追踪的最大递归层数
         * - **几何体实例限制**：加速结构中的最大实例数量
         * - **内存对齐要求**：各种缓冲区的对齐要求
         * 
         * @return VkPhysicalDeviceRayTracingPipelinePropertiesKHR 光线追踪属性
         * 
         * @note 只有在isRayTracingSupported()返回true时才有效
         */
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties() const {
            return rayTracingPipelineProperties_;
        }

        /**
         * @brief 获取片段密度图属性
         * 
         * 返回片段密度图功能的属性信息。片段密度图是一种可变分辨率着色技术，
         * 可以在不同区域使用不同的着色密度，提升渲染性能。
         * 
         * ## 片段密度图的优势：
         * - **性能优化**：在不重要的区域减少着色开销
         * - **VR优化**：模拟人眼的中心凹视觉特性
         * - **动态调整**：根据内容复杂度调整着色密度
         * 
         * @return const VkPhysicalDeviceFragmentDensityMapPropertiesEXT& 片段密度图属性
         * 
         * @note 只有在isFragmentDensityMapSupported()返回true时才有效
         */
        const VkPhysicalDeviceFragmentDensityMapPropertiesEXT& fragmentDensityMapProperties()
        const {
            return fragmentDensityMapProperties_;
        }

        /**
         * @brief 获取片段密度图偏移属性（高通专用）
         * 
         * 返回高通GPU特有的片段密度图偏移功能属性。这是高通Adreno GPU
         * 的专有扩展，提供额外的片段密度图优化。
         * 
         * @return const VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM& 高通片段密度图偏移属性
         * 
         * @note 这是高通Adreno GPU的专有功能
         * @warning 只在支持VK_QCOM_fragment_density_map_offset扩展的设备上有效
         */
        const VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM&
        fragmentDensityMapOffsetProperties() const {
            return fragmentDensityMapOffsetProperties_;
        }

        /**
         * @brief 获取支持的呈现模式列表
         * 
         * 返回GPU与显示表面支持的所有呈现模式。呈现模式控制着图像如何
         * 从交换链显示到屏幕上。
         * 
         * ## 常见的呈现模式：
         * - **VK_PRESENT_MODE_IMMEDIATE_KHR**：立即呈现，可能撕裂
         * - **VK_PRESENT_MODE_FIFO_KHR**：垂直同步，保证不撕裂
         * - **VK_PRESENT_MODE_FIFO_RELAXED_KHR**：自适应垂直同步
         * - **VK_PRESENT_MODE_MAILBOX_KHR**：三重缓冲，低延迟
         * 
         * ## 选择策略：
         * ```cpp
         * auto& modes = physicalDevice.presentModes();
         * // 优先选择mailbox模式（低延迟）
         * if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
         *     selectedMode = VK_PRESENT_MODE_MAILBOX_KHR;
         * }
         * ```
         * 
         * @return const std::vector<VkPresentModeKHR>& 支持的呈现模式列表
         * 
         * @note FIFO模式是Vulkan规范要求必须支持的
         */
        const std::vector<VkPresentModeKHR>& presentModes() const { return presentModes_; }

        /**
         * @brief 检查是否支持多视图渲染
         * 
         * 检查GPU是否支持多视图渲染功能。多视图渲染允许在单次渲染过程中
         * 生成多个视角的图像，主要用于VR和立体渲染。
         * 
         * ## 多视图渲染的应用：
         * - **VR渲染**：同时渲染左右眼图像
         * - **立体显示**：生成立体视觉效果
         * - **多显示器**：同时渲染到多个显示器
         * - **阴影贴图**：同时生成多个方向的阴影
         * 
         * ## 性能优势：
         * - 减少绘制调用次数
         * - 共享几何处理阶段
         * - 优化GPU资源利用率
         * 
         * @return bool 如果支持多视图渲染则返回true
         * 
         * @note 多视图渲染是Vulkan 1.1的核心功能
         */
        bool isMultiviewSupported() const { return multiviewFeature_.multiview; }

        /**
         * @brief 检查是否支持片段密度图
         * 
         * 检查GPU是否支持片段密度图功能。这是一种可变分辨率着色技术，
         * 可以根据图像内容的重要性调整着色密度。
         * 
         * ## 片段密度图的工作原理：
         * 1. 创建一个小尺寸的密度图纹理
         * 2. 密度图的每个像素控制对应区域的着色密度
         * 3. GPU根据密度图自动调整着色频率
         * 4. 在不重要的区域减少着色开销
         * 
         * @return bool 如果支持片段密度图则返回true
         * 
         * @note 这是VK_EXT_fragment_density_map扩展提供的功能
         * @note 主要在移动GPU和VR应用中使用
         */
        bool isFragmentDensityMapSupported() const {
            return fragmentDensityMapFeature_.fragmentDensityMap == VK_TRUE;
        }

        /**
         * @brief 检查是否支持片段密度图偏移（高通专用）
         * 
         * 检查是否支持高通GPU特有的片段密度图偏移功能。这是对标准
         * 片段密度图功能的增强，提供更精细的控制。
         * 
         * @return bool 如果支持片段密度图偏移则返回true
         * 
         * @note 这是高通Adreno GPU的专有功能
         * @warning 只在高通芯片组上可用
         */
        bool isFragmentDensityMapOffsetSupported() const {
            return fragmentDensityMapOffsetFeature_.fragmentDensityMapOffset == VK_TRUE;
        }

    private:
        /**
         * @brief 枚举表面支持的像素格式
         * 
         * 查询指定表面支持的所有像素格式和颜色空间组合。
         * 这些信息用于创建兼容的交换链。
         * 
         * @param surface 显示表面句柄
         * 
         * @note 在构造函数中自动调用
         */
        void enumerateSurfaceFormats(VkSurfaceKHR surface);

        /**
         * @brief 枚举表面能力信息
         * 
         * 查询GPU与指定表面的兼容性信息，包括支持的图像尺寸、
         * 数量限制、变换能力等。
         * 
         * @param surface 显示表面句柄
         * 
         * @note 在构造函数中自动调用
         */
        void enumerateSurfaceCapabilities(VkSurfaceKHR surface);

        /**
         * @brief 枚举支持的呈现模式
         * 
         * 查询GPU与表面支持的所有呈现模式，用于控制图像的
         * 显示时机和同步方式。
         * 
         * @param surface 显示表面句柄
         * 
         * @note 在构造函数中自动调用
         */
        void enumeratePresentationModes(VkSurfaceKHR surface);

    private:
        // === 核心设备信息 ===
        /**
         * @brief Vulkan物理设备句柄
         * 
         * 指向实际GPU硬件的Vulkan句柄。这个句柄由Vulkan实例提供，
         * 代表系统中的一个具体GPU设备。
         * 
         * @note 此对象不拥有句柄的生命周期，不负责销毁
         */
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

        /**
         * @brief 设备支持的扩展名称列表
         * 
         * 包含此GPU支持的所有Vulkan扩展名称。扩展提供超出核心规范的额外功能，
         * 如光线追踪、可变分辨率着色等。
         * 
         * @note 在构造时查询并缓存，运行期间不会改变
         */
        std::vector<std::string> extensions_;

        // === 设备属性链（Properties Chain） ===
        // 使用链式结构查询各种扩展的属性信息
        // 链的顺序：Properties2 -> RayTracing -> FragmentDensityMap -> FragmentDensityMapOffset

        /**
         * @brief 高通片段密度图偏移属性（链的末端）
         * 
         * 高通Adreno GPU特有的片段密度图偏移功能属性。
         * 这是属性链的最后一个节点（pNext = nullptr）。
         * 
         * @note 只在高通芯片组上有意义
         */
        VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM
                fragmentDensityMapOffsetProperties_{
                .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM,
                .pNext = nullptr,  // 链的末端
        };

        /**
         * @brief 片段密度图属性
         * 
         * 可变分辨率着色（VRS）功能的属性信息，包括密度图的尺寸限制、
         * 支持的密度级别等。链接到高通偏移属性。
         * 
         * @note VK_EXT_fragment_density_map扩展提供
         */
        VkPhysicalDeviceFragmentDensityMapPropertiesEXT fragmentDensityMapProperties_{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT,
                .pNext = &fragmentDensityMapOffsetProperties_,  // 链接到下一个属性结构
        };

        /**
         * @brief 光线追踪管线属性
         * 
         * 光线追踪功能的详细属性信息，包括着色器组句柄大小、递归深度限制、
         * 各种缓冲区的对齐要求等。链接到片段密度图属性。
         * 
         * @note VK_KHR_ray_tracing_pipeline扩展提供
         */
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties_{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
                .pNext = &fragmentDensityMapProperties_,  // 链接到下一个属性结构
        };

        /**
         * @brief 设备属性主结构（链的头部）
         * 
         * Vulkan设备的基础属性信息，包括设备名称、驱动版本、硬件限制等。
         * 这是属性链的头部，通过pNext链接到各种扩展属性。
         * 
         * @note 这是查询设备属性时使用的主要结构
         */
        VkPhysicalDeviceProperties2 properties_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &rayTracingPipelineProperties_,  // 链接到扩展属性链
        };

        // === 设备特性链（Features Chain） ===
        // 使用链式结构查询各种扩展的特性信息
        // 链的顺序：Features2 -> Vulkan12 -> BufferDeviceAddress -> DescriptorIndexing -> 
        //          AccelerationStructure -> RayTracingPipeline -> RayQuery -> MeshShader -> 
        //          TimelineSemaphore -> Multiview -> FragmentDensityMap -> FragmentDensityMapOffset

        /**
         * @brief 高通片段密度图偏移特性（链的末端）
         * 
         * 高通Adreno GPU特有的片段密度图偏移功能特性开关。
         * 这是特性链的最后一个节点（pNext = nullptr）。
         */
        VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM fragmentDensityMapOffsetFeature_ =
                {
                        .sType =
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
                        .pNext = nullptr,  // 链的末端
                };

        /**
         * @brief 片段密度图特性
         * 
         * 可变分辨率着色功能的特性开关，控制是否启用片段密度图功能。
         */
        VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
                .pNext = &fragmentDensityMapOffsetFeature_,
        };

        /**
         * @brief 多视图渲染特性
         * 
         * VR和立体渲染的多视图功能特性开关，控制是否可以在单次渲染中
         * 生成多个视角的图像。
         */
        VkPhysicalDeviceMultiviewFeatures multiviewFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
                .pNext = &fragmentDensityMapFeature_,
        };

        /**
         * @brief 时间线信号量特性
         * 
         * Vulkan 1.2引入的时间线信号量功能，提供更精细的GPU-CPU同步控制。
         */
        VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
                .pNext = &multiviewFeature_,
        };

        /**
         * @brief 网格着色器特性（NVIDIA专用）
         * 
         * NVIDIA GPU特有的网格着色器功能，提供更高效的几何体处理管线。
         */
        VkPhysicalDeviceMeshShaderFeaturesNV meshShaderFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV,
                .pNext = (void*)&timelineSemaphoreFeature_,
        };

        /**
         * @brief 光线查询特性
         * 
         * 在任意着色器阶段进行光线追踪查询的功能，是光线追踪的重要组成部分。
         */
        VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
                .pNext = (void*)&meshShaderFeature_,
        };

        /**
         * @brief 光线追踪管线特性
         * 
         * 专用的光线追踪着色器管线功能，支持光线生成、相交、命中等着色器。
         */
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
                .pNext = (void*)&rayQueryFeature_,
        };

        /**
         * @brief 加速结构特性
         * 
         * 光线追踪加速结构功能，用于加速光线-几何体相交测试。
         */
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
                .pNext = (void*)&rayTracingFeature_,
        };

        /**
         * @brief 描述符索引特性
         * 
         * 动态描述符索引功能，允许在着色器中使用变量索引访问描述符。
         */
        VkPhysicalDeviceDescriptorIndexingFeatures descIndexFeature_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
                .pNext = (void*)&accelStructFeature_,
        };

        /**
         * @brief 缓冲设备地址特性
         * 
         * 允许获取缓冲区的GPU地址，用于高级内存管理和光线追踪。
         */
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
                .pNext = (void*)&descIndexFeature_,
        };

        /**
         * @brief Vulkan 1.2特性集合
         * 
         * Vulkan 1.2版本引入的所有新特性的集合结构。
         */
        VkPhysicalDeviceVulkan12Features features12_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .pNext = (void*)&bufferDeviceAddressFeatures_,
        };

        /**
         * @brief 设备特性主结构（链的头部）
         * 
         * Vulkan设备的基础特性信息，这是特性链的头部，
         * 通过pNext链接到各种扩展特性。
         */
        VkPhysicalDeviceFeatures2 features_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = (void*)&features12_,
        };

        /**
         * @brief 设备内存属性
         * 
         * GPU的内存配置信息，包括内存堆的类型、大小和属性。
         */
        VkPhysicalDeviceMemoryProperties2 memoryProperties_ = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        };

        // === 队列族配置信息 ===
        /**
         * @brief 图形队列族索引
         * 
         * 支持图形操作的队列族索引。如果GPU不支持图形操作（极其罕见），
         * 则为std::nullopt。
         */
        std::optional<uint32_t> graphicsFamilyIndex_;

        /**
         * @brief 图形队列族中可用的队列数量
         * 
         * 图形队列族中可以创建的队列数量。多个队列可以并行处理不同的渲染任务。
         */
        uint32_t graphicsQueueCount_ = 0;

        /**
         * @brief 计算队列族索引
         * 
         * 支持计算操作的队列族索引。可能与图形队列是同一个族，
         * 也可能是专用的计算队列族。
         */
        std::optional<uint32_t> computeFamilyIndex_;

        /**
         * @brief 计算队列族中可用的队列数量
         * 
         * 计算队列族中可以创建的队列数量。专用计算队列可以与图形渲染并行执行。
         */
        uint32_t computeQueueCount_ = 0;

        /**
         * @brief 传输队列族索引
         * 
         * 支持数据传输操作的队列族索引。专用传输队列可以在后台进行数据传输，
         * 与图形/计算操作并行执行。
         */
        std::optional<uint32_t> transferFamilyIndex_;

        /**
         * @brief 传输队列族中可用的队列数量
         * 
         * 传输队列族中可以创建的队列数量。多个传输队列可以并行处理不同的数据传输任务。
         */
        uint32_t transferQueueCount_ = 0;

        /**
         * @brief 稀疏资源队列族索引
         * 
         * 支持稀疏资源绑定的队列族索引。稀疏资源是Vulkan的高级特性，
         * 不是所有GPU都支持。
         */
        std::optional<uint32_t> sparseFamilyIndex_;

        /**
         * @brief 稀疏资源队列族中可用的队列数量
         * 
         * 稀疏资源队列族中可以创建的队列数量。稀疏资源操作通常不需要多个队列。
         */
        uint32_t sparseQueueCount_ = 0;

        /**
         * @brief 呈现队列族索引
         * 
         * 支持呈现操作的队列族索引。呈现队列负责将渲染结果显示到屏幕上。
         * 呈现支持与具体的显示表面相关。
         */
        std::optional<uint32_t> presentationFamilyIndex_;

        /**
         * @brief 呈现队列族中可用的队列数量
         * 
         * 呈现队列族中可以创建的队列数量。通常一个呈现队列就足够了。
         */
        uint32_t presentationQueueCount_ = 0;

        /**
         * @brief 所有队列族的属性信息
         * 
         * 包含GPU所有队列族的详细属性，如支持的操作类型、队列数量、
         * 时间戳查询支持等。
         */
        std::vector<VkQueueFamilyProperties> queueFamilyProperties_;

        // === 交换链支持信息 ===
        /**
         * @brief 表面支持的像素格式列表
         * 
         * GPU与显示表面支持的所有像素格式和颜色空间组合。
         * 用于创建兼容的交换链图像。
         */
        std::vector<VkSurfaceFormatKHR> surfaceFormats_;

        /**
         * @brief 表面能力信息
         * 
         * GPU与显示表面的兼容性信息，包括图像数量限制、尺寸限制、
         * 变换能力、合成模式等。
         */
        VkSurfaceCapabilitiesKHR surfaceCapabilities_;

        /**
         * @brief 支持的呈现模式列表
         * 
         * GPU与表面支持的所有呈现模式，控制图像如何从交换链显示到屏幕。
         * 包括立即呈现、垂直同步、三重缓冲等模式。
         */
        std::vector<VkPresentModeKHR> presentModes_;

        /**
         * @brief 已启用的扩展集合
         * 
         * 构造时成功验证并启用的扩展名称集合。这些扩展在创建逻辑设备时会被启用。
         * 使用unordered_set提供O(1)的查找性能。
         */
        std::unordered_set<std::string> enabledExtensions_;
    };

}  // namespace VkCore