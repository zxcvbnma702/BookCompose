//
// Created by nio on 2025/10/8.
//

#include "DynamicRendering.h"

namespace VkCore {

    /**
     * 获取动态渲柕所需的Vulkan实例扩展
     * 
     * 什么是VK_KHR_get_physical_device_properties2？
     * 这个扩展允许获取物理设备的扩展属性和特性信息。
     * 动态渲柕需要检查硬件是否支持相关特性，因此需要这个扩展。
     * 
     * 扩展的作用：
     * 1. 检查GPU是否支持动态渲柕特性
     * 2. 获取动态渲柕的具体限制和能力
     * 3. 确保应用程序可以在不同的硬件上正常工作
     * 
     * @return 返回所需扩展的名称字符串
     */
    std::string DynamicRendering::instanceExtensions() {
        return "VK_KHR_get_physical_device_properties2";
    }

    /**
     * 开始动态渲柕命令的实现
     * 
     * 这个函数是动态渲柕的核心实现，它执行以下主要步骤：
     * 
     * 步骤1: 处理颜色附件
     * - 将用户提供的AttachmentDescription转换为Vulkan原生VkRenderingAttachmentInfo
     * - 支持多目标渲柕（MRT），可以同时输出到多个颜色附件
     * 
     * 步骤2: 处理深度附件（可选）
     * - 用于深度测试，判断像素的可见性
     * - 如果不提供，则没有深度测试
     * 
     * 步骤3: 处理模板附件（可选）
     * - 用于复杂的遮罩和轮廓操作
     * - 如果不提供，则没有模板测试
     * 
     * 步骤4: 构建VkRenderingInfo结构体
     * - 整合所有附件信息和渲柕参数
     * 
     * 步骤5: 处理图像布局变换（如果需要）
     * - 使用管线屏障确保图像在渲柕前处于正确的布局状态
     * 
     * 步骤6: 开始渲柕
     * - 调用vkCmdBeginRendering正式开始渲柕过程
     */
    void DynamicRendering::beginRenderingCmd(
            VkCommandBuffer commandBuffer, VkImage image, VkRenderingFlags renderingFlags,
            VkRect2D rectRenderSize, uint32_t layerCount, uint32_t viewMask,
            std::vector<AttachmentDescription> colorAttachmentDescList,
            const AttachmentDescription* depthAttachmentDescList,
            const AttachmentDescription* stencilAttachmentDescList, VkImageLayout oldLayout,
            VkImageLayout newLayout) {
            
        // ========================================
        // 步骤1: 处理颜色附件
        // ========================================
        
        /**
         * 创建颜色附件信息列表
         * 
         * 为什么需要转换？
         * AttachmentDescription是我们自定义的简化结构体，而VkRenderingAttachmentInfo
         * 是Vulkan原生结构体。需要将前者的数据复制到后者中。
         * 
         * 多目标渲柕（MRT）的优势：
         * - 延迟渲柕：分别输出颜色、法线、位置等数据
         * - HDR渲柕：同时输出高亮度和低亮度版本
         * - 后处理效果：生成多种中间结果供后续处理
         */
        std::vector<VkRenderingAttachmentInfo> colorRenderingAttachmentInfoList;

        // 遍历所有颜色附件描述，将其转换为Vulkan原生格式
        for (auto& renderingAttachmentInfoParam : colorAttachmentDescList) {
            /**
             * VkRenderingAttachmentInfo 结构体详解
             * 
             * 这个结构体定义了动态渲柕中的一个附件。
             * 它包含了所有必要的信息来告诉GPU如何处理这个附件。
             * 
             * 字段说明：
             * - sType: Vulkan结构体类型标识符
             * - pNext: 扩展结构体指针（通常为null）
             * - imageView: 附件对应的图像视图
             * - imageLayout: 图像在渲柕过程中的布局
             * - resolveMode: 多采样解析模式（用于抗锯齿）
             * - resolveImageView: 多采样解析目标图像视图
             * - resolveImageLayout: 解析目标图像布局
             * - loadOp: 加载操作（LOAD/CLEAR/DONT_CARE）
             * - storeOp: 存储操作（STORE/DONT_CARE）
             * - clearValue: 清除值（当loadOp=CLEAR时使用）
             */
            VkRenderingAttachmentInfo renderingAttachmentInfo = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,         // Vulkan结构体类型
                    .pNext = nullptr,                                            // 无扩展结构体
                    .imageView = renderingAttachmentInfoParam.imageView,         // 目标图像视图
                    .imageLayout = renderingAttachmentInfoParam.imageLayout,     // 图像布局
                    .resolveMode = renderingAttachmentInfoParam.resolveModeFlagBits,        // 多采样解析模式
                    .resolveImageView = renderingAttachmentInfoParam.resolveImageView,      // 解析目标图像视图
                    .resolveImageLayout = renderingAttachmentInfoParam.resolveImageLayout,  // 解析目标布局
                    .loadOp = renderingAttachmentInfoParam.attachmentLoadOp,     // 加载操作
                    .storeOp = renderingAttachmentInfoParam.attachmentStoreOp,   // 存储操作
                    .clearValue = renderingAttachmentInfoParam.clearValue        // 清除值
            };

            // 将配置好的附件信息添加到列表中
            colorRenderingAttachmentInfoList.push_back(renderingAttachmentInfo);
        }

        // ========================================
        // 步骤2: 处理深度附件（可选）
        // ========================================
        
        /**
         * 深度附件处理
         * 
         * 什么是深度附件？
         * 深度附件存储每个像素的深度值（距离相机的远近）。
         * GPU使用这个信息来判断哪个物体在前面，哪个被遭挡。
         * 
         * 深度测试的作用：
         * 1. 正确显示物体之间的遭挡关系
         * 2. 避免后面的物体遮挡前面的物体
         * 3. 提高渲柕性能（早期被遭挡的像素不需要跑片段着色器）
         * 
         * 注意：深度附件是可选的。如果不需要深度测试（如2D UI渲柕），
         * 可以传递nullptr。
         */
        VkRenderingAttachmentInfo depthRenderingAttachmentInfo;      // 深度附件信息结构体
        VkRenderingAttachmentInfo* depthRenderingAttachmentInfoPtr = NULL;  // 指向深度附件信息的指针
        
        // 检查是否提供了深度附件描述
        if (depthAttachmentDescList) {
            // 配置深度附件信息（和颜色附件类似）
            depthRenderingAttachmentInfo = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,           // Vulkan结构体类型
                    .pNext = NULL,                                                 // 无扩展结构体
                    .imageView = depthAttachmentDescList->imageView,               // 深度图像视图
                    .imageLayout = depthAttachmentDescList->imageLayout,           // 深度图像布局
                    .resolveMode = depthAttachmentDescList->resolveModeFlagBits,   // 多采样解析模式
                    .resolveImageView = depthAttachmentDescList->resolveImageView, // 解析目标图像视图
                    .resolveImageLayout = depthAttachmentDescList->resolveImageLayout, // 解析目标布局
                    .loadOp = depthAttachmentDescList->attachmentLoadOp,           // 加载操作
                    .storeOp = depthAttachmentDescList->attachmentStoreOp,         // 存储操作
                    .clearValue = depthAttachmentDescList->clearValue              // 清除值（通常是1.0）
            };

            // 设置指针指向深度附件信息
            depthRenderingAttachmentInfoPtr = &depthRenderingAttachmentInfo;
        }
        // 如果没有提供深度附件，指针保持NULL，表示不使用深度测试

        // ========================================
        // 步骤3: 处理模板附件（可选）
        // ========================================
        
        /**
         * 模板附件处理
         * 
         * 什么是模板附件？
         * 模板附件存储每个像素的模板值（通常是8位整数）。
         * 模板测试允许执行复杂的像素级别操作，如遮罩、轮廓渲柕等。
         * 
         * 模板测试的应用场景：
         * 1. 阴影体渲柕：标记阴影区域，然后只在这些区域绘制阴影
         * 2. 反射效果：标记反射表面，然后只在这些表面绘制反射
         * 3. 轮廓渲柕：先绘制轮廓，再填充内部
         * 4. UI遮罩：复杂形状的UI组件裁剪
         * 5. 体积渲柕：标记体积边界，控制渲柕顺序
         * 
         * 注意：模板附件也是可选的。如果不需要模板测试，
         * 可以传递nullptr。
         */
        VkRenderingAttachmentInfo stencilRenderingAttachmentInfo;        // 模板附件信息结构体
        VkRenderingAttachmentInfo* stencilRenderingAttachmentInfoPtr = nullptr;  // 指向模板附件信息的指针
        
        // 检查是否提供了模板附件描述
        if (stencilAttachmentDescList) {
            // 配置模板附件信息（和颜色、深度附件类似）
            stencilRenderingAttachmentInfo = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,             // Vulkan结构体类型
                    .pNext = nullptr,                                                // 无扩展结构体
                    .imageView = stencilAttachmentDescList->imageView,               // 模板图像视图
                    .imageLayout = stencilAttachmentDescList->imageLayout,           // 模板图像布局
                    .resolveMode = stencilAttachmentDescList->resolveModeFlagBits,   // 多采样解析模式
                    .resolveImageView = stencilAttachmentDescList->resolveImageView, // 解析目标图像视图
                    .resolveImageLayout = stencilAttachmentDescList->resolveImageLayout, // 解析目标布局
                    .loadOp = stencilAttachmentDescList->attachmentLoadOp,           // 加载操作
                    .storeOp = stencilAttachmentDescList->attachmentStoreOp,         // 存储操作
                    .clearValue = stencilAttachmentDescList->clearValue              // 清除值（通常是0）
            };

            // 设置指针指向模板附件信息
            stencilRenderingAttachmentInfoPtr = &stencilRenderingAttachmentInfo;
        }
        // 如果没有提供模板附件，指针保持nullptr，表示不使用模板测试

        // ========================================
        // 步骤4: 构建VkRenderingInfo结构体
        // ========================================
        
        /**
         * VkRenderingInfo 结构体详解
         * 
         * 这是动态渲柕的核心结构体，它整合了所有渲柕参数和附件信息。
         * 它相当于传统渲柕通道中的VkRenderPass + VkFramebuffer的组合。
         * 
         * 主要字段说明：
         * - flags: 渲柕标志，用于控制渲柕行为的特殊设置
         * - renderArea: 渲柕区域，定义渲柕的像素范围
         * - layerCount: 层数，用于数组纹理或立体纹理的渲柕
         * - viewMask: 视图掩码，用于VR/AR多视图渲柕
         * - colorAttachmentCount/pColorAttachments: 颜色附件数量和数组
         * - pDepthAttachment: 深度附件，可为null
         * - pStencilAttachment: 模板附件，又可为null
         * 
         * 渲柕区域 vs 视口的区别：
         * - 渲柕区域：定义哪个像素区域会被渲柕命令影响
         * - 视口：定义坐标系变换，将3D坐标映射到屏幕坐标
         * 
         * 多视图渲柕（VR/AR）：
         * - viewMask用于指定哪些视图层需要渲柕
         * - 一次渲柕调用可以同时渲柕多个视图（如左右眼）
         * - 提高VR渲柕性能，减少CPU开销
         */
        VkRenderingInfo renderingInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,                              // Vulkan结构体类型
                .pNext = NULL,                                                         // 无扩展结构体
                .flags = renderingFlags,                                               // 渲柕标志
                .renderArea = rectRenderSize,                                          // 渲柕区域（像素范围）
                .layerCount = layerCount,                                              // 层数（单层渲柕通常为1）
                .viewMask = viewMask,                                                  // 视图掩码（多视图渲柕用）
                .colorAttachmentCount = (uint32_t)colorRenderingAttachmentInfoList.size(),  // 颜色附件数量
                .pColorAttachments = colorRenderingAttachmentInfoList.data(),          // 颜色附件数组指针
                .pDepthAttachment = depthRenderingAttachmentInfoPtr,                   // 深度附件指针（可为null）
                .pStencilAttachment = stencilRenderingAttachmentInfoPtr                // 模板附件指针（可为null）
        };

        // ========================================
        // 步骤5: 处理图像布局变换（如果需要）
        // ========================================
        
        /**
         * 图像布局变换和管线屏障
         * 
         * 什么是图像布局？
         * 图像布局描述了图像在GPU内存中的组织方式。
         * 不同的操作（着色器采样、渲柕输出、显示）需要不同的布局。
         * 
         * 常见的布局类型：
         * - VK_IMAGE_LAYOUT_UNDEFINED: 未定义，初始状态
         * - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: 优化用于颜色附件输出
         * - VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: 优化用于屏幕显示
         * - VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: 优化用于着色器采样
         * 
         * 什么是管线屏障？
         * 管线屏障是一种同步原语，用于确保内存访问和执行顺序的正确性。
         * 在GPU伪并行执行中，需要显式地告诉硬件依赖关系。
         * 
         * 这里的管线屏障作用：
         * 1. 确保图像在渲柕开始前就绦完成了布局变换
         * 2. 保证数据一致性，避免童束问题
         * 3. 告诉GPU什么时候可以安全地访问图像
         */
        // 检查是否需要图像布局变换
        if (oldLayout != newLayout) {
            /**
             * VkImageMemoryBarrier 结构体详解
             * 
             * 这个结构体定义了一个图像内存屏障，用于控制图像的访问和布局变换。
             * 
             * 字段说明：
             * - srcAccessMask: 源访问掩码，定义屏障前的访问类型
             * - dstAccessMask: 目标访问掩码，定义屏障后的访问类型
             * - oldLayout: 原图像布局
             * - newLayout: 新图像布局
             * - image: 要变换的图像对象
             * - subresourceRange: 子资源范围（哪些像素层和 mip 级别）
             */
            const VkImageMemoryBarrier imageMemoryBarrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,           // Vulkan结构体类型
                    .srcAccessMask = 0,                                        // 源访问掩码（自动推导）
                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // 目标访问：颜色附件写入
                    .oldLayout = oldLayout,                                    // 原布局状态
                    .newLayout = newLayout,                                    // 新布局状态
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,           // 源队列族（忽略）
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,           // 目标队列族（忽略）
                    .image = image,                                            // 目标图像
                    .subresourceRange = {                                      // 子资源范围
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,           // 颜色方面（非深度/模板）
                            .baseMipLevel = 0,                                 // 起始 mip 级别
                            .levelCount = 1,                                   // mip 级别数量
                            .baseArrayLayer = 0,                               // 起始数组层
                            .layerCount = 1,                                   // 数组层数量
                    }
            };

            /**
             * 插入管线屏障命令
             * 
             * vkCmdPipelineBarrier 是管线屏障的核心函数，它控制命令的执行顺序。
             * 这里的屏障确保图像在渲柕开始前就已经完成了布局变换。
             * 
             * 参数说明：
             * - commandBuffer: 命令缓冲区
             * - srcStageMask: 源管线阶段（TOP_OF_PIPE表示管线的最开始）
             * - dstStageMask: 目标管线阶段（COLOR_ATTACHMENT_OUTPUT表示颜色输出阶段）
             * - dependencyFlags: 依赖标志（0表示默认行为）
             * - 其他参数: 内存、缓冲区、图像屏障的数量和指针
             */
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,              // srcStageMask: 管线开始
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask: 颜色输出阶段
                                 0,                                              // dependencyFlags: 默认依赖
                                 0, nullptr,                                     // 内存屏障数量和数组
                                 0, nullptr,                                     // 缓冲区屏障数量和数组
                                 1, &imageMemoryBarrier);                        // 图像屏障数量和数组
        }
        // 如果不需要布局变换，直接跳过屏障步骤

        // ========================================
        // 步骤6: 开始动态渲柕
        // ========================================
        
        /**
         * 正式开始动态渲柕
         * 
         * vkCmdBeginRendering 是动态渲柕的核心函数，它替代了传统的vkCmdBeginRenderPass。
         * 这个调用标志着渲柕过程的正式开始。
         * 
         * 执行后的状态变化：
         * 1. 所有附件被清除或加载（根据loadOp设置）
         * 2. GPU开始接受后续的绘制命令
         * 3. 渲柕状态机进入“渲柕中”状态
         * 4. 所有后续的绘制命令都会影响指定的附件
         * 
         * 注意事项：
         * - 必须与 vkCmdEndRendering 配对使用
         * - 在调用这个函数后，才能调用绘制命令（vkCmdDraw*）
         * - 不允许嵌套调用（不能在渲柕过程中再次开始渲柕）
         */
        vkCmdBeginRendering(commandBuffer, &renderingInfo);
        
        // 此时动态渲柕已经开始，可以执行后续的绘制命令了
    }  // beginRenderingCmd 函数结束

    /**
     * 结束动态渲柕命令的实现
     * 
     * 这个函数结束动态渲柕过程并处理后续的图像布局变换。
     * 它执行以下主要步骤：
     * 
     * 步骤1: 结束渲柕
     * - 调用vkCmdEndRendering正式结束渲柕过程
     * - 确保所有绘制命令都已经完成
     * - 渲柕状态机退出“渲柕中”状态
     * 
     * 步骤2: 处理图像布局变换（如果需要）
     * - 使用管线屏障确保图像在后续使用前处于正确的布局
     * - 常见的变换：COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR（用于显示）
     * 
     * 这个函数的重要性：
     * 1. 确保渲柕结果正确写入附件
     * 2. 为后续操作（如显示、纹理采样）准备图像
     * 3.维持GPU内存一致性和性能优化
     */
    void DynamicRendering::endRenderingCmd(VkCommandBuffer commandBuffer, VkImage image,
                                           VkImageLayout oldLayout, VkImageLayout newLayout) {
                                           
        // ========================================
        // 步骤1: 结束动态渲柕
        // ========================================
        
        /**
         * 正式结束动态渲柕
         * 
         * vkCmdEndRendering 标志着渲柕过程的正式结束。
         * 这个调用会触发以下操作：
         * 
         * 1. 刷新渲柕缓存：确保所有挂起的绘制命令都完成
         * 2. 执行附件存储操作：根据storeOp设置处理附件数据
         * 3. 多采样解析：如果启用了MSAA，将多采样数据合并为单采样
         * 4. 释放渲柕资源：允许GPU释放中间缓冲区和优化结构
         * 
         * 注意：必须与之前的vkCmdBeginRendering配对使用。
         */
        vkCmdEndRendering(commandBuffer);

        // ========================================
        // 步骤2: 处理图像布局变换（如果需要）
        // ========================================
        
        /**
         * 渲柕结束后的图像布局变换
         * 
         * 为什么需要结束后的布局变换？
         * 渲柕结束后，图像通常需要用于不同的目的：
         * - 显示到屏幕：需要PRESENT_SRC_KHR布局
         * - 作为纹理使用：需要SHADER_READ_ONLY_OPTIMAL布局
         * - 作为下一次渲柕的输入：可能需要其他特定布局
         * 
         * 这个屏障和开始时的屏障的区别：
         * - 开始时：从任意布局 → COLOR_ATTACHMENT_OPTIMAL（为渲柕做准备）
         * - 结束时：从 COLOR_ATTACHMENT_OPTIMAL → 目标布局（为后续使用做准备）
         * 
         * 管线阶段的选择：
         * - srcStageMask: COLOR_ATTACHMENT_OUTPUT（渲柕输出完成）
         * - dstStageMask: BOTTOM_OF_PIPE（管线最末端，确保所有操作都完成）
         */
        // 检查是否需要图像布局变换
        if (oldLayout != newLayout) {
            /**
             * 渲柕结束后的图像内存屏障
             * 
             * 这个屏障确保：
             * 1. 所有渲柕操作都已经完成并写入内存
             * 2. 图像布局变换在后续使用前完成
             * 3. 内存可见性和一致性得到保证
             */
            const VkImageMemoryBarrier image_memory_barrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,           // Vulkan结构体类型
                    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // 源访问：颜色附件写入
                    .dstAccessMask = 0,                                        // 目标访问（自动推导）
                    .oldLayout = oldLayout,                                    // 原布局（通常是COLOR_ATTACHMENT_OPTIMAL）
                    .newLayout = newLayout,                                    // 新布局（通常是PRESENT_SRC_KHR）
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,           // 源队列族（忽略）
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,           // 目标队列族（忽略）
                    .image = image,                                            // 目标图像
                    .subresourceRange = {                                      // 子资源范围
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,           // 颜色方面
                            .baseMipLevel = 0,                                 // 起始 mip 级别
                            .levelCount = 1,                                   // mip 级别数量
                            .baseArrayLayer = 0,                               // 起始数组层
                            .layerCount = 1,                                   // 数组层数量
                    }
            };

            /**
             * 插入渲柕结束后的管线屏障
             * 
             * 这个屏障的作用：
             * 1. 等待所有颜色附件输出操作完成
             * 2. 确保图像布局变换在后续使用前完成
             * 3. 保证数据在不同的GPU单元之间正确同步
             * 
             * 管线阶段选择说明：
             * - COLOR_ATTACHMENT_OUTPUT: 颜色附件输出阶段，渲柕的最后一步
             * - BOTTOM_OF_PIPE: 管线最末端，确保所有先前的操作都完成
             */
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask: 颜色输出阶段
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // dstStageMask: 管线末端
                                 0,                                              // dependencyFlags: 默认依赖
                                 0, nullptr,                                     // 内存屏障数量和数组
                                 0, nullptr,                                     // 缓冲区屏障数量和数组
                                 1,                                              // 图像屏障数量
                                 &image_memory_barrier                           // 图像屏障数组指针
            );
        }
        // 如果不需要布局变换，直接跳过屏障步骤
        
        // 动态渲柕完全结束，图像已经准备好供后续使用
    }  // endRenderingCmd 函数结束

}  // namespace VkCore

/*
 * ========================================
 * 动态渲柕（Dynamic Rendering）系统的总结和学习指南
 * ========================================
 * 
 * 一、动态渲柕的核心优势
 * 
 * 1. 简化API使用：
 *    - 传统方式：需要创建VkRenderPass + VkFramebuffer + 管理生命周期
 *    - 动态方式：直接指定附件信息，无需预创建对象
 * 
 * 2. 提高灵活性：
 *    - 运行时决定渲柕配置
 *    - 支持动态改变附件数量和格式
 *    - 更容易实现复杂的渲柕效果
 * 
 * 3. 减少驱动开销：
 *    - 避免预编译渲柕通道的复杂性
 *    - 减少内存分配和管理开销
 *    - 提高缓存命中率
 * 
 * 二、动态渲柕的工作流程
 * 
 * 1. 初始化阶段：
 *    - 检查GPU是否支持动态渲柕特性
 *    - 启用所需的Vulkan扩展
 *    - 创建必要的图像和图像视图
 * 
 * 2. 渲柕阶段：
 *    - 调用beginRenderingCmd开始渲柕
 *    - 执行绘制命令（vkCmdDraw*系列）
 *    - 调用endRenderingCmd结束渲柕
 * 
 * 3. 清理阶段：
 *    - 等待所有操作完成
 *    - 释放图像和其他资源
 * 
 * 三、附件类型和使用场景
 * 
 * 1. 颜色附件 (Color Attachment)：
 *    - 单目标：基本的3D渲柕、UI渲柕
 *    - 多目标 (MRT)：延迟渲柕、HDR渲柕、后处理效果
 * 
 * 2. 深度附件 (Depth Attachment)：
 *    - 3D渲柕：处理物体之间的遭挡关系
 *    - 阴影映射：生成阴影贴图
 *    - 深度预通道：提前拒绝被遭挡的像素以提高性能
 * 
 * 3. 模板附件 (Stencil Attachment)：
 *    - 轮廓渲柕：复杂形状的精确控制
 *    - 阴影体：标记阴影区域
 *    - 反射效果：标记反射表面
 *    - UI遮罩：复杂形状的裁剪
 * 
 * 四、图像布局和管线屏障
 * 
 * 1. 布局变换的必要性：
 *    - GPU为不同操作优化内存组织方式
 *    - 渲柕、采样、显示需要不同的布局
 *    - 正确的布局可以显著提高性能
 * 
 * 2. 管线屏障的作用：
 *    - 确保内存访问的正确顺序
 *    - 保证数据一致性和可见性
 *    - 管理GPU不同单元之间的依赖关系
 * 
 * 3. 常见的布局转换模式：
 *    - 渲柕到显示：UNDEFINED → COLOR_ATTACHMENT → PRESENT_SRC
 *    - 渲柕到纹理：UNDEFINED → COLOR_ATTACHMENT → SHADER_READ_ONLY
 *    - 纹理采样：SHADER_READ_ONLY → COLOR_ATTACHMENT → SHADER_READ_ONLY
 * 
 * 五、性能优化建议
 * 
 * 1. 附件管理：
 *    - 合理使用loadOp和storeOp，避免不必要的内存操作
 *    - 优先使用DONT_CARE来减少带宽消耗
 *    - 根据需要选择适当的附件格式
 * 
 * 2. 布局优化：
 *    - 减少不必要的布局变换
 *    - 批量处理布局变换以减少屏障数量
 *    - 使用最优的布局以获得最佳性能
 * 
 * 3. 渲柕传输：
 *    - 将相似的渲柕操作组合在一起
 *    - 减少渲柕通道切换的次数
 *    - 使用多目标渲柕减少渲柕遍数
 * 
 * 六、错误排查和调试
 * 
 * 1. 常见错误：
 *    - 忘记调用endRenderingCmd
 *    - 附件格式与图像格式不匹配
 *    - 布局变换不正确或缺失
 *    - 在渲柕过程中调用不兼容的命令
 * 
 * 2. 调试技巧：
 *    - 使用Vulkan验证层捕获API错误
 *    - 通过RenderDoc等工具分析渲柕过程
 *    - 检查GPU功能支持和限制
 *    - 监控内存使用和性能指标
 * 
 * 3. 最佳实践：
 *    - 总是检查返回值和错误状态
 *    - 使用有意义的调试名称
 *    - 遵循DCGPU和Mobile GPU的最佳实践
 *    - 保持代码的可读性和可维护性
 * 
 * 这个DynamicRendering类为现代Vulkan应用提供了一个高效、灵活、
 * 易用的动态渲柕解决方案，特别适合现代游戏引擎和
 * 高性能图形应用的需求。
 */