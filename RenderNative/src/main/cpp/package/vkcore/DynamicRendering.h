#pragma once
#include "Common.h"
#include "Utils.h"
#include "vk_mem_alloc.h"

namespace VkCore {

    /**
     * Dynamic Rendering（动态渲染）类
     * 
     * 什么是动态渲染？
     * 动态渲染是Vulkan 1.3引入的一个革命性特性，它简化了传统的渲染通道（RenderPass）使用方式。
     * 在传统Vulkan中，你需要预先创建VkRenderPass和VkFramebuffer对象，这增加了API的复杂性。
     * 动态渲染允许你在录制命令时直接指定渲染目标，无需预创建这些对象。
     * 
     * 传统渲染 vs 动态渲染的对比：
     * 
     * 传统渲染流程：
     * 1. 创建VkRenderPass（定义附件格式和操作）
     * 2. 创建VkFramebuffer（绑定具体的图像视图）
     * 3. 调用vkCmdBeginRenderPass开始渲染
     * 4. 录制绘制命令
     * 5. 调用vkCmdEndRenderPass结束渲染
     * 
     * 动态渲染流程：
     * 1. 直接调用vkCmdBeginRendering，指定附件信息
     * 2. 录制绘制命令
     * 3. 调用vkCmdEndRendering结束渲染
     * 
     * 动态渲染的优势：
     * - 简化API使用：减少对象创建和管理
     * - 提高灵活性：运行时决定渲染配置
     * - 减少驱动开销：避免预编译渲染通道的复杂性
     * - 更直观：类似现代图形API（如DirectX 12）的设计
     * 
     * 使用场景：
     * - 现代游戏引擎：简化渲染管线管理
     * - 动态效果：实时改变渲染目标配置
     * - 移动设备：减少内存占用和初始化时间
     * - 原型开发：快速迭代渲染效果
     * 
     * 注意事项：
     * - 需要Vulkan 1.3或VK_KHR_dynamic_rendering扩展
     * - 与传统渲染通道不兼容，需要选择其中一种方式
     * - 某些优化（如tile-based渲染）可能需要额外配置
     */
    class DynamicRendering final {
    public:
        /**
         * 附件描述结构体
         * 
         * 什么是附件（Attachment）？
         * 在图形渲染中，附件是指渲染目标，也就是渲染结果要写入的图像。
         * 想象你在画画：画布就是附件，颜料就是渲染的像素数据。
         * 
         * 常见的附件类型：
         * 1. 颜色附件（Color Attachment）：存储最终的像素颜色
         * 2. 深度附件（Depth Attachment）：存储像素的深度信息，用于遮挡判断
         * 3. 模板附件（Stencil Attachment）：存储模板值，用于复杂的遮罩操作
         * 
         * 多目标渲染（MRT - Multiple Render Targets）：
         * 现代GPU支持同时向多個颜色附件写入数据，用于：
         * - 延迟渲染：分别写入颜色、法线、材质参数等
         * - HDR渲染：分别写入不同亮度范围的数据
         * - 后处理效果：生成多种中间结果
         */
        struct AttachmentDescription {
            VkImageView imageView;              ///< 图像视图：定义如何访问和解释图像数据
            VkImageLayout imageLayout;          ///< 图像布局：告诉GPU如何在内存中组织图像数据以获得最佳性能
            
            // 多采样解析相关字段（用于抗锯齿）
            VkResolveModeFlagBits resolveModeFlagBits = VK_RESOLVE_MODE_NONE;  ///< 解析模式：如何将多采样数据合并为单采样
            VkImageView resolveImageView = VK_NULL_HANDLE;                    ///< 解析目标图像视图：多采样数据的最终输出位置
            VkImageLayout resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;     ///< 解析目标图像布局
            
            // 附件操作定义
            VkAttachmentLoadOp attachmentLoadOp;   ///< 加载操作：渲染开始时如何处理附件中的现有数据
            VkAttachmentStoreOp attachmentStoreOp; ///< 存储操作：渲染结束时如何处理附件中的数据
            VkClearValue clearValue;               ///< 清除值：当加载操作为CLEAR时使用的值
        };

        /**
         * 获取所需的Vulkan实例扩展
         * 
         * 什么是Vulkan扩展？
         * Vulkan的扩展系统允许驱动程序提供额外的功能，这些功能不在核心Vulkan规范中。
         * 动态渲染需要特定的扩展支持才能正常工作。
         * 
         * @return 返回所需扩展的名称字符串
         */
        static std::string instanceExtensions();

        /**
         * 开始动态渲柕命令
         * 
         * 这个函数是动态渲柕的核心，它替代了传统的vkCmdBeginRenderPass。
         * 它会配置渲柕目标（附件）并开始渲柕过程。
         * 
         * 主要功能：
         * 1. 设置颜色、深度、模板附件
         * 2. 配置附件的加载和存储操作
         * 3. 处理图像布局变换（如果需要）
         * 4. 调用vkCmdBeginRendering开始渲柕
         * 
         * @param commandBuffer 命令缓冲区：用于录制GPU命令的对象
         * @param image 主图像：需要处理布局变换的图像（通常是交换链图像）
         * @param renderingFlags 渲染标志：控制渲柕行为的特殊设置
         * @param rectRenderSize 渲柕区域：定义渲柕的像素范围（类似视口）
         * @param layerCount 层数：用于数组纹理或立体纹理的渲柕
         * @param viewMask 视图掩码：用于VR/AR多视图渲柕
         * @param colorAttachmentDescList 颜色附件列表：可以有多个颜色输出（MRT）
         * @param depthAttachmentDescList 深度附件描述：可选，用于深度测试
         * @param stencilAttachmentDescList 模板附件描述：可选，用于模板测试
         * @param oldLayout 原图像布局：渲柕开始前图像的布局状态
         * @param newLayout 新图像布局：渲柕开始后图像的布局状态
         */
        static void beginRenderingCmd(
                VkCommandBuffer commandBuffer, VkImage image, VkRenderingFlags renderingFlags,
                VkRect2D rectRenderSize, uint32_t layerCount, uint32_t viewMask,
                std::vector<AttachmentDescription> colorAttachmentDescList,
                const AttachmentDescription* depthAttachmentDescList,
                const AttachmentDescription* stencilAttachmentDescList,
                VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                
        /**
         * 结束动态渲柕命令
         * 
         * 这个函数结束渲柕过程并处理后续的图像布局变换。
         * 它替代了传统的vkCmdEndRenderPass。
         * 
         * 主要功能：
         * 1. 调用vkCmdEndRendering结束渲柕
         * 2. 处理图像布局变换（如果需要）
         * 3. 确保数据正确写入附件并可供后续使用
         * 
         * 常见的使用场景：
         * - 渲柕到交换链：最后需要转换为PRESENT_SRC布局以便显示
         * - 渲柕到纹理：转换为SHADER_READ_ONLY以便在着色器中使用
         * 
         * @param commandBuffer 命令缓冲区：与beginRenderingCmd中使用的同一个
         * @param image 主图像：需要处理布局变换的图像
         * @param oldLayout 原图像布局：渲柕结束时图像的当前布局
         * @param newLayout 新图像布局：渲柕结束后图像需要的布局
         */
        static void endRenderingCmd(
                VkCommandBuffer commandBuffer, VkImage image,
                VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VkImageLayout newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    };

}  // namespace VkCore