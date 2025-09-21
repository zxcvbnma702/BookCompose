/**
 * @file FrameBuffer.h
 * @brief Vulkan帧缓冲管理类 - 为初学者详细解释
 * @author nio
 * @date 2025/7/28
 * 
 * ## 什么是FrameBuffer（帧缓冲）？
 * 
 * FrameBuffer是Vulkan中的重要概念，它定义了渲染操作的"画布"。可以把它想象成：
 * - 一个"画板"，包含了所有要绘制的"画纸"（附件）
 * - 连接RenderPass（渲染蓝图）和实际的图像资源
 * - 指定了渲染的具体目标和尺寸
 * 
 * ## FrameBuffer vs RenderPass 的关系
 * 
 * 用一个简单的比喻来理解：
 * - **RenderPass**：就像是一个"菜谱"，定义了烹饪的步骤和要用什么类型的食材
 * - **FrameBuffer**：就像是"具体的食材"，实际要处理的食物
 * - **渲染过程**：就是按照菜谱（RenderPass）处理具体食材（FrameBuffer）
 * 
 * 更具体地说：
 * - RenderPass定义："我需要一个颜色附件和一个深度附件"
 * - FrameBuffer提供："这里有一个1920x1080的颜色纹理和一个1920x1080的深度纹理"
 * 
 * ## 核心概念解释：
 * 
 * ### 1. 附件（Attachments）
 * 附件是FrameBuffer的核心组成部分：
 * - **颜色附件**：存储渲染的颜色结果（如最终的图像）
 * - **深度附件**：存储深度信息（用于3D物体的前后关系）
 * - **模板附件**：存储模板信息（用于复杂的遮罩效果）
 * 
 * ### 2. 兼容性
 * FrameBuffer必须与RenderPass兼容：
 * - 附件数量必须匹配
 * - 附件格式必须匹配
 * - 附件的用途必须匹配
 * 
 * ### 3. 尺寸
 * FrameBuffer有固定的尺寸：
 * - 所有附件必须有相同的宽度和高度
 * - 决定了渲染的分辨率
 * - 影响性能和内存使用
 * 
 * ## 使用场景：
 * 
 * ### 1. 屏幕渲染
 * ```cpp
 * // 渲染到屏幕的交换链图像
 * auto colorTexture = swapchainImage;
 * auto depthTexture = depthBuffer;
 * Framebuffer screenFramebuffer(context, device, renderPass, 
 *                               {colorTexture}, depthTexture, nullptr);
 * ```
 * 
 * ### 2. 离屏渲染
 * ```cpp
 * // 渲染到纹理（如阴影贴图、后处理）
 * auto offscreenColor = createOffscreenTexture(1024, 1024);
 * auto offscreenDepth = createDepthTexture(1024, 1024);
 * Framebuffer offscreenFramebuffer(context, device, renderPass,
 *                                  {offscreenColor}, offscreenDepth, nullptr);
 * ```
 * 
 * ### 3. 多渲染目标（MRT）
 * ```cpp
 * // 同时渲染到多个颜色附件
 * std::vector<std::shared_ptr<Texture>> colorAttachments = {
 *     albedoTexture,    // 基础颜色
 *     normalTexture,    // 法线信息
 *     roughnessTexture  // 粗糙度信息
 * };
 * Framebuffer mrtFramebuffer(context, device, renderPass,
 *                            colorAttachments, depthTexture, nullptr);
 * ```
 * 
 * @note 本文件提供了简化的FrameBuffer创建接口，自动处理复杂的配置
 * @warning 确保所有附件的尺寸一致，否则会导致创建失败
 */

#ifndef BOOKCOMPOSE_FRAMEBUFFER_H
#define BOOKCOMPOSE_FRAMEBUFFER_H

#include <memory>     // 智能指针支持
#include <vector>     // 动态数组支持

#include "Common.h"   // Vulkan通用定义
#include "Utils.h"    // 实用工具函数

namespace VkCore {

    // 前向声明 - 避免循环包含
    class Context;   // Vulkan上下文管理类
    class Texture;   // 纹理管理类

    /**
     * @class Framebuffer
     * @brief Vulkan帧缓冲管理类 - 初学者完整指南
     * 
     * ## 类功能概述
     * 
     * Framebuffer类封装了Vulkan帧缓冲的创建和管理。它就像是一个"画板"，
     * 将多个纹理（附件）组合在一起，为渲染操作提供具体的目标。
     * 
     * ## 帧缓冲的工作流程
     * 
     * 1. **创建阶段**：
     *    - 收集所有需要的纹理附件
     *    - 验证附件的兼容性和尺寸
     *    - 创建VkFramebuffer对象
     * 
     * 2. **使用阶段**：
     *    - 在渲染通道中指定此帧缓冲
     *    - GPU将渲染结果写入到附件纹理中
     * 
     * 3. **清理阶段**：
     *    - 自动释放VkFramebuffer资源
     *    - 附件纹理由各自的Texture对象管理
     * 
     * ## 设计特点
     * 
     * ### 1. 简化的接口
     * 只需要提供纹理对象，自动处理：
     * - ImageView的创建和管理
     * - 附件顺序的正确排列
     * - 尺寸验证和错误检查
     * 
     * ### 2. 灵活的附件支持
     * - **多颜色附件**：支持多渲染目标（MRT）
     * - **可选深度附件**：3D渲染的深度测试
     * - **可选模板附件**：复杂的遮罩效果
     * 
     * ### 3. RAII资源管理
     * - 构造时自动创建GPU资源
     * - 析构时自动清理GPU资源
     * - 移动语义避免不必要的复制
     * 
     * ## 使用模式
     * 
     * ### 基础用法
     * ```cpp
     * // 创建简单的颜色+深度帧缓冲
     * auto colorTexture = std::make_shared<Texture>(...);
     * auto depthTexture = std::make_shared<Texture>(...);
     * 
     * Framebuffer framebuffer(context, device, renderPass,
     *                         {colorTexture},  // 颜色附件列表
     *                         depthTexture,    // 深度附件
     *                         nullptr,         // 无模板附件
     *                         "Main Framebuffer");
     * ```
     * 
     * ### 多渲染目标用法
     * ```cpp
     * // 延迟渲染的G-Buffer
     * std::vector<std::shared_ptr<Texture>> gBufferTextures = {
     *     albedoTexture,     // 基础颜色
     *     normalTexture,     // 世界空间法线
     *     materialTexture    // 材质属性
     * };
     * 
     * Framebuffer gBufferFramebuffer(context, device, renderPass,
     *                                gBufferTextures,
     *                                depthTexture,
     *                                nullptr,
     *                                "G-Buffer");
     * ```
     * 
     * @note 使用final关键字防止继承，确保资源管理的安全性
     * @warning 所有附件必须有相同的尺寸，否则创建会失败
     */
    class Framebuffer final {
    public:
        /**
         * @brief 移动语义宏 - 只允许移动，禁止复制
         * 
         * Framebuffer管理昂贵的GPU资源，不应该被意外复制。
         * 只允许移动语义确保资源所有权的唯一性。
         */
        MOVABLE_ONLY(Framebuffer);

        /**
         * @brief 构造函数 - 创建Vulkan帧缓冲
         * 
         * 这个构造函数将多个纹理附件组合成一个帧缓冲对象。
         * 它自动处理ImageView的创建、尺寸验证和GPU资源管理。
         * 
         * ## 工作原理：
         * 1. **收集ImageView**：从每个纹理对象获取VkImageView
         * 2. **验证兼容性**：检查附件数量和尺寸的一致性
         * 3. **确定尺寸**：从第一个有效附件获取帧缓冲尺寸
         * 4. **创建帧缓冲**：调用vkCreateFramebuffer创建GPU对象
         * 5. **设置调试名称**：便于GPU调试工具识别
         * 
         * ## 附件顺序：
         * 最终的附件顺序将是：
         * 1. 所有颜色附件（按vector顺序）
         * 2. 深度附件（如果提供）
         * 3. 模板附件（如果提供）
         * 
         * 这个顺序必须与RenderPass中的附件描述顺序完全匹配。
         * 
         * ## 尺寸规则：
         * - 如果有颜色附件，使用第一个颜色附件的尺寸
         * - 如果没有颜色附件，使用深度附件的尺寸
         * - 所有附件的尺寸必须完全一致
         * 
         * @param context Vulkan上下文，提供调试信息设置功能
         * @param device Vulkan逻辑设备，用于创建帧缓冲对象
         * @param renderPass 兼容的渲染通道，定义了附件的格式和用途
         * @param attachments 颜色附件列表，可以为空但通常至少有一个
         * @param depthAttachment 深度附件，可以为nullptr（无深度测试）
         * @param stencilAttachment 模板附件，可以为nullptr（无模板测试）
         * @param name 调试名称，用于GPU调试工具识别
         * 
         * @note 构造函数是explicit的，避免意外的隐式转换
         * @warning renderPass必须与提供的附件兼容，否则会导致验证层错误
         * @warning 至少要提供一个附件（颜色或深度），否则会断言失败
         */
        explicit Framebuffer(const Context &context, VkDevice device, VkRenderPass renderPass,
                             const std::vector<std::shared_ptr<Texture>> &attachments,
                             const std::shared_ptr<Texture> depthAttachment,
                             const std::shared_ptr<Texture> stencilAttachment,
                             const std::string &name = "");

        /**
         * @brief 析构函数 - 自动清理GPU资源
         * 
         * 使用RAII模式自动释放VkFramebuffer对象。
         * 
         * ## 清理过程：
         * 1. 调用vkDestroyFramebuffer释放GPU资源
         * 2. 使用之前保存的device_句柄
         * 3. 传递nullptr作为分配器（使用默认分配器）
         * 
         * ## 重要说明：
         * - 只清理VkFramebuffer对象本身
         * - 不会影响附件纹理（由Texture对象管理）
         * - 不会影响RenderPass（由RenderPass对象管理）
         * 
         * @note 在对象销毁时自动调用，无需手动管理
         * @warning 必须确保没有其他对象正在使用此Framebuffer
         */
        ~Framebuffer();

        /**
         * @brief 获取底层的VkFramebuffer对象
         * 
         * 返回原生的Vulkan帧缓冲句柄，用于与其他Vulkan对象交互。
         * 这是一个const函数，不会修改对象状态。
         * 
         * ## 常见用途：
         * - 调用vkCmdBeginRenderPass时传递framebuffer参数
         * - 查询帧缓冲的属性和兼容性
         * - 与其他Vulkan API函数交互
         * 
         * ## 使用示例：
         * ```cpp
         * VkRenderPassBeginInfo beginInfo = {};
         * beginInfo.framebuffer = framebuffer.vkFramebuffer();
         * vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
         * ```
         * 
         * @return VkFramebuffer 原生Vulkan帧缓冲句柄
         * 
         * @note [[nodiscard]]属性提醒调用者不要忽略返回值
         * @warning 返回的句柄只在此对象生命周期内有效
         */
        [[nodiscard]] VkFramebuffer vkFramebuffer() const;

    private:
        /**
         * @brief Vulkan逻辑设备句柄
         * 
         * 用于创建和销毁VkFramebuffer对象。
         * 在析构函数中需要使用此句柄来清理资源。
         */
        VkDevice device_ = VK_NULL_HANDLE;
        
        /**
         * @brief Vulkan帧缓冲句柄
         * 
         * 实际的GPU资源，定义了渲染操作的目标附件。
         * 这是最终被渲染命令使用的对象。
         */
        VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    };

} // VkCore

#endif //BOOKCOMPOSE_FRAMEBUFFER_H

/**
 * @page framebuffer_usage_guide Framebuffer使用指南
 * 
 * ## 概述
 * 
 * Framebuffer是Vulkan渲染管线的重要组件，它将RenderPass（渲染蓝图）与具体的纹理资源连接起来。
 * 本指南将帮助初学者理解如何正确使用Framebuffer类。
 * 
 * ## 基础概念
 * 
 * ### RenderPass vs Framebuffer 的关系
 * 
 * 可以用做菜的比喻来理解：
 * - **RenderPass**：菜谱，定义了"需要什么类型的食材，按什么步骤处理"
 * - **Framebuffer**：具体的食材，提供了"实际的蔬菜、肉类等"
 * - **渲染过程**：按照菜谱（RenderPass）处理具体食材（Framebuffer）
 * 
 * ### 兼容性要求
 * 
 * Framebuffer必须与RenderPass兼容：
 * 
 * | RenderPass定义 | Framebuffer提供 | 说明 |
 * |---|---|---|
 * | 2个颜色附件 | 2个颜色纹理 | 数量必须匹配 |
 * | VK_FORMAT_R8G8B8A8_UNORM | RGBA8格式的纹理 | 格式必须匹配 |
 * | VK_SAMPLE_COUNT_4_BIT | 4x MSAA纹理 | 采样数必须匹配 |
 * 
 * ## 使用示例
 * 
 * ### 1. 简单的屏幕渲染
 * ```cpp
 * // 最简单的情况：渲染到屏幕
 * 
 * // 1. 创建纹理（通常来自交换链）
 * auto colorTexture = swapchain.getCurrentImage();  // 来自交换链
 * auto depthTexture = std::make_shared<Texture>(   // 手动创建深度纹理
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT,
 *     0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Depth Buffer"
 * );
 * 
 * // 2. 创建兼容的RenderPass
 * std::vector<VkFormat> formats = {
 *     VK_FORMAT_B8G8R8A8_UNORM,  // 颜色附件格式
 *     VK_FORMAT_D32_SFLOAT       // 深度附件格式
 * };
 * // ... 其他RenderPass参数 ...
 * RenderPass renderPass(context, formats, ...);
 * 
 * // 3. 创建Framebuffer
 * Framebuffer framebuffer(context, device, renderPass.vkRenderPass(),
 *                         {colorTexture},  // 颜色附件列表
 *                         depthTexture,    // 深度附件
 *                         nullptr,         // 无模板附件
 *                         "Screen Framebuffer");
 * 
 * // 4. 使用Framebuffer进行渲染
 * VkRenderPassBeginInfo beginInfo = {
 *     .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
 *     .renderPass = renderPass.vkRenderPass(),
 *     .framebuffer = framebuffer.vkFramebuffer(),  // 使用我们的帧缓冲
 *     .renderArea = {{0, 0}, {1920, 1080}},
 *     // ... 其他参数 ...
 * };
 * vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
 * ```
 * 
 * ### 2. 离屏渲染（渲染到纹理）
 * ```cpp
 * // 渲染到纹理，用于后处理或阴影贴图
 * 
 * // 1. 创建离屏渲染纹理
 * auto offscreenColor = std::make_shared<Texture>(
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
 *     0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
 *     VkExtent3D{1024, 1024, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Offscreen Color"
 * );
 * 
 * auto offscreenDepth = std::make_shared<Texture>(
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT,
 *     0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
 *     VkExtent3D{1024, 1024, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Offscreen Depth"
 * );
 * 
 * // 2. 创建对应的RenderPass
 * std::vector<VkFormat> offscreenFormats = {
 *     VK_FORMAT_R8G8B8A8_UNORM,
 *     VK_FORMAT_D32_SFLOAT
 * };
 * // ... 配置RenderPass ...
 * RenderPass offscreenRenderPass(context, offscreenFormats, ...);
 * 
 * // 3. 创建离屏Framebuffer
 * Framebuffer offscreenFramebuffer(context, device, offscreenRenderPass.vkRenderPass(),
 *                                  {offscreenColor},
 *                                  offscreenDepth,
 *                                  nullptr,
 *                                  "Offscreen Framebuffer");
 * 
 * // 4. 渲染到离屏缓冲
 * // ... 渲染命令 ...
 * 
 * // 5. 在后续渲染中使用离屏纹理
 * // offscreenColor现在包含了渲染结果，可以作为普通纹理使用
 * ```
 * 
 * ### 3. 多渲染目标（MRT）- 延迟渲染
 * ```cpp
 * // 延迟渲染的G-Buffer，同时输出到多个纹理
 * 
 * // 1. 创建G-Buffer纹理
 * auto albedoTexture = std::make_shared<Texture>(     // 基础颜色
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
 *     0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Albedo Buffer"
 * );
 * 
 * auto normalTexture = std::make_shared<Texture>(     // 法线信息
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT,
 *     0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Normal Buffer"
 * );
 * 
 * auto materialTexture = std::make_shared<Texture>(   // 材质属性
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
 *     0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Material Buffer"
 * );
 * 
 * auto depthTexture = std::make_shared<Texture>(      // 深度缓冲
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT,
 *     0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "G-Buffer Depth"
 * );
 * 
 * // 2. 创建多附件RenderPass
 * std::vector<VkFormat> gBufferFormats = {
 *     VK_FORMAT_R8G8B8A8_UNORM,        // albedo
 *     VK_FORMAT_R16G16B16A16_SFLOAT,   // normal
 *     VK_FORMAT_R8G8B8A8_UNORM,        // material
 *     VK_FORMAT_D32_SFLOAT             // depth
 * };
 * // ... 配置RenderPass支持多个颜色输出 ...
 * RenderPass gBufferRenderPass(context, gBufferFormats, ...);
 * 
 * // 3. 创建G-Buffer Framebuffer
 * std::vector<std::shared_ptr<Texture>> gBufferAttachments = {
 *     albedoTexture,
 *     normalTexture,
 *     materialTexture
 * };
 * 
 * Framebuffer gBufferFramebuffer(context, device, gBufferRenderPass.vkRenderPass(),
 *                                gBufferAttachments,  // 多个颜色附件
 *                                depthTexture,        // 深度附件
 *                                nullptr,             // 无模板附件
 *                                "G-Buffer Framebuffer");
 * 
 * // 4. 在着色器中同时输出到多个附件
 * // Fragment Shader输出：
 * // layout(location = 0) out vec4 albedo;    -> albedoTexture
 * // layout(location = 1) out vec4 normal;    -> normalTexture
 * // layout(location = 2) out vec4 material;  -> materialTexture
 * ```
 * 
 * ### 4. MSAA抗锯齿渲染
 * ```cpp
 * // 多重采样抗锯齿渲染
 * 
 * // 1. 创建MSAA纹理
 * auto msaaColorTexture = std::make_shared<Texture>(
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
 *     0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_4_BIT, "MSAA Color"  // 4x MSAA
 * );
 * 
 * auto msaaDepthTexture = std::make_shared<Texture>(
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT,
 *     0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_4_BIT, "MSAA Depth"  // 4x MSAA
 * );
 * 
 * // 2. 创建解析目标（单采样）
 * auto resolveTexture = std::make_shared<Texture>(
 *     context, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
 *     0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
 *     VkExtent3D{1920, 1080, 1}, 1, 1,
 *     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
 *     false, VK_SAMPLE_COUNT_1_BIT, "Resolve Target"  // 单采样
 * );
 * 
 * // 3. 创建支持MSAA解析的RenderPass
 * // 需要在RenderPass构造时指定解析附件
 * RenderPass msaaRenderPass(context, 
 *                          {msaaColorTexture}, {resolveTexture},  // 主要附件和解析附件
 *                          loadOps, storeOps, layouts, bindPoint, "MSAA Pass");
 * 
 * // 4. 创建MSAA Framebuffer
 * Framebuffer msaaFramebuffer(context, device, msaaRenderPass.vkRenderPass(),
 *                             {msaaColorTexture},  // MSAA颜色附件
 *                             msaaDepthTexture,    // MSAA深度附件
 *                             nullptr,
 *                             "MSAA Framebuffer");
 * 
 * // 渲染后，resolveTexture将包含抗锯齿后的最终结果
 * ```
 * 
 * ## 常见问题和解决方案
 * 
 * ### 1. 验证层错误："framebuffer attachment X has different dimensions"
 * **原因**：不同附件的尺寸不一致
 * **解决**：确保所有附件（颜色、深度、模板）都有相同的宽度和高度
 * ```cpp
 * // 错误示例
 * auto colorTexture = createTexture(1920, 1080);  // 1920x1080
 * auto depthTexture = createTexture(1024, 768);   // 1024x768 - 尺寸不匹配！
 * 
 * // 正确示例
 * const uint32_t width = 1920, height = 1080;
 * auto colorTexture = createTexture(width, height);
 * auto depthTexture = createTexture(width, height);  // 相同尺寸
 * ```
 * 
 * ### 2. 验证层错误："framebuffer incompatible with renderpass"
 * **原因**：Framebuffer与RenderPass不兼容
 * **解决**：检查附件数量、格式、采样数是否匹配
 * ```cpp
 * // RenderPass期望的格式
 * std::vector<VkFormat> renderPassFormats = {
 *     VK_FORMAT_R8G8B8A8_UNORM,  // 颜色附件
 *     VK_FORMAT_D32_SFLOAT       // 深度附件
 * };
 * 
 * // Framebuffer提供的纹理格式必须匹配
 * auto colorTexture = createTexture(..., VK_FORMAT_R8G8B8A8_UNORM, ...);  // 匹配
 * auto depthTexture = createTexture(..., VK_FORMAT_D32_SFLOAT, ...);      // 匹配
 * ```
 * 
 * ### 3. 断言失败："Creating a framebuffer with no attachments is not supported"
 * **原因**：没有提供任何附件
 * **解决**：至少提供一个颜色附件或深度附件
 * ```cpp
 * // 错误示例
 * Framebuffer framebuffer(context, device, renderPass,
 *                         {},       // 空的颜色附件列表
 *                         nullptr,  // 无深度附件
 *                         nullptr); // 无模板附件 - 没有任何附件！
 * 
 * // 正确示例
 * Framebuffer framebuffer(context, device, renderPass,
 *                         {colorTexture},  // 至少一个颜色附件
 *                         nullptr,         // 深度附件可选
 *                         nullptr);
 * ```
 * 
 * ### 4. 性能问题
 * **原因**：不必要的大尺寸帧缓冲
 * **解决**：根据实际需求选择合适的分辨率
 * ```cpp
 * // 阴影贴图不需要很高的分辨率
 * auto shadowMapTexture = createTexture(1024, 1024);  // 而不是4K
 * 
 * // 后处理效果可以使用较低分辨率
 * auto blurTexture = createTexture(width/2, height/2);  // 半分辨率
 * ```
 * 
 * ## 性能优化建议
 * 
 * ### 1. 选择合适的格式
 * - **颜色附件**：使用最小满足需求的位深
 *   - UI渲染：VK_FORMAT_R8G8B8A8_UNORM
 *   - HDR渲染：VK_FORMAT_R16G16B16A16_SFLOAT
 * - **深度附件**：根据精度需求选择
 *   - 一般3D：VK_FORMAT_D32_SFLOAT
 *   - 移动设备：VK_FORMAT_D16_UNORM
 * 
 * ### 2. 合理使用MSAA
 * - 移动设备：使用较低的MSAA级别（2x或4x）
 * - 桌面设备：可以使用更高级别（8x或16x）
 * - 考虑使用FXAA等后处理抗锯齿替代
 * 
 * ### 3. 优化帧缓冲尺寸
 * - 根据用途选择合适的分辨率
 * - 阴影贴图：512x512 到 2048x2048
 * - 环境贴图：256x256 到 1024x1024
 * - 后处理缓冲：可以使用较低分辨率
 * 
 * ### 4. 重用帧缓冲
 * ```cpp
 * // 对于相同格式和尺寸的渲染，重用Framebuffer对象
 * class FramebufferPool {
 *     std::vector<std::unique_ptr<Framebuffer>> pool_;
 * public:
 *     Framebuffer* acquire(uint32_t width, uint32_t height, VkFormat format);
 *     void release(Framebuffer* fb);
 * };
 * ```
 */
