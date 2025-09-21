/**
 * @file PhysicalDevice.cpp
 * @brief Vulkan物理设备实现 - 初学者详细指南
 * @author nio
 * @date 2025/7/7
 * 
 * ## 实现概述
 * 
 * 本文件实现了Vulkan物理设备的完整查询和管理功能。物理设备代表系统中实际的GPU硬件，
 * 这个实现负责发现GPU的所有能力、限制和特性，为创建逻辑设备做准备。
 * 
 * ## 核心功能
 * 
 * ### 1. 设备信息查询
 * - **设备属性查询**：获取GPU名称、驱动版本、硬件限制
 * - **设备特性查询**：获取GPU支持的功能开关
 * - **内存属性查询**：获取GPU内存配置信息
 * - **扩展枚举**：发现GPU支持的所有Vulkan扩展
 * 
 * ### 2. 队列族发现
 * - **队列族枚举**：发现GPU的所有队列族
 * - **能力检测**：检查每个队列族支持的操作类型
 * - **智能分配**：根据需求分配最优的队列族
 * - **呈现支持**：检查队列族的显示能力
 * 
 * ### 3. 表面兼容性
 * - **表面格式查询**：获取支持的像素格式和颜色空间
 * - **表面能力查询**：获取交换链的限制和能力
 * - **呈现模式查询**：获取支持的显示同步模式
 * 
 * ## 关键Vulkan概念
 * 
 * ### 物理设备 vs 逻辑设备
 * - **物理设备**：代表实际的GPU硬件，只能查询，不能直接使用
 * - **逻辑设备**：基于物理设备创建的抽象接口，用于实际的GPU操作
 * - **关系**：一个物理设备可以创建多个逻辑设备（不常见）
 * 
 * ### 队列族系统
 * Vulkan将GPU的功能分为不同的队列族：
 * - 每个队列族支持特定类型的操作
 * - 同一队列族中可以有多个队列
 * - 不同队列族可以并行工作
 * 
 * ### 扩展系统
 * Vulkan使用扩展提供额外功能：
 * - **核心功能**：所有GPU都必须支持
 * - **可选扩展**：GPU厂商提供的额外功能
 * - **依赖关系**：某些扩展依赖其他扩展
 * 
 * ## 实现策略
 * 
 * ### 1. 一次性查询
 * 在构造时一次性查询所有信息，避免重复API调用：
 * - 缓存所有查询结果
 * - 提供高效的访问接口
 * - 减少运行时开销
 * 
 * ### 2. 智能队列分配
 * 优先使用专用队列，回退到共享队列：
 * - 专用Transfer队列优于Graphics队列进行传输
 * - 专用Compute队列优于Graphics队列进行计算
 * - 确保呈现队列的可用性
 * 
 * ### 3. 调试友好
 * 提供详细的设备信息输出：
 * - 设备名称和版本信息
 * - 支持的扩展列表
 * - 表面格式和呈现模式
 */

#include "PhysicalDevice.h"

#include <algorithm>    // std::算法函数
#include <iostream>     // 标准输入输出流
#include <set>          // 有序集合容器

namespace VkCore {

    /**
     * @brief 构造函数实现 - 完整的设备查询和初始化过程
     * 
     * 这个构造函数是PhysicalDevice类的核心，它执行完整的GPU设备查询流程。
     * 整个过程分为7个主要阶段，每个阶段负责查询GPU的不同方面。
     * 
     * ## 执行流程概述：
     * 1. **光线追踪配置**：根据enableRayTracing参数调整特性链
     * 2. **设备特性查询**：获取GPU支持的所有功能特性
     * 3. **设备属性查询**：获取GPU的硬件属性和限制
     * 4. **内存属性查询**：获取GPU的内存配置信息
     * 5. **队列族枚举**：发现所有可用的队列族
     * 6. **扩展枚举**：发现并验证GPU支持的扩展
     * 7. **表面兼容性查询**：查询与显示表面的兼容性信息
     * 8. **调试信息输出**：可选的详细设备信息打印
     * 
     * ## 关键设计决策：
     * - **一次性查询**：构造时查询所有信息，运行时不再调用Vulkan API
     * - **链式结构**：使用Vulkan的pNext链查询扩展特性和属性
     * - **条件配置**：根据需求动态调整查询内容
     * - **错误处理**：使用VK_CHECK宏确保API调用成功
     */
    PhysicalDevice::PhysicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,
                                   const std::vector<std::string>& requestedExtensions,
                                   bool printEnumerations, bool enableRayTracing)
            : physicalDevice_{device} {  // 保存物理设备句柄

        // === 阶段1：光线追踪特性链配置 ===
        // 根据是否启用光线追踪来调整特性查询链的结构
        if (!enableRayTracing) {
            // 如果不启用光线追踪，跳过光线追踪相关的特性查询
            // 将描述符索引特性直接链接到网格着色器特性，绕过光线追踪特性
            descIndexFeature_.pNext = &meshShaderFeature_;
        }
        // 注意：如果启用光线追踪，保持头文件中定义的完整链式结构

        // === 阶段2：设备特性查询 ===
        // 查询GPU支持的所有功能特性（可启用/禁用的功能开关）
        vkGetPhysicalDeviceFeatures2(physicalDevice_, &features_);
        // 这个调用会通过pNext链自动查询所有扩展特性：
        // - Vulkan 1.2特性
        // - 缓冲设备地址特性
        // - 描述符索引特性
        // - 光线追踪相关特性（如果启用）
        // - 网格着色器特性
        // - 时间线信号量特性
        // - 多视图特性
        // - 片段密度图特性

        // === 阶段3：设备属性查询 ===
        // 查询GPU的硬件属性和各种限制参数
        vkGetPhysicalDeviceProperties2(physicalDevice_, &properties_);
        // 这个调用会通过pNext链自动查询所有扩展属性：
        // - 基础设备属性（名称、驱动版本、硬件限制等）
        // - 光线追踪管线属性（如果启用）
        // - 片段密度图属性
        // - 高通片段密度图偏移属性

        // === 阶段4：内存属性查询 ===
        // 查询GPU的内存配置信息（内存堆类型、大小、属性等）
        vkGetPhysicalDeviceMemoryProperties2(physicalDevice_, &memoryProperties_);
        // 内存属性包括：
        // - 内存堆数量和大小（设备本地内存、主机可见内存等）
        // - 内存类型及其属性（缓存一致性、主机访问性等）

        // === 阶段5：队列族枚举 ===
        // 发现GPU的所有队列族，了解每个队列族支持的操作类型和队列数量
        {
            // 子步骤5.1：查询队列族数量
            uint32_t queueFamilyCount{0};
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
            
            // 子步骤5.2：分配存储空间并获取队列族属性
            queueFamilyProperties_.resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount,
                                                     queueFamilyProperties_.data());
            
            // 队列族属性包含：
            // - queueFlags：支持的操作类型（Graphics、Compute、Transfer、Sparse）
            // - queueCount：该族中可创建的队列数量
            // - timestampValidBits：时间戳查询的精度
            // - minImageTransferGranularity：图像传输的最小粒度
        }

        // === 阶段6：扩展枚举和验证 ===
        // 发现GPU支持的所有扩展，并验证应用程序请求的扩展是否可用
        {
            // 子步骤6.1：查询扩展数量
            uint32_t propertyCount{0};
            VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr,
                                                          &propertyCount, nullptr));
            
            // 子步骤6.2：分配存储空间并获取扩展属性
            std::vector<VkExtensionProperties> properties(propertyCount);
            VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr,
                                                          &propertyCount, properties.data()));

            // 子步骤6.3：将扩展属性转换为字符串列表
            // 使用std::transform将VkExtensionProperties转换为std::string
            std::transform(properties.begin(), properties.end(), std::back_inserter(extensions_),
                           [](const VkExtensionProperties& property) {
                               // 从每个扩展属性中提取扩展名称
                               return std::string(property.extensionName);
                           });

            // 子步骤6.4：验证请求的扩展并构建启用列表
            // 使用工具函数过滤出GPU实际支持的请求扩展
            enabledExtensions_ = util::filterExtensions(extensions_, requestedExtensions);
            
            // 扩展验证的重要性：
            // - 确保应用程序请求的功能确实可用
            // - 避免在创建逻辑设备时出现错误
            // - 提供扩展可用性的反馈
        }

        // === 阶段7：表面兼容性查询（可选） ===
        // 如果提供了显示表面，查询GPU与表面的兼容性信息
        if (surface != VK_NULL_HANDLE) {
            // 子步骤7.1：查询表面支持的像素格式
            enumerateSurfaceFormats(surface);
            // 获取GPU与表面支持的所有像素格式和颜色空间组合
            // 用于创建兼容的交换链图像
            
            // 子步骤7.2：查询表面能力信息
            enumerateSurfaceCapabilities(surface);
            // 获取交换链的各种限制和能力：
            // - 图像数量限制（最少/最多图像数）
            // - 图像尺寸限制（最小/最大分辨率）
            // - 变换能力（旋转、翻转支持）
            // - 合成模式（与窗口系统的混合方式）
            
            // 子步骤7.3：查询呈现模式
            enumeratePresentationModes(surface);
            // 获取支持的所有呈现模式：
            // - 立即呈现（可能撕裂）
            // - 垂直同步（保证不撕裂）
            // - 三重缓冲（低延迟）
            // - 自适应垂直同步
        }
        // 注意：如果surface为VK_NULL_HANDLE，跳过表面查询
        // 这适用于纯计算应用或离屏渲染应用

        // === 阶段8：调试信息输出（可选） ===
        // 如果启用了调试输出，打印详细的设备信息
        if (printEnumerations) {
            // 子步骤8.1：输出设备基本信息
            // 打印GPU名称、厂商ID、设备ID和Vulkan版本
            std::cerr << properties_.properties.deviceName << " "          // GPU名称（如"NVIDIA GeForce RTX 4090"）
                      << properties_.properties.vendorID << " ("           // 厂商ID（NVIDIA=0x10DE, AMD=0x1002, Intel=0x8086）
                      << properties_.properties.deviceID << ") - ";        // 设备ID（具体GPU型号的标识）
            
            // 解析并输出Vulkan API版本
            const auto apiVersion = properties_.properties.apiVersion;
            std::cerr << "Vulkan " << VK_API_VERSION_MAJOR(apiVersion) << "."    // 主版本号
                      << VK_API_VERSION_MINOR(apiVersion) << "."                 // 次版本号
                      << VK_API_VERSION_PATCH(apiVersion) << "."                 // 补丁版本号
                      << VK_API_VERSION_VARIANT(apiVersion) << ")" << std::endl; // 变体版本号

            // 子步骤8.2：输出支持的扩展列表
            std::cerr << "Extensions: " << std::endl;
            for (const auto& extension : extensions_) {
                std::cerr << "\t" << extension << std::endl;
                // 输出每个扩展的名称，如：
                // - VK_KHR_swapchain
                // - VK_KHR_ray_tracing_pipeline
                // - VK_EXT_fragment_density_map
            }

            // 子步骤8.3：输出支持的表面格式
            std::cerr << "Supported surface formats: " << std::endl;
            for (const auto format : surfaceFormats_) {
#ifdef _WIN32
                // Windows平台：使用Vulkan SDK提供的字符串转换函数
                std::cerr << "\t" << string_VkFormat(format.format) << " : "
                          << string_VkColorSpaceKHR(format.colorSpace) << std::endl;
#else
                // 其他平台：直接输出枚举值（数字）
                std::cerr << "\t" << format.format << " : " << format.colorSpace << std::endl;
#endif
                // 表面格式包括：
                // - 像素格式：VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM等
                // - 颜色空间：VK_COLOR_SPACE_SRGB_NONLINEAR_KHR等
            }

            // 子步骤8.4：输出支持的呈现模式
            std::cerr << "Supported presentation modes: " << std::endl;
            for (const auto mode : presentModes_) {
#ifdef _WIN32
                // Windows平台：使用字符串转换函数输出可读的模式名称
                std::cerr << "\t" << string_VkPresentModeKHR(mode) << std::endl;
#else
                // 其他平台：输出枚举值
                std::cerr << "\t" << mode << std::endl;
#endif
                // 呈现模式包括：
                // - VK_PRESENT_MODE_IMMEDIATE_KHR：立即呈现
                // - VK_PRESENT_MODE_FIFO_KHR：垂直同步
                // - VK_PRESENT_MODE_MAILBOX_KHR：三重缓冲
                // - VK_PRESENT_MODE_FIFO_RELAXED_KHR：自适应垂直同步
            }
        }
        // 调试输出的价值：
        // - 帮助开发者了解GPU的具体能力
        // - 便于调试设备选择和兼容性问题
        // - 在不同平台上验证扩展和格式支持
    }  // 构造函数结束

    /**
     * @brief 获取底层VkPhysicalDevice句柄实现
     * 
     * 返回构造时保存的原生Vulkan物理设备句柄。这个句柄用于与其他
     * Vulkan API函数交互，特别是创建逻辑设备时。
     * 
     * @return VkPhysicalDevice 原生Vulkan物理设备句柄
     * 
     * @note 这是一个简单的getter函数，直接返回成员变量
     */
    VkPhysicalDevice PhysicalDevice::vkPhysicalDevice() const { 
        return physicalDevice_; 
    }

    /**
     * @brief 获取设备支持的扩展列表实现
     * 
     * 返回在构造时查询并缓存的GPU扩展列表。这个列表包含了GPU
     * 支持的所有Vulkan扩展名称。
     * 
     * @return const std::vector<std::string>& 扩展名称列表的常引用
     * 
     * @note 返回引用避免不必要的复制，const确保不会被修改
     */
    const std::vector<std::string>& PhysicalDevice::extensions() const { 
        return extensions_; 
    }

    /**
     * @brief 队列预留实现 - 智能队列分配算法
     * 
     * 这是PhysicalDevice类中最复杂的函数之一，它实现了智能的队列族分配算法。
     * 根据应用程序的需求，为不同类型的操作分配最优的队列族。
     * 
     * ## 算法设计原理：
     * 
     * ### 1. 专用队列优先策略
     * - **专用Transfer队列**：如果存在只支持Transfer的队列族，优先使用
     * - **专用Compute队列**：如果存在只支持Compute的队列族，优先使用
     * - **共享队列回退**：如果没有专用队列，使用Graphics队列（通常支持所有操作）
     * 
     * ### 2. 多线程友好设计
     * - 每个队列族分配给单一用途，避免多线程竞争
     * - Graphics队列专用于渲染，不与其他操作共享
     * - 只有呈现操作可以与其他操作共享队列族
     * 
     * ### 3. 呈现支持检查
     * - 如果提供了surface，确保找到支持呈现的队列族
     * - 呈现队列可以与其他类型的队列共享（通常与Graphics共享）
     * 
     * ## 分配流程：
     * 1. 遍历所有队列族
     * 2. 检查呈现支持（如果需要）
     * 3. 按优先级分配队列族：Graphics -> Compute -> Transfer -> Sparse
     * 4. 验证分配结果的有效性
     * 
     * ## 使用场景：
     * ```cpp
     * // 基础图形应用
     * physicalDevice.reserveQueues(VK_QUEUE_GRAPHICS_BIT, surface);
     * 
     * // 计算应用
     * physicalDevice.reserveQueues(VK_QUEUE_COMPUTE_BIT, VK_NULL_HANDLE);
     * 
     * // 多功能应用
     * physicalDevice.reserveQueues(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | 
     *                              VK_QUEUE_TRANSFER_BIT, surface);
     * ```
     */
    void PhysicalDevice::reserveQueues(VkQueueFlags requestedQueueTypes,
                                       VkSurfaceKHR surface) {
        // === 步骤1：参数验证 ===
        ASSERT(requestedQueueTypes > 0, "Requested queue types is empty");

        // === 核心设计理念说明 ===
        // 我们只允许队列与呈现功能共享。Vulkan队列可以支持多种操作
        // （如graphics、compute、sparse、transfer等），但在这种情况下
        // 该队列只能通过一个线程使用。这段代码确保我们将每个队列视为独立的，
        // 它只能用于graphics/compute/transfer或sparse中的一种，
        // 这有助于多线程处理。然而，如果设备只有一个支持所有操作的队列，
        // 那么由于这种设计，我们可能无法创建compute/transfer队列。
        //
        // 这种设计的权衡：
        // + 多线程友好：避免队列竞争
        // + 性能优化：专用队列可以并行工作
        // - 兼容性：某些设备可能无法充分利用

        // === 步骤2：队列族遍历和分配 ===
        // 遍历所有队列族，为每种请求的队列类型找到最适合的队列族
        for (uint32_t queueFamilyIndex = 0;
             queueFamilyIndex < queueFamilyProperties_.size() && requestedQueueTypes != 0;
             ++queueFamilyIndex) {
            // === 子步骤2.1：呈现支持检查 ===
            // 如果需要呈现功能且尚未找到支持呈现的队列族
            if (!presentationFamilyIndex_.has_value() && surface != VK_NULL_HANDLE) {
                VkBool32 supportsPresent{VK_FALSE};
                // 查询当前队列族是否支持向指定表面呈现
                vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, queueFamilyIndex, surface,
                                                     &supportsPresent);
                if (supportsPresent == VK_TRUE) {
                    // 找到支持呈现的队列族，记录其索引和队列数量
                    presentationFamilyIndex_ = queueFamilyIndex;
                    presentationQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
                }
            }

            // === 子步骤2.2：图形队列分配 ===
            // 检查是否需要图形队列且当前队列族支持图形操作
            if (!graphicsFamilyIndex_.has_value() &&
                (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
                VK_QUEUE_GRAPHICS_BIT) {
                // 分配图形队列族
                graphicsFamilyIndex_ = queueFamilyIndex;
                graphicsQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
                // 从请求中移除图形队列标志，表示已满足需求
                requestedQueueTypes &= ~VK_QUEUE_GRAPHICS_BIT;
                continue;  // 继续下一个队列族，保持专用队列策略
            }

            // === 子步骤2.3：计算队列分配 ===
            // 检查是否需要计算队列且当前队列族支持计算操作
            if (!computeFamilyIndex_.has_value() &&
                (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
                VK_QUEUE_COMPUTE_BIT) {
                // 分配计算队列族
                computeFamilyIndex_ = queueFamilyIndex;
                computeQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
                // 从请求中移除计算队列标志
                requestedQueueTypes &= ~VK_QUEUE_COMPUTE_BIT;
                continue;  // 保持专用队列策略
            }

            // === 子步骤2.4：传输队列分配 ===
            // 检查是否需要传输队列且当前队列族支持传输操作
            if (!transferFamilyIndex_.has_value() &&
                (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
                VK_QUEUE_TRANSFER_BIT) {
                // 分配传输队列族
                transferFamilyIndex_ = queueFamilyIndex;
                transferQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
                // 从请求中移除传输队列标志
                requestedQueueTypes &= ~VK_QUEUE_TRANSFER_BIT;
                continue;  // 保持专用队列策略
            }

            // === 子步骤2.5：稀疏资源队列分配 ===
            // 检查是否需要稀疏资源队列且当前队列族支持稀疏绑定操作
            if (!sparseFamilyIndex_.has_value() &&
                (requestedQueueTypes & queueFamilyProperties_[queueFamilyIndex].queueFlags) &
                VK_QUEUE_SPARSE_BINDING_BIT) {
                // 分配稀疏资源队列族
                sparseFamilyIndex_ = queueFamilyIndex;
                sparseQueueCount_ = queueFamilyProperties_[queueFamilyIndex].queueCount;
                // 从请求中移除稀疏绑定队列标志
                requestedQueueTypes &= ~VK_QUEUE_SPARSE_BINDING_BIT;
                continue;  // 保持专用队列策略
            }
        }
        
        // 注意：continue语句的作用
        // 每次成功分配一个队列族后，使用continue跳过当前循环迭代
        // 这确保了专用队列策略：每个队列族只分配给一种用途
        // 这样可以最大化并行性能，避免队列竞争

        // === 步骤3：分配结果验证 ===
        
        // 子步骤3.1：验证至少分配了一种队列类型
        ASSERT(graphicsFamilyIndex_.has_value() || computeFamilyIndex_.has_value() ||
               transferFamilyIndex_.has_value() || sparseFamilyIndex_.has_value(),
               "No suitable queue(s) found");
        // 这个断言确保至少找到了一种请求的队列类型
        // 如果所有队列类型都未找到，说明GPU不支持请求的操作

        // 子步骤3.2：验证呈现支持（如果需要）
        ASSERT(surface == VK_NULL_HANDLE || presentationFamilyIndex_.has_value(),
               "No queues with presentation capabilities found");
        // 这个断言确保：
        // - 如果不需要呈现（surface为NULL），则跳过检查
        // - 如果需要呈现，则必须找到支持呈现的队列族
        // 呈现支持的重要性：没有呈现队列就无法显示渲染结果
    }  // reserveQueues函数结束

    /**
     * @brief 获取队列族配置实现 - 为逻辑设备创建准备数据
     * 
     * 这个函数将之前分配的队列族信息转换为创建逻辑设备时需要的格式。
     * 它返回所有已分配队列族的索引和数量配对，用于VkDeviceCreateInfo。
     * 
     * ## 实现细节：
     * 
     * ### 1. 去重机制
     * 使用std::set自动去除重复的队列族索引：
     * - 如果Graphics和Present使用同一队列族，只记录一次
     * - 确保每个队列族只在结果中出现一次
     * 
     * ### 2. 数据格式转换
     * - 输入：成员变量中的optional<uint32_t>索引
     * - 输出：vector<pair<uint32_t, uint32_t>>格式
     * - pair.first：队列族索引
     * - pair.second：该队列族中要创建的队列数量
     * 
     * ### 3. 使用场景
     * 返回的数据直接用于创建逻辑设备：
     * ```cpp
     * auto queueConfigs = physicalDevice.queueFamilyIndexAndCount();
     * std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
     * for (auto& [familyIndex, queueCount] : queueConfigs) {
     *     // 创建VkDeviceQueueCreateInfo...
     * }
     * ```
     * 
     * @return std::vector<std::pair<uint32_t, uint32_t>> 队列族配置列表
     * 
     * @note 使用set确保队列族索引的唯一性和有序性
     */
    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>>
    PhysicalDevice::queueFamilyIndexAndCount() const {
        // 使用std::set自动处理重复和排序
        std::set<std::pair<uint32_t, uint32_t>> familyIndices;
        
        // 检查并添加图形队列族配置
        if (graphicsFamilyIndex_.has_value()) {
            familyIndices.insert({graphicsFamilyIndex_.value(), graphicsFamilyCount()});
        }
        
        // 检查并添加计算队列族配置
        if (computeFamilyIndex_.has_value()) {
            familyIndices.insert({computeFamilyIndex_.value(), computeFamilyCount()});
        }
        
        // 检查并添加传输队列族配置
        if (transferFamilyIndex_.has_value()) {
            familyIndices.insert({transferFamilyIndex_.value(), transferFamilyCount()});
        }
        
        // 检查并添加稀疏资源队列族配置
        if (sparseFamilyIndex_.has_value()) {
            familyIndices.insert({sparseFamilyIndex_.value(), sparseFamilyCount()});
        }
        
        // 检查并添加呈现队列族配置
        if (presentationFamilyIndex_.has_value()) {
            familyIndices.insert({presentationFamilyIndex_.value(), presentationFamilyCount()});
        }
        
        // 将set转换为vector返回
        // 这样既保证了去重，又提供了客户端期望的数据结构
        std::vector<std::pair<uint32_t, uint32_t>> returnValues(familyIndices.begin(),
                                                                familyIndices.end());
        return returnValues;
    }

    // === 队列族索引getter函数实现 ===
    // 这些函数都是简单的getter，直接返回成员变量的值
    // 使用std::optional确保类型安全：如果队列族未分配，返回nullopt

    /**
     * @brief 图形队列族索引getter实现
     * @return std::optional<uint32_t> 图形队列族索引，未分配时为nullopt
     */
    std::optional<uint32_t> PhysicalDevice::graphicsFamilyIndex() const {
        return graphicsFamilyIndex_;
    }

    /**
     * @brief 计算队列族索引getter实现
     * @return std::optional<uint32_t> 计算队列族索引，未分配时为nullopt
     */
    std::optional<uint32_t> PhysicalDevice::computeFamilyIndex() const {
        return computeFamilyIndex_;
    }

    /**
     * @brief 传输队列族索引getter实现
     * @return std::optional<uint32_t> 传输队列族索引，未分配时为nullopt
     */
    std::optional<uint32_t> PhysicalDevice::transferFamilyIndex() const {
        return transferFamilyIndex_;
    }

    /**
     * @brief 稀疏资源队列族索引getter实现
     * @return std::optional<uint32_t> 稀疏资源队列族索引，未分配时为nullopt
     */
    std::optional<uint32_t> PhysicalDevice::sparseFamilyIndex() const {
        return sparseFamilyIndex_;
    }

    /**
     * @brief 呈现队列族索引getter实现
     * @return std::optional<uint32_t> 呈现队列族索引，未分配时为nullopt
     */
    std::optional<uint32_t> PhysicalDevice::presentationFamilyIndex() const {
        return presentationFamilyIndex_;
    }

    /**
     * @brief 表面能力信息getter实现
     * 
     * 返回在构造时查询的表面能力信息。这些信息描述了GPU与显示表面的
     * 兼容性，包括交换链的各种限制和能力。
     * 
     * @return const VkSurfaceCapabilitiesKHR& 表面能力信息的常引用
     * 
     * @note 如果构造时未提供surface，此信息可能无效
     */
    const VkSurfaceCapabilitiesKHR& PhysicalDevice::surfaceCapabilities() const {
        return surfaceCapabilities_;
    }

    // === 私有成员函数实现 - 表面兼容性查询 ===
    // 这些函数在构造时被调用，用于查询GPU与显示表面的兼容性信息

    /**
     * @brief 枚举表面格式实现
     * 
     * 查询GPU与指定表面支持的所有像素格式和颜色空间组合。
     * 这些格式用于创建兼容的交换链图像。
     * 
     * ## 实现步骤：
     * 1. **查询格式数量**：第一次调用获取支持的格式总数
     * 2. **分配存储空间**：根据数量调整容器大小
     * 3. **获取格式数据**：第二次调用获取具体的格式信息
     * 
     * ## 格式信息包含：
     * - **像素格式**：如VK_FORMAT_B8G8R8A8_UNORM、VK_FORMAT_R8G8B8A8_UNORM
     * - **颜色空间**：如VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
     * 
     * @param surface 显示表面句柄
     * 
     * @note 这是标准的Vulkan两阶段查询模式
     */
    void PhysicalDevice::enumerateSurfaceFormats(VkSurfaceKHR surface) {
        // 步骤1：查询支持的表面格式数量
        uint32_t formatCount{0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, nullptr);
        
        // 步骤2：分配足够的存储空间
        surfaceFormats_.resize(formatCount);
        
        // 步骤3：获取具体的表面格式数据
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount,
                                             surfaceFormats_.data());
        
        // 常见的表面格式：
        // - VK_FORMAT_B8G8R8A8_UNORM + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR（最常见）
        // - VK_FORMAT_R8G8B8A8_UNORM + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        // - VK_FORMAT_A2B10G10R10_UNORM_PACK32（HDR支持）
    }

    /**
     * @brief 枚举表面能力实现
     * 
     * 查询GPU与指定表面的详细兼容性信息。这些信息定义了交换链的
     * 各种限制和能力。
     * 
     * ## 能力信息包含：
     * - **图像数量限制**：minImageCount、maxImageCount
     * - **图像尺寸限制**：minImageExtent、maxImageExtent、currentExtent
     * - **变换支持**：supportedTransforms、currentTransform
     * - **合成模式**：supportedCompositeAlpha
     * - **用法标志**：supportedUsageFlags
     * 
     * ## 重要字段说明：
     * - **currentExtent**：当前表面尺寸，通常等于窗口尺寸
     * - **minImageCount**：交换链最少图像数（通常为2，双缓冲）
     * - **maxImageCount**：交换链最多图像数（0表示无限制）
     * 
     * @param surface 显示表面句柄
     * 
     * @note 与格式查询不同，能力查询是单次调用，直接填充结构体
     */
    void PhysicalDevice::enumerateSurfaceCapabilities(VkSurfaceKHR surface) {
        // 直接查询表面能力信息到成员变量
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface,
                                                  &surfaceCapabilities_);
        
        // 能力信息的典型用途：
        // - 确定交换链图像数量：max(minImageCount, 2)
        // - 确定交换链尺寸：通常使用currentExtent
        // - 选择合适的变换模式：通常使用currentTransform
        // - 验证用法标志：确保支持COLOR_ATTACHMENT_BIT
    }

    /**
     * @brief 枚举呈现模式实现
     * 
     * 查询GPU与表面支持的所有呈现模式。呈现模式控制图像如何从
     * 交换链显示到屏幕上，影响性能和视觉质量。
     * 
     * ## 实现步骤：
     * 1. **查询模式数量**：第一次调用获取支持的呈现模式总数
     * 2. **分配存储空间**：根据数量调整容器大小
     * 3. **获取模式数据**：第二次调用获取具体的呈现模式
     * 
     * ## 呈现模式类型：
     * - **VK_PRESENT_MODE_IMMEDIATE_KHR**：立即呈现，可能撕裂，最低延迟
     * - **VK_PRESENT_MODE_FIFO_KHR**：垂直同步，保证不撕裂，必须支持
     * - **VK_PRESENT_MODE_FIFO_RELAXED_KHR**：自适应垂直同步
     * - **VK_PRESENT_MODE_MAILBOX_KHR**：三重缓冲，低延迟且不撕裂
     * 
     * ## 选择策略：
     * 1. 游戏应用：优先选择MAILBOX（低延迟）
     * 2. 一般应用：选择FIFO（稳定，保证支持）
     * 3. 对延迟敏感：选择IMMEDIATE（可能撕裂）
     * 
     * @param surface 显示表面句柄
     * 
     * @note FIFO模式是Vulkan规范要求必须支持的
     */
    void PhysicalDevice::enumeratePresentationModes(VkSurfaceKHR surface) {
        // 步骤1：查询支持的呈现模式数量
        uint32_t presentModeCount{0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount,
                                                  nullptr);

        // 步骤2：分配足够的存储空间
        presentModes_.resize(presentModeCount);
        
        // 步骤3：获取具体的呈现模式数据
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount,
                                                  presentModes_.data());
        
        // 呈现模式的性能特征：
        // IMMEDIATE: 最低延迟，可能撕裂
        // FIFO:      中等延迟，不撕裂，垂直同步
        // MAILBOX:   低延迟，不撕裂，三重缓冲
        // FIFO_RELAXED: 自适应，延迟时允许撕裂
    }

}  // namespace VkCore

/**
 * @page physicaldevice_implementation_guide PhysicalDevice实现指南
 * 
 * ## 文件总结
 * 
 * PhysicalDevice.cpp实现了Vulkan物理设备的完整查询和管理功能。
 * 这个实现为Vulkan初学者提供了一个完整的GPU设备发现和配置解决方案。
 * 
 * ### 核心功能
 * 
 * #### 1. 设备信息查询
 * - **特性查询**：使用链式结构查询所有扩展特性
 * - **属性查询**：获取硬件限制和性能参数
 * - **内存查询**：了解GPU内存配置
 * - **扩展枚举**：发现并验证支持的扩展
 * 
 * #### 2. 智能队列管理
 * - **专用队列优先**：优先使用专门的队列族
 * - **多线程友好**：避免队列竞争
 * - **呈现支持**：确保显示功能可用
 * - **灵活配置**：支持多种应用场景
 * 
 * #### 3. 表面兼容性
 * - **格式查询**：获取支持的像素格式
 * - **能力查询**：了解交换链限制
 * - **呈现模式**：选择合适的显示同步方式
 * 
 * ### 设计模式
 * 
 * #### 1. 一次性查询
 * 构造时查询所有信息，运行时提供高效访问：
 * - 减少API调用开销
 * - 提供一致的接口
 * - 缓存复杂的查询结果
 * 
 * #### 2. 类型安全
 * 使用现代C++特性确保安全性：
 * - std::optional处理可选值
 * - const成员函数保证不变性
 * - 强类型接口避免错误
 * 
 * #### 3. 调试友好
 * 提供详细的调试信息输出：
 * - 设备基本信息
 * - 扩展支持列表
 * - 表面兼容性信息
 * 
 * ### 性能优化
 * 
 * #### 1. 内存效率
 * - 预分配容器避免重复分配
 * - 引用返回避免不必要复制
 * - 智能容器选择（set去重，vector访问）
 * 
 * #### 2. 算法效率
 * - 单次遍历完成队列分配
 * - 位运算处理标志位
 * - 早期退出优化循环
 * 
 * #### 3. 并行友好
 * - 专用队列支持并行操作
 * - 无状态查询函数
 * - 线程安全的设计
 * 
 * ### 使用建议
 * 
 * #### 1. 基础应用
 * ```cpp
 * PhysicalDevice device(vkDevice, surface, {VK_KHR_SWAPCHAIN_EXTENSION_NAME});
 * device.reserveQueues(VK_QUEUE_GRAPHICS_BIT, surface);
 * ```
 * 
 * #### 2. 计算应用
 * ```cpp
 * PhysicalDevice device(vkDevice, VK_NULL_HANDLE, {});
 * device.reserveQueues(VK_QUEUE_COMPUTE_BIT, VK_NULL_HANDLE);
 * ```
 * 
 * #### 3. 高级应用
 * ```cpp
 * PhysicalDevice device(vkDevice, surface, rtExtensions, true, true);
 * device.reserveQueues(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | 
 *                      VK_QUEUE_TRANSFER_BIT, surface);
 * if (device.isRayTracingSupported()) {
 *     // 配置光线追踪...
 * }
 * ```
 * 
 * ### 扩展建议
 * 
 * #### 1. 设备评分
 * 可以添加设备评分机制，根据性能和功能对GPU进行排序。
 * 
 * #### 2. 自动回退
 * 可以添加自动回退机制，当专用队列不可用时自动使用共享队列。
 * 
 * #### 3. 配置验证
 * 可以添加更详细的配置验证，确保所有必需的功能都可用。
 * 
 * ### 常见问题
 * 
 * #### 1. 队列族不足
 * **问题**：某些设备只有一个支持所有操作的队列族
 * **解决**：当前设计优先专用队列，可能需要回退到共享队列
 * 
 * #### 2. 扩展不支持
 * **问题**：请求的扩展在某些设备上不可用
 * **解决**：使用filterExtensions函数过滤，只启用支持的扩展
 * 
 * #### 3. 表面不兼容
 * **问题**：某些队列族不支持特定表面的呈现
 * **解决**：遍历所有队列族找到支持呈现的队列
 * 
 * 这个实现为Vulkan初学者提供了一个完整、高效、易用的物理设备管理解决方案。
 */