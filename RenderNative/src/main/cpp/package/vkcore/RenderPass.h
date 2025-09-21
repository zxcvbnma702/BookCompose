/**
 * @file RenderPass.h
 * @brief Vulkan渲染通道管理类 - 为初学者详细解释
 * @author nio
 * @date 2025/7/27
 * 
 * ## 什么是RenderPass（渲染通道）？
 * 
 * RenderPass是Vulkan中的核心概念，它定义了渲染操作的"蓝图"。可以把它想象成：
 * - 一个"渲染配方"，告诉GPU如何处理图像数据
 * - 定义了渲染过程中使用的所有"画布"（附件）
 * - 规定了每个"画布"的处理方式（清除、保存、丢弃等）
 * 
 * ## 为什么需要RenderPass？
 * 
 * 在传统图形API中，你可以随时改变渲染目标。但Vulkan要求预先声明：
 * - **性能优化**：GPU驱动可以提前优化内存访问模式
 * - **移动设备友好**：基于瓦片的渲染器（如手机GPU）可以优化带宽使用
 * - **并行性**：多个线程可以安全地记录渲染命令
 * 
 * ## 核心概念解释：
 * 
 * ### 1. 附件（Attachments）
 * 附件就是渲染过程中使用的"画布"：
 * - **颜色附件**：存储最终的颜色结果（如屏幕显示的图像）
 * - **深度附件**：存储深度信息（用于3D物体的前后关系）
 * - **模板附件**：存储模板信息（用于复杂的遮罩效果）
 * - **解析附件**：用于多重采样抗锯齿的最终结果
 * 
 * ### 2. 子通道（Subpasses）
 * 一个RenderPass可以包含多个子通道：
 * - 每个子通道定义一组渲染操作
 * - 子通道间可以共享附件数据
 * - 用于实现延迟渲染等高级技术
 * 
 * ### 3. 依赖关系（Dependencies）
 * 定义子通道间的同步关系：
 * - 确保正确的执行顺序
 * - 管理内存访问冲突
 * - 优化GPU并行执行
 * 
 * @note 本文件提供了三种不同的RenderPass构造方式，适应不同的使用场景
 */

#ifndef BOOKCOMPOSE_RENDERPASS_H
#define BOOKCOMPOSE_RENDERPASS_H

#include <memory>     // 智能指针支持
#include <string>     // 字符串支持
#include <vector>     // 动态数组支持

#include "Common.h"   // Vulkan通用定义
#include "Utils.h"    // 实用工具函数

namespace VkCore{

    // 前向声明 - 避免循环包含
    class Context;   // Vulkan上下文管理类
    class Texture;   // 纹理管理类

    /**
     * @class RenderPass
     * @brief Vulkan渲染通道管理类 - 初学者完整指南
     * 
     * ## 类功能概述
     * 
     * RenderPass类封装了Vulkan渲染通道的创建和管理。它就像是一个"渲染蓝图"，
     * 定义了一次完整渲染操作中需要的所有资源和处理方式。
     * 
     * ## 渲染通道的组成部分
     * 
     * ### 1. 附件描述（VkAttachmentDescription）
     * 定义每个"画布"的属性：
     * ```cpp
     * // 示例：颜色附件
     * VkAttachmentDescription colorAttachment = {
     *     .format = VK_FORMAT_B8G8R8A8_UNORM,           // 像素格式（BGRA 8位）
     *     .samples = VK_SAMPLE_COUNT_1_BIT,             // 采样数（无抗锯齿）
     *     .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,        // 开始时清除内容
     *     .storeOp = VK_ATTACHMENT_STORE_OP_STORE,      // 结束时保存结果
     *     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,   // 初始布局（不关心）
     *     .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR // 最终布局（用于显示）
     * };
     * ```
     * 
     * ### 2. 附件引用（VkAttachmentReference）
     * 在子通道中引用附件：
     * ```cpp
     * VkAttachmentReference colorRef = {
     *     .attachment = 0,                                      // 附件索引
     *     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL    // 使用时的布局
     * };
     * ```
     * 
     * ### 3. 子通道描述（VkSubpassDescription）
     * 定义渲染操作的具体内容：
     * - **颜色附件**：输出颜色的地方
     * - **深度附件**：处理3D深度的地方
     * - **输入附件**：从上一个子通道读取数据
     * - **解析附件**：多重采样的最终输出
     * 
     * ### 4. 子通道依赖（VkSubpassDependency）
     * 确保正确的执行顺序和内存同步：
     * ```cpp
     * VkSubpassDependency dependency = {
     *     .srcSubpass = VK_SUBPASS_EXTERNAL,              // 外部操作
     *     .dstSubpass = 0,                                // 我们的子通道
     *     .srcStageMask = VK_PIPELINE_STAGE_...,          // 等待的阶段
     *     .dstStageMask = VK_PIPELINE_STAGE_...,          // 开始的阶段
     *     .srcAccessMask = VK_ACCESS_...,                 // 等待的访问类型
     *     .dstAccessMask = VK_ACCESS_...                  // 需要的访问类型
     * };
     * ```
     * 
     * ## 三种构造函数的用途
     * 
     * 1. **基于Texture对象的构造函数**：
     *    - 适用于已有纹理对象的场景
     *    - 自动从纹理获取格式和属性
     *    - 最简单易用的方式
     * 
     * 2. **基于格式的标准构造函数**：
     *    - 适用于已知格式但没有纹理对象的场景
     *    - 支持多视图渲染（VR应用）
     *    - 灵活性较高
     * 
     * 3. **支持片段密度图的构造函数**：
     *    - 适用于高级渲染技术
     *    - 支持可变分辨率着色（VRS）
     *    - 移动设备性能优化
     * 
     * ## 常见使用模式
     * 
     * ### 简单颜色渲染
     * ```cpp
     * // 创建一个简单的颜色渲染通道
     * std::vector<VkFormat> formats = {VK_FORMAT_B8G8R8A8_UNORM};
     * std::vector<VkImageLayout> initialLayouts = {VK_IMAGE_LAYOUT_UNDEFINED};
     * std::vector<VkImageLayout> finalLayouts = {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
     * std::vector<VkAttachmentLoadOp> loadOps = {VK_ATTACHMENT_LOAD_OP_CLEAR};
     * std::vector<VkAttachmentStoreOp> storeOps = {VK_ATTACHMENT_STORE_OP_STORE};
     * 
     * RenderPass renderPass(context, formats, initialLayouts, finalLayouts,
     *                       loadOps, storeOps, VK_PIPELINE_BIND_POINT_GRAPHICS,
     *                       {}, UINT32_MAX);
     * ```
     * 
     * ### 带深度缓冲的渲染
     * ```cpp
     * std::vector<VkFormat> formats = {
     *     VK_FORMAT_B8G8R8A8_UNORM,    // 颜色附件
     *     VK_FORMAT_D32_SFLOAT         // 深度附件
     * };
     * // ... 其他设置
     * RenderPass renderPass(context, formats, initialLayouts, finalLayouts,
     *                       loadOps, storeOps, VK_PIPELINE_BIND_POINT_GRAPHICS,
     *                       {}, 1);  // 深度附件索引为1
     * ```
     * 
     * @note 使用final关键字防止继承，确保资源管理的安全性
     * @warning 所有附件的索引必须有效，否则会导致验证层错误
     */
    class RenderPass final{
    public:
        /**
         * @brief 移动语义宏 - 只允许移动，禁止复制
         * 
         * RenderPass管理昂贵的GPU资源，不应该被意外复制。
         * 只允许移动语义确保资源所有权的唯一性。
         */
        MOVABLE_ONLY(RenderPass);

        /**
         * @brief 基于Texture对象的构造函数 - 最简单的创建方式
         * 
         * 这是最直观的构造函数，直接使用已创建的纹理对象。
         * 适用于已经有纹理对象的场景，如离屏渲染或后处理效果。
         * 
         * ## 工作原理：
         * 1. 从纹理对象自动提取格式、采样数等属性
         * 2. 根据纹理类型自动分类为颜色或深度附件
         * 3. 处理MSAA解析附件（如果提供）
         * 4. 创建标准的单子通道渲染通道
         * 
         * ## 使用场景：
         * - 离屏渲染到纹理
         * - 后处理效果链
         * - 阴影贴图生成
         * - 反射/折射纹理
         * 
         * @param context Vulkan上下文，提供设备和调试信息
         * @param attachments 主要附件列表（纹理对象）
         * @param resolveAttachments MSAA解析附件列表（可以为空）
         * @param loadOp 每个附件的加载操作：
         *   - VK_ATTACHMENT_LOAD_OP_LOAD: 保留现有内容
         *   - VK_ATTACHMENT_LOAD_OP_CLEAR: 清除为指定值
         *   - VK_ATTACHMENT_LOAD_OP_DONT_CARE: 不关心现有内容
         * @param storeOp 每个附件的存储操作：
         *   - VK_ATTACHMENT_STORE_OP_STORE: 保存渲染结果
         *   - VK_ATTACHMENT_STORE_OP_DONT_CARE: 不关心结果（性能优化）
         * @param layout 每个附件的最终布局
         * @param bindPoint 管线绑定点（通常为VK_PIPELINE_BIND_POINT_GRAPHICS）
         * @param name 调试名称，便于GPU调试工具识别
         * 
         * @note 附件、loadOp、storeOp、layout数组的大小必须匹配
         * @warning 深度/模板纹理会自动识别并设置为深度附件
         */
        RenderPass(const Context& context,
                   std::vector<std::shared_ptr<Texture>> attachments,
                   std::vector<std::shared_ptr<Texture>> resolveAttachments,
                   const std::vector<VkAttachmentLoadOp>& loadOp,
                   const std::vector<VkAttachmentStoreOp>& storeOp,
                   const std::vector<VkImageLayout>& layout, 
                   VkPipelineBindPoint bindPoint,
                   const std::string& name = "");

        /**
         * @brief 基于格式的标准构造函数 - 最灵活的创建方式
         * 
         * 这是最灵活的构造函数，允许完全自定义所有参数。
         * 适用于需要精确控制渲染通道行为的场景。
         * 
         * ## 主要特点：
         * - 支持多视图渲染（VR/AR应用）
         * - 可以指定特定的深度和模板附件
         * - 支持自定义的模板操作
         * - 完全控制所有附件属性
         * 
         * ## 使用场景：
         * - VR/AR双眼渲染
         * - 复杂的多附件渲染
         * - 需要精确控制布局转换的场景
         * - 自定义深度/模板测试
         * 
         * @param context Vulkan上下文
         * @param formats 所有附件的像素格式数组
         * @param initialLayouts 每个附件的初始布局
         * @param finalLayouts 每个附件的最终布局
         * @param loadOp 每个附件的加载操作
         * @param storeOp 每个附件的存储操作
         * @param bindPoint 管线绑定点
         * @param resolveAttachmentsIndices MSAA解析附件的索引列表
         * @param depthAttachmentIndex 深度附件的索引（UINT32_MAX表示无深度）
         * @param stencilAttachmentIndex 模板附件的索引（默认与深度相同）
         * @param stencilLoadOp 模板的加载操作
         * @param stencilStoreOp 模板的存储操作
         * @param multiview 是否启用多视图渲染
         * @param name 调试名称
         * 
         * @note 所有数组的大小必须匹配
         * @warning 深度和模板索引必须指向有效的附件
         */
        RenderPass(const Context& context, 
                   const std::vector<VkFormat>& formats,
                   const std::vector<VkImageLayout>& initialLayouts,
                   const std::vector<VkImageLayout>& finalLayouts,
                   const std::vector<VkAttachmentLoadOp>& loadOp,
                   const std::vector<VkAttachmentStoreOp>& storeOp,
                   VkPipelineBindPoint bindPoint,
                   std::vector<uint32_t> resolveAttachmentsIndices,
                   uint32_t depthAttachmentIndex, 
                   uint32_t stencilAttachmentIndex = UINT32_MAX,
                   VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                   VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                   bool multiview = false, 
                   const std::string& name = "");

        /**
         * @brief 支持片段密度图的高级构造函数 - 性能优化专用
         * 
         * 这是最高级的构造函数，支持Vulkan的片段密度图扩展。
         * 片段密度图允许在不同区域使用不同的着色分辨率，
         * 是移动设备和VR应用的重要性能优化技术。
         * 
         * ## 片段密度图的作用：
         * - **可变分辨率着色**：中心区域高分辨率，边缘区域低分辨率
         * - **性能优化**：减少不重要区域的着色开销
         * - **VR优化**：模拟人眼的视觉特性
         * - **移动设备节能**：降低GPU功耗
         * 
         * ## 工作原理：
         * 1. 片段密度图定义每个区域的着色密度
         * 2. GPU根据密度图调整着色频率
         * 3. 高密度区域：每个像素都着色
         * 4. 低密度区域：多个像素共享一次着色结果
         * 
         * @param context Vulkan上下文
         * @param formats 所有附件的像素格式数组
         * @param initialLayouts 每个附件的初始布局
         * @param finalLayouts 每个附件的最终布局
         * @param loadOp 每个附件的加载操作
         * @param storeOp 每个附件的存储操作
         * @param bindPoint 管线绑定点
         * @param resolveAttachmentsIndices MSAA解析附件的索引列表
         * @param depthAttachmentIndex 深度附件的索引
         * @param fragmentDensityMapIndex 片段密度图附件的索引
         * @param stencilAttachmentIndex 模板附件的索引
         * @param stencilLoadOp 模板的加载操作
         * @param stencilStoreOp 模板的存储操作
         * @param multiview 是否启用多视图渲染
         * @param name 调试名称
         * 
         * @note 需要设备支持VK_EXT_fragment_density_map扩展
         * @warning 片段密度图必须使用特定的格式和布局
         */
        RenderPass(const Context& context, 
                   const std::vector<VkFormat>& formats,
                   const std::vector<VkImageLayout>& initialLayouts,
                   const std::vector<VkImageLayout>& finalLayouts,
                   const std::vector<VkAttachmentLoadOp>& loadOp,
                   const std::vector<VkAttachmentStoreOp>& storeOp,
                   VkPipelineBindPoint bindPoint,
                   std::vector<uint32_t> resolveAttachmentsIndices,
                   uint32_t depthAttachmentIndex, 
                   uint32_t fragmentDensityMapIndex,
                   uint32_t stencilAttachmentIndex = UINT32_MAX,
                   VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                   VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                   bool multiview = false, 
                   const std::string& name = "");

        /**
         * @brief 析构函数 - 自动清理GPU资源
         * 
         * 使用RAII模式自动释放VkRenderPass对象。
         * 确保GPU资源不会泄漏。
         */
        ~RenderPass();

        /**
         * @brief 获取底层的VkRenderPass对象
         * 
         * 返回原生的Vulkan渲染通道句柄，用于：
         * - 创建图形管线
         * - 创建帧缓冲
         * - 开始渲染通道
         * 
         * @return VkRenderPass 原生Vulkan渲染通道句柄
         * @note [[nodiscard]]属性提醒调用者不要忽略返回值
         */
        [[nodiscard]] VkRenderPass vkRenderPass() const;

    private:
        /**
         * @brief Vulkan逻辑设备句柄
         * 
         * 用于创建和销毁VkRenderPass对象。
         * 在析构函数中需要使用此句柄来清理资源。
         */
        VkDevice device_ = VK_NULL_HANDLE;
        
        /**
         * @brief Vulkan渲染通道句柄
         * 
         * 实际的GPU资源，定义了渲染操作的所有参数。
         * 这是最终被图形管线和帧缓冲使用的对象。
         */
        VkRenderPass renderPass_ = VK_NULL_HANDLE;
    };

} // namespace VkCore

#endif //BOOKCOMPOSE_RENDERPASS_H

/**
 * @page renderpass_usage_guide RenderPass使用指南
 * 
 * ## 概述
 * 
 * RenderPass是Vulkan渲染管线的核心组件，它定义了渲染操作的"蓝图"。
 * 本指南将帮助初学者理解如何正确使用RenderPass类。
 * 
 * ## 基础概念
 * 
 * ### 什么时候需要RenderPass？
 * 在Vulkan中，任何渲染操作都需要RenderPass：
 * - 绘制到屏幕
 * - 离屏渲染
 * - 后处理效果
 * - 阴影贴图生成
 * 
 * ### RenderPass vs FrameBuffer
 * - **RenderPass**：定义"如何渲染"（格式、操作等）
 * - **FrameBuffer**：定义"渲染到哪里"（具体的图像）
 * 
 * 可以把RenderPass想象成"食谱"，FrameBuffer想象成"具体的食材"。
 * 
 * ## 使用示例
 * 
 * ### 1. 简单的屏幕渲染
 * ```cpp
 * // 最简单的情况：渲染到屏幕
 * std::vector<VkFormat> formats = {VK_FORMAT_B8G8R8A8_UNORM};
 * std::vector<VkImageLayout> initialLayouts = {VK_IMAGE_LAYOUT_UNDEFINED};
 * std::vector<VkImageLayout> finalLayouts = {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
 * std::vector<VkAttachmentLoadOp> loadOps = {VK_ATTACHMENT_LOAD_OP_CLEAR};
 * std::vector<VkAttachmentStoreOp> storeOps = {VK_ATTACHMENT_STORE_OP_STORE};
 * 
 * RenderPass screenRenderPass(
 *     context,
 *     formats,
 *     initialLayouts,
 *     finalLayouts,
 *     loadOps,
 *     storeOps,
 *     VK_PIPELINE_BIND_POINT_GRAPHICS,
 *     {},              // 无解析附件
 *     UINT32_MAX,      // 无深度附件
 *     UINT32_MAX,      // 无模板附件
 *     VK_ATTACHMENT_LOAD_OP_DONT_CARE,
 *     VK_ATTACHMENT_STORE_OP_DONT_CARE,
 *     false,           // 非多视图
 *     "Screen Render Pass"
 * );
 * ```
 * 
 * ### 2. 带深度测试的3D渲染
 * ```cpp
 * std::vector<VkFormat> formats = {
 *     VK_FORMAT_B8G8R8A8_UNORM,    // 颜色附件
 *     VK_FORMAT_D32_SFLOAT         // 深度附件
 * };
 * 
 * std::vector<VkImageLayout> initialLayouts = {
 *     VK_IMAGE_LAYOUT_UNDEFINED,           // 颜色：不关心初始内容
 *     VK_IMAGE_LAYOUT_UNDEFINED            // 深度：不关心初始内容
 * };
 * 
 * std::vector<VkImageLayout> finalLayouts = {
 *     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,     // 颜色：用于显示
 *     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL  // 深度：保持深度格式
 * };
 * 
 * std::vector<VkAttachmentLoadOp> loadOps = {
 *     VK_ATTACHMENT_LOAD_OP_CLEAR,         // 颜色：清除为背景色
 *     VK_ATTACHMENT_LOAD_OP_CLEAR          // 深度：清除为最大深度
 * };
 * 
 * std::vector<VkAttachmentStoreOp> storeOps = {
 *     VK_ATTACHMENT_STORE_OP_STORE,        // 颜色：保存结果
 *     VK_ATTACHMENT_STORE_OP_DONT_CARE     // 深度：不关心结果
 * };
 * 
 * RenderPass depthRenderPass(
 *     context,
 *     formats,
 *     initialLayouts,
 *     finalLayouts,
 *     loadOps,
 *     storeOps,
 *     VK_PIPELINE_BIND_POINT_GRAPHICS,
 *     {},              // 无解析附件
 *     1,               // 深度附件索引为1
 *     UINT32_MAX,      // 无单独模板附件
 *     VK_ATTACHMENT_LOAD_OP_DONT_CARE,
 *     VK_ATTACHMENT_STORE_OP_DONT_CARE,
 *     false,
 *     "3D Render Pass with Depth"
 * );
 * ```
 * 
 * ### 3. 离屏渲染（使用纹理对象）
 * ```cpp
 * // 创建离屏渲染纹理
 * auto colorTexture = std::make_shared<Texture>(
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
 *     0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
 *     VkExtent3D{1024, 1024, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Offscreen Color"
 * );
 * 
 * std::vector<std::shared_ptr<Texture>> attachments = {colorTexture};
 * std::vector<std::shared_ptr<Texture>> resolveAttachments = {};  // 无MSAA
 * std::vector<VkAttachmentLoadOp> loadOps = {VK_ATTACHMENT_LOAD_OP_CLEAR};
 * std::vector<VkAttachmentStoreOp> storeOps = {VK_ATTACHMENT_STORE_OP_STORE};
 * std::vector<VkImageLayout> finalLayouts = {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
 * 
 * RenderPass offscreenRenderPass(
 *     context,
 *     attachments,
 *     resolveAttachments,
 *     loadOps,
 *     storeOps,
 *     finalLayouts,
 *     VK_PIPELINE_BIND_POINT_GRAPHICS,
 *     "Offscreen Render Pass"
 * );
 * ```
 * 
 * ### 4. VR双眼渲染（多视图）
 * ```cpp
 * std::vector<VkFormat> formats = {VK_FORMAT_B8G8R8A8_UNORM};
 * // ... 其他设置与普通渲染相同 ...
 * 
 * RenderPass vrRenderPass(
 *     context,
 *     formats,
 *     initialLayouts,
 *     finalLayouts,
 *     loadOps,
 *     storeOps,
 *     VK_PIPELINE_BIND_POINT_GRAPHICS,
 *     {},
 *     UINT32_MAX,
 *     UINT32_MAX,
 *     VK_ATTACHMENT_LOAD_OP_DONT_CARE,
 *     VK_ATTACHMENT_STORE_OP_DONT_CARE,
 *     true,            // 启用多视图！
 *     "VR Stereo Render Pass"
 * );
 * ```
 * 
 * ## 常见问题和解决方案
 * 
 * ### 1. 验证层错误："attachment index out of bounds"
 * **原因**：附件索引超出了附件数组的范围
 * **解决**：确保所有索引都小于附件数量
 * 
 * ### 2. 渲染结果是黑屏
 * **原因**：可能是loadOp设置错误
 * **解决**：检查是否使用了正确的VK_ATTACHMENT_LOAD_OP_CLEAR
 * 
 * ### 3. 性能问题
 * **原因**：不必要的STORE操作
 * **解决**：对不需要的附件使用VK_ATTACHMENT_STORE_OP_DONT_CARE
 * 
 * ### 4. 布局转换错误
 * **原因**：initialLayout和finalLayout不匹配实际使用
 * **解决**：确保布局与后续操作兼容
 * 
 * ## 性能优化建议
 * 
 * ### 1. 选择合适的LoadOp
 * - **CLEAR**：需要清除内容时使用
 * - **LOAD**：需要保留现有内容时使用
 * - **DONT_CARE**：不关心内容时使用（最快）
 * 
 * ### 2. 选择合适的StoreOp
 * - **STORE**：需要保存结果时使用
 * - **DONT_CARE**：不关心结果时使用（节省带宽）
 * 
 * ### 3. 移动设备优化
 * - 尽量使用VK_ATTACHMENT_STORE_OP_DONT_CARE
 * - 考虑使用片段密度图
 * - 避免不必要的布局转换
 * 
 * ### 4. VR应用优化
 * - 使用多视图渲染减少绘制调用
 * - 考虑使用片段密度图模拟注视点渲染
 * - 使用合适的MSAA级别平衡质量和性能
 */
