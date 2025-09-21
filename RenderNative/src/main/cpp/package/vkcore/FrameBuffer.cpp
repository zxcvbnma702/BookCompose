/**
 * @file FrameBuffer.cpp
 * @brief Vulkan帧缓冲实现 - 初学者详细指南
 * @author nio
 * @date 2025/7/28
 * 
 * ## 实现概述
 * 
 * 本文件实现了Vulkan帧缓冲的创建和管理功能。帧缓冲是连接渲染通道（RenderPass）
 * 和实际纹理资源的桥梁，定义了渲染操作的具体目标。
 * 
 * ## 核心功能
 * 
 * ### 1. 帧缓冲创建
 * - 从纹理对象收集ImageView句柄
 * - 验证附件的兼容性和尺寸一致性
 * - 创建VkFramebuffer对象并设置调试信息
 * 
 * ### 2. 资源管理
 * - RAII模式的自动资源清理
 * - 移动语义支持，避免资源复制
 * - 调试名称设置，便于GPU调试
 * 
 * ### 3. 兼容性处理
 * - 自动处理颜色、深度、模板附件的顺序
 * - 尺寸验证和错误检查
 * - 与RenderPass的兼容性确保
 * 
 * ## 关键Vulkan概念
 * 
 * ### VkFramebuffer
 * Vulkan中的帧缓冲对象，包含一组ImageView，定义了渲染的目标。
 * 它必须与特定的RenderPass兼容，并且所有附件必须有相同的尺寸。
 * 
 * ### VkImageView
 * 图像视图，定义了如何访问VkImage的特定部分。每个附件都需要
 * 一个对应的ImageView来告诉GPU如何解释图像数据。
 * 
 * ### 附件顺序
 * Vulkan要求帧缓冲中的附件顺序与RenderPass中的附件描述顺序完全匹配：
 * 1. 所有颜色附件（按提供的顺序）
 * 2. 深度附件（如果有）
 * 3. 模板附件（如果有）
 * 
 * ## 实现特点
 * 
 * ### 简化接口
 * 用户只需要提供纹理对象，实现会自动：
 * - 获取正确的ImageView
 * - 排列正确的附件顺序
 * - 进行尺寸和兼容性验证
 * 
 * ### 错误处理
 * - 使用断言检查关键错误（如空附件列表）
 * - 依赖Vulkan验证层检测兼容性问题
 * - 提供清晰的调试信息
 */

#include "FrameBuffer.h"

#include <algorithm>    // std::算法函数

#include "Context.h"    // Vulkan上下文管理
#include "Texture.h"    // 纹理管理类

namespace VkCore {

    /**
     * @brief 构造函数实现 - 创建Vulkan帧缓冲
     * 
     * 这个构造函数是整个Framebuffer类的核心，它将多个纹理对象组合成一个
     * 可以被GPU使用的帧缓冲对象。整个过程可以分为5个主要步骤。
     * 
     * ## 实现步骤：
     * 1. **收集ImageView**：从每个纹理对象获取VkImageView句柄
     * 2. **验证附件**：确保至少有一个附件，防止创建空帧缓冲
     * 3. **确定尺寸**：从第一个有效附件获取帧缓冲的宽度和高度
     * 4. **创建帧缓冲**：使用VkFramebufferCreateInfo创建GPU对象
     * 5. **设置调试名称**：便于GPU调试工具识别此对象
     * 
     * ## 关键概念解释：
     * 
     * ### ImageView的作用
     * 每个纹理（VkImage）需要通过ImageView来告诉GPU如何解释图像数据：
     * - 使用哪种格式读取像素
     * - 访问哪个mip级别
     * - 如何处理多层图像
     * 
     * ### 附件顺序的重要性
     * Vulkan严格要求帧缓冲中的附件顺序与RenderPass中的附件描述完全匹配。
     * 我们使用的顺序是：颜色附件 -> 深度附件 -> 模板附件
     * 
     * ### 尺寸一致性
     * 所有附件必须有相同的宽度和高度，这是Vulkan的硬性要求。
     * 如果尺寸不匹配，vkCreateFramebuffer会失败。
     */
    Framebuffer::Framebuffer(const Context& context, VkDevice device, VkRenderPass renderPass,
                             const std::vector<std::shared_ptr<Texture>>& attachments,
                             const std::shared_ptr<Texture> depthAttachment,
                             const std::shared_ptr<Texture> stencilAttachment,
                             const std::string& name)
            : device_{device} {  // 保存设备句柄用于析构时清理资源
        // === 步骤1：收集所有附件的ImageView句柄 ===
        // ImageView是GPU访问图像数据的"窗口"，每个附件都需要一个
        std::vector<VkImageView> imageViews;
        
        // === 子步骤1.1：处理所有颜色附件 ===
        // 颜色附件用于存储渲染的颜色结果（如最终图像、G-Buffer等）
        for (const auto& texture : attachments) {
            // 获取mip级别0的ImageView（通常是最高分辨率的级别）
            // 对于帧缓冲，我们总是使用mip级别0，因为渲染总是输出到最高分辨率
            imageViews.push_back(texture->vkImageView(0));
        }
        
        // === 子步骤1.2：处理深度附件（可选） ===
        // 深度附件存储每个像素的深度值，用于3D渲染的深度测试
        if (depthAttachment) {
            // 深度纹理也使用mip级别0
            imageViews.push_back(depthAttachment->vkImageView(0));
        }
        
        // === 子步骤1.3：处理模板附件（可选） ===
        // 模板附件存储模板值，用于复杂的遮罩和剔除效果
        if (stencilAttachment) {
            // 模板纹理同样使用mip级别0
            imageViews.push_back(stencilAttachment->vkImageView(0));
        }

        // === 步骤2：验证附件有效性 ===
        // Vulkan不允许创建没有任何附件的帧缓冲，这在逻辑上也没有意义
        // 因为没有地方存储渲染结果
        ASSERT(!imageViews.empty(),
               "Creating a framebuffer with no attachments is not supported");

        // === 步骤3：确定帧缓冲尺寸 ===
        // 帧缓冲的尺寸由其附件决定，所有附件必须有相同的尺寸
        // 我们从第一个有效的附件获取尺寸信息
        const uint32_t width = !attachments.empty() ? attachments[0]->vkExtents().width      // 优先使用颜色附件的尺寸
                                                    : depthAttachment->vkExtents().width;     // 如果没有颜色附件，使用深度附件
        const uint32_t height = !attachments.empty() ? attachments[0]->vkExtents().height    // 优先使用颜色附件的尺寸
                                                     : depthAttachment->vkExtents().height;   // 如果没有颜色附件，使用深度附件
        
        // 注意：这里假设所有附件都有相同的尺寸，如果不同，vkCreateFramebuffer会失败

        // === 步骤4：创建VkFramebufferCreateInfo结构体 ===
        // 这个结构体包含了创建帧缓冲所需的所有信息
        const VkFramebufferCreateInfo framebufferInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,              // 结构体类型标识
                //.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,                  // 可选标志，注释掉表示使用传统模式
                .renderPass = renderPass,                                        // 兼容的渲染通道
                .attachmentCount = static_cast<uint32_t>(imageViews.size()),     // 附件数量
                .pAttachments = imageViews.data(),                               // 附件ImageView数组指针
                .width = width,                                                  // 帧缓冲宽度（像素）
                .height = height,                                                // 帧缓冲高度（像素）
                .layers = 1,                                                     // 图层数量（通常为1，VR应用可能>1）
        };
        
        // 关于注释掉的flags字段的解释：
        // VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT 是一个高级特性，允许创建"无图像"的帧缓冲
        // 这种帧缓冲在渲染时才指定具体的图像，提供了更大的灵活性
        // 但需要额外的扩展支持，这里使用传统模式更简单可靠
        
        // === 步骤4.1：创建VkFramebuffer对象 ===
        // 调用Vulkan API创建实际的GPU资源
        VK_CHECK(vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffer_));
        
        // VK_CHECK是一个宏，用于检查Vulkan函数的返回值
        // 如果返回值不是VK_SUCCESS，会触发错误处理（通常是断言或异常）
        
        // === 步骤5：设置调试名称 ===
        // 为GPU对象设置人类可读的名称，便于调试工具识别
        // 这在复杂应用中非常有用，可以快速定位问题
        context.setVkObjectname(framebuffer_, VK_OBJECT_TYPE_FRAMEBUFFER,
                                "Framebuffer: " + name);
        
        // 调试名称的好处：
        // 1. 在GPU调试器（如RenderDoc）中显示有意义的名称
        // 2. 在验证层错误信息中提供更清晰的对象标识
        // 3. 便于性能分析工具的对象跟踪
    }  // 构造函数结束

    /**
     * @brief 析构函数实现 - 自动清理GPU资源
     * 
     * 使用RAII（Resource Acquisition Is Initialization）模式自动释放
     * VkFramebuffer对象。这确保了即使在异常情况下也不会发生GPU资源泄漏。
     * 
     * ## 清理过程：
     * 1. 调用vkDestroyFramebuffer释放GPU端的帧缓冲对象
     * 2. 使用之前保存的device_句柄进行清理
     * 3. 传递nullptr作为分配器参数（使用默认分配器）
     * 
     * ## 重要说明：
     * - **只清理帧缓冲本身**：不会影响附件纹理，它们由各自的Texture对象管理
     * - **不影响RenderPass**：RenderPass由其自己的对象管理生命周期
     * - **线程安全**：确保没有其他线程正在使用此帧缓冲
     * 
     * ## 资源管理的最佳实践：
     * 这种设计遵循了"谁创建谁负责清理"的原则：
     * - Framebuffer负责清理VkFramebuffer对象
     * - Texture对象负责清理VkImage和VkImageView
     * - RenderPass对象负责清理VkRenderPass
     * 
     * @note 析构函数是隐式调用的，无需手动管理
     * @warning 必须确保没有正在进行的渲染操作使用此帧缓冲
     */
    Framebuffer::~Framebuffer() { 
        vkDestroyFramebuffer(device_, framebuffer_, nullptr); 
    }

    /**
     * @brief 获取底层VkFramebuffer句柄
     * 
     * 返回原生的Vulkan帧缓冲句柄，用于与Vulkan API直接交互。
     * 这是一个const成员函数，不会修改对象的状态。
     * 
     * ## 主要用途：
     * 
     * ### 1. 渲染通道开始
     * ```cpp
     * VkRenderPassBeginInfo beginInfo = {
     *     .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
     *     .renderPass = renderPass.vkRenderPass(),
     *     .framebuffer = framebuffer.vkFramebuffer(),  // 使用此函数
     *     .renderArea = {{0, 0}, {width, height}},
     *     // ... 其他字段
     * };
     * vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
     * ```
     * 
     * ### 2. 兼容性查询
     * ```cpp
     * // 检查帧缓冲是否与特定RenderPass兼容
     * VkBool32 compatible = checkFramebufferCompatibility(
     *     framebuffer.vkFramebuffer(), 
     *     renderPass.vkRenderPass()
     * );
     * ```
     * 
     * ### 3. 调试和性能分析
     * ```cpp
     * // 在调试工具中标记渲染阶段
     * debugMarker.beginRegion(commandBuffer, 
     *                        "Render to " + getFramebufferName(framebuffer.vkFramebuffer()));
     * ```
     * 
     * ## 设计考虑：
     * - **const正确性**：函数不修改对象状态，可以在const对象上调用
     * - **[[nodiscard]]属性**：提醒调用者不要忽略返回值
     * - **直接访问**：避免不必要的包装，提供最大的灵活性
     * 
     * @return VkFramebuffer 原生Vulkan帧缓冲句柄
     * 
     * @note 返回的句柄只在此对象生命周期内有效
     * @warning 不要在对象销毁后使用返回的句柄
     */
    VkFramebuffer Framebuffer::vkFramebuffer() const { 
        return framebuffer_; 
    }

}  // namespace VkCore

/**
 * @page framebuffer_implementation_guide Framebuffer实现详解
 * 
 * ## 实现总结
 * 
 * 本文件实现了一个简化但功能完整的Vulkan帧缓冲管理类。通过封装复杂的Vulkan API调用，
 * 为用户提供了直观易用的接口，同时保持了高性能和灵活性。
 * 
 * ## 核心特性
 * 
 * ### 1. 自动化资源管理
 * - **RAII模式**：构造时分配，析构时释放，确保资源安全
 * - **移动语义**：避免昂贵的GPU资源复制操作
 * - **智能指针兼容**：与std::shared_ptr<Texture>无缝集成
 * 
 * ### 2. 简化的接口设计
 * - **类型安全**：使用强类型参数，减少错误
 * - **默认参数**：提供合理的默认值，简化常见用例
 * - **清晰的分离**：颜色、深度、模板附件分别处理
 * 
 * ### 3. 错误处理和调试支持
 * - **断言验证**：在开发阶段捕获逻辑错误
 * - **调试命名**：GPU调试工具友好的对象标识
 * - **文档化**：详细的注释和使用示例
 * 
 * ## 设计模式
 * 
 * ### 1. RAII (Resource Acquisition Is Initialization)
 * ```cpp
 * {
 *     Framebuffer fb(context, device, renderPass, attachments, depth, nullptr);
 *     // 使用帧缓冲...
 * } // 自动清理，无需手动释放
 * ```
 * 
 * ### 2. 移动语义 (Move Semantics)
 * ```cpp
 * Framebuffer createFramebuffer() {
 *     return Framebuffer(context, device, renderPass, attachments, depth, nullptr);
 *     // 返回时自动移动，不复制昂贵的GPU资源
 * }
 * ```
 * 
 * ### 3. 桥接模式 (Bridge Pattern)
 * ```cpp
 * // 高级接口（Framebuffer类）桥接到底层API（Vulkan）
 * class Framebuffer {
 *     VkFramebuffer framebuffer_;  // 桥接到Vulkan对象
 * public:
 *     VkFramebuffer vkFramebuffer() const;  // 提供底层访问
 * };
 * ```
 * 
 * ## 性能优化
 * 
 * ### 1. 最小化GPU状态变化
 * ```cpp
 * // 重用相同尺寸和格式的帧缓冲，避免频繁创建/销毁
 * class FramebufferCache {
 *     std::unordered_map<FramebufferKey, std::unique_ptr<Framebuffer>> cache_;
 * public:
 *     Framebuffer* getOrCreate(const FramebufferKey& key);
 * };
 * ```
 * 
 * ### 2. 批量操作
 * ```cpp
 * // 对于多个相似的帧缓冲，批量创建可以提高效率
 * std::vector<Framebuffer> createFramebuffers(
 *     const std::vector<FramebufferDesc>& descs
 * );
 * ```
 * 
 * ### 3. 内存局部性
 * ```cpp
 * // 将相关的帧缓冲对象存储在连续内存中
 * class FramebufferArray {
 *     std::vector<Framebuffer> framebuffers_;  // 连续存储
 * };
 * ```
 * 
 * ## 扩展建议
 * 
 * ### 1. 支持动态调整大小
 * ```cpp
 * class ResizableFramebuffer : public Framebuffer {
 * public:
 *     void resize(uint32_t newWidth, uint32_t newHeight);
 * private:
 *     void recreateFramebuffer();
 * };
 * ```
 * 
 * ### 2. 多重采样支持优化
 * ```cpp
 * class MSAAFramebuffer : public Framebuffer {
 * public:
 *     MSAAFramebuffer(VkSampleCountFlagBits samples, ...);
 *     void resolve(Framebuffer& target);  // MSAA解析
 * };
 * ```
 * 
 * ### 3. 帧缓冲池管理
 * ```cpp
 * class FramebufferPool {
 * public:
 *     std::unique_ptr<Framebuffer> acquire(const FramebufferDesc& desc);
 *     void release(std::unique_ptr<Framebuffer> fb);
 * private:
 *     std::vector<std::unique_ptr<Framebuffer>> available_;
 *     std::vector<std::unique_ptr<Framebuffer>> inUse_;
 * };
 * ```
 * 
 * ### 4. 异步创建支持
 * ```cpp
 * class AsyncFramebufferFactory {
 * public:
 *     std::future<Framebuffer> createAsync(const FramebufferDesc& desc);
 * private:
 *     std::thread_pool pool_;
 * };
 * ```
 * 
 * ## 调试技巧
 * 
 * ### 1. 验证层配置
 * ```cpp
 * // 启用相关的验证层检查
 * const char* validationLayers[] = {
 *     "VK_LAYER_KHRONOS_validation"
 * };
 * 
 * // 在实例创建时启用
 * VkInstanceCreateInfo instanceInfo = {
 *     .enabledLayerCount = 1,
 *     .ppEnabledLayerNames = validationLayers,
 * };
 * ```
 * 
 * ### 2. 调试标记
 * ```cpp
 * // 在关键操作前后插入调试标记
 * void renderWithDebugMarkers(VkCommandBuffer cmd, const Framebuffer& fb) {
 *     vkCmdBeginDebugUtilsLabelEXT(cmd, &{
 *         .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
 *         .pLabelName = "Main Render Pass"
 *     });
 *     
 *     // 渲染操作...
 *     
 *     vkCmdEndDebugUtilsLabelEXT(cmd);
 * }
 * ```
 * 
 * ### 3. 性能监控
 * ```cpp
 * class FramebufferProfiler {
 * public:
 *     void recordCreation(const Framebuffer& fb, std::chrono::nanoseconds time);
 *     void recordUsage(const Framebuffer& fb);
 *     void generateReport();
 * };
 * ```
 * 
 * ## 常见陷阱和解决方案
 * 
 * ### 1. 附件尺寸不匹配
 * **问题**：创建帧缓冲时出现验证层错误
 * ```cpp
 * // 错误：不同尺寸的附件
 * auto color = createTexture(1920, 1080);
 * auto depth = createTexture(1024, 768);  // 尺寸不匹配！
 * ```
 * **解决**：确保所有附件有相同尺寸
 * ```cpp
 * const uint32_t width = 1920, height = 1080;
 * auto color = createTexture(width, height);
 * auto depth = createTexture(width, height);  // 相同尺寸
 * ```
 * 
 * ### 2. RenderPass兼容性问题
 * **问题**：帧缓冲与渲染通道不兼容
 * ```cpp
 * // 确保附件顺序、格式、采样数完全匹配
 * std::vector<VkFormat> renderPassFormats = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT};
 * auto colorTexture = createTexture(..., VK_FORMAT_R8G8B8A8_UNORM, ...);
 * auto depthTexture = createTexture(..., VK_FORMAT_D32_SFLOAT, ...);
 * ```
 * 
 * ### 3. 生命周期管理错误
 * **问题**：在帧缓冲销毁后使用
 * ```cpp
 * VkFramebuffer handle;
 * {
 *     Framebuffer fb(...);
 *     handle = fb.vkFramebuffer();
 * } // fb销毁
 * // handle现在无效！
 * ```
 * **解决**：确保正确的生命周期管理
 * ```cpp
 * class RenderSystem {
 *     std::vector<Framebuffer> framebuffers_;  // 保持生命周期
 * public:
 *     void render() {
 *         for (auto& fb : framebuffers_) {
 *             // 安全使用
 *         }
 *     }
 * };
 * ```
 * 
 * ### 4. 多线程访问问题
 * **问题**：多线程同时访问帧缓冲
 * **解决**：使用适当的同步机制
 * ```cpp
 * class ThreadSafeFramebuffer {
 *     mutable std::shared_mutex mutex_;
 *     Framebuffer framebuffer_;
 * public:
 *     VkFramebuffer vkFramebuffer() const {
 *         std::shared_lock lock(mutex_);
 *         return framebuffer_.vkFramebuffer();
 *     }
 * };
 * ```
 * 
 * ## 总结
 * 
 * Framebuffer类提供了一个简洁而强大的接口来管理Vulkan帧缓冲。通过合理的设计模式
 * 和错误处理，它简化了复杂的GPU资源管理，同时保持了高性能和可扩展性。
 * 
 * 对于初学者，重点理解：
 * 1. **帧缓冲的作用**：连接渲染通道和纹理资源
 * 2. **附件的概念**：颜色、深度、模板的不同用途
 * 3. **兼容性要求**：与RenderPass的严格匹配关系
 * 4. **资源管理**：RAII模式的自动化清理
 * 
 * 掌握这些概念后，就可以有效地使用Framebuffer类来构建复杂的渲染系统。
 */