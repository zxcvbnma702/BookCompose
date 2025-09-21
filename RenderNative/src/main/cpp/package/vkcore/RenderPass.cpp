/**
 * @file RenderPass.cpp
 * @brief Vulkan渲染通道实现 - 初学者详细指南
 * @author nio
 * @date 2025/7/27
 * 
 * ## 文件概述
 * 
 * 本文件实现了RenderPass类的所有功能，包括三种不同的构造方式：
 * 1. **基于纹理对象的构造**：最简单直观的方式
 * 2. **基于格式的标准构造**：最灵活的方式，支持多视图
 * 3. **支持片段密度图的构造**：高级性能优化方式
 * 
 * ## 核心实现逻辑
 * 
 * 每个构造函数都遵循相似的模式：
 * 1. **参数验证**：确保输入参数的一致性
 * 2. **附件描述创建**：定义每个附件的属性
 * 3. **附件引用创建**：在子通道中引用附件
 * 4. **子通道描述创建**：定义渲染操作
 * 5. **依赖关系创建**：确保正确的同步
 * 6. **渲染通道创建**：调用Vulkan API
 * 7. **调试命名**：设置GPU调试工具的名称
 * 
 * ## 关键Vulkan概念
 * 
 * ### 附件（Attachments）
 * 附件是渲染过程中使用的图像，分为几类：
 * - **颜色附件**：存储渲染的颜色结果
 * - **深度附件**：存储深度信息（Z-buffer）
 * - **模板附件**：存储模板测试信息
 * - **解析附件**：MSAA的最终输出
 * 
 * ### 子通道（Subpasses）
 * 子通道定义一组渲染操作，可以：
 * - 读取输入附件
 * - 写入颜色附件
 * - 执行深度测试
 * - 解析多重采样
 * 
 * ### 依赖关系（Dependencies）
 * 确保正确的执行顺序和内存同步：
 * - **外部到子通道**：等待之前的操作完成
 * - **子通道到外部**：确保结果可被后续操作使用
 * - **子通道间依赖**：多子通道时的同步
 * 
 * @note 本实现针对初学者优化，包含大量解释性注释
 * @warning 某些依赖关系设置较为宽泛，生产环境应根据具体需求优化
 */

#include "RenderPass.h"

#include <optional>    // 可选值支持

#include "Context.h"   // Vulkan上下文管理
#include "Texture.h"   // 纹理管理

namespace VkCore {

    /**
     * @brief 基于纹理对象的构造函数实现
     * 
     * 这是最直观的构造函数，从已有的纹理对象中提取所有必要信息。
     * 特别适合离屏渲染和后处理效果的场景。
     * 
     * ## 实现步骤概述：
     * 1. 验证参数一致性（注释的断言）
     * 2. 创建主要附件的描述和引用
     * 3. 创建解析附件的描述和引用（如果有MSAA）
     * 4. 配置子通道描述
     * 5. 设置子通道依赖关系
     * 6. 创建VkRenderPass对象
     * 7. 设置调试名称
     */
    RenderPass::RenderPass(const Context& context,
                           const std::vector<std::shared_ptr<Texture>> attachments,
                           const std::vector<std::shared_ptr<Texture>> resolveAttachments,
                           const std::vector<VkAttachmentLoadOp>& loadOp,
                           const std::vector<VkAttachmentStoreOp>& storeOp,
                           const std::vector<VkImageLayout>& layout,
                           VkPipelineBindPoint bindPoint, const std::string& name)
            : device_{context.device()} {  // 保存设备句柄用于析构
        // === 步骤1：参数验证（注释的断言检查） ===
        // 这些断言确保所有数组的大小匹配，避免运行时错误
        // ASSERT(attachments.size() == loadOp.size() && attachments.size() == storeOp.size() &&
        //            attachments.size() == layout.size(),
        //        "The sizes of the attachments and their load and store operations and final "
        //        "layouts must match");
        // ASSERT(resolveAttachments.empty() || (attachments.size() == resolveAttachments.size()));

        // === 步骤2：准备数据结构 ===
        // 这些容器将存储Vulkan需要的所有附件信息
        std::vector<VkAttachmentDescription> attachmentDescriptors;    // 附件描述列表
        std::vector<VkAttachmentReference> colorAttachmentReferences;  // 颜色附件引用列表
        std::vector<VkAttachmentReference> resolveAttachmentReferences; // 解析附件引用列表
        std::optional<VkAttachmentReference> depthStencilAttachmentReference; // 深度/模板附件引用（可选）
        // === 步骤3：处理主要附件 ===
        // 遍历所有附件，为每个附件创建描述和引用
        for (uint32_t index = 0; index < attachments.size(); ++index) {
            // === 子步骤3.1：创建附件描述 ===
            // 从纹理对象中提取所有必要的属性
            attachmentDescriptors.emplace_back(VkAttachmentDescription{
                    .format = attachments[index]->vkFormat(),           // 像素格式（从纹理获取）
                    .samples = attachments[index]->VkSampleCount(),     // MSAA采样数（从纹理获取）
                    .loadOp = loadOp[index],                            // 加载操作（用户指定）
                    .storeOp = storeOp[index],                          // 存储操作（用户指定）
                    .stencilLoadOp = attachments[index]->isStencil()    // 模板加载操作
                                     ? loadOp[index]                    // 如果是模板纹理，使用相同操作
                                     : VK_ATTACHMENT_LOAD_OP_DONT_CARE, // 否则不关心
                    .stencilStoreOp = attachments[index]->isStencil()   // 模板存储操作
                                      ? storeOp[index]                  // 如果是模板纹理，使用相同操作
                                      : VK_ATTACHMENT_STORE_OP_DONT_CARE, // 否则不关心
                    .initialLayout = attachments[index]->vkLayout(),    // 初始布局（从纹理获取）
                    .finalLayout = layout[index],                       // 最终布局（用户指定）
            });

            // === 子步骤3.2：创建附件引用并分类 ===
            // 根据纹理类型决定是深度/模板附件还是颜色附件
            if (attachments[index]->isStencil() || attachments[index]->isDepth()) {
                // 深度或模板纹理 -> 深度/模板附件
                depthStencilAttachmentReference = VkAttachmentReference{
                        .attachment = index,                                        // 附件索引
                        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // 深度/模板最优布局
                };
            } else {
                // 普通纹理 -> 颜色附件
                colorAttachmentReferences.emplace_back(VkAttachmentReference{
                        .attachment = index,                                // 附件索引
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // 颜色附件最优布局
                });
            }
        }

        // === 步骤4：处理MSAA解析附件 ===
        // 解析附件用于将多重采样的结果转换为单采样图像
        const uint32_t numAttachments = attachmentDescriptors.size(); // 记录主要附件的数量
        for (uint32_t index = 0; index < resolveAttachments.size(); ++index) {
            // === 子步骤4.1：创建解析附件描述 ===
            attachmentDescriptors.emplace_back(VkAttachmentDescription{
                    .format = resolveAttachments[index]->vkFormat(),        // 格式（通常与对应的颜色附件相同）
                    .samples = VK_SAMPLE_COUNT_1_BIT,                       // 解析附件总是单采样
                    .loadOp = loadOp[index + numAttachments],               // 加载操作（通常是DONT_CARE）
                    .storeOp = storeOp[index + numAttachments],             // 存储操作（通常是STORE）
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,       // 解析附件不处理模板
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,     // 解析附件不处理模板
                    .initialLayout = resolveAttachments[index]->vkLayout(), // 初始布局
                    .finalLayout = layout[index + numAttachments],          // 最终布局
            });

            // === 子步骤4.2：创建解析附件引用 ===
            resolveAttachmentReferences.emplace_back(VkAttachmentReference{
                    .attachment = static_cast<uint32_t>(attachmentDescriptors.size() - 1), // 刚添加的附件索引
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                    // 颜色附件最优布局
            });
        }

        // === 步骤5：创建子通道描述 ===
        // 子通道定义了一组渲染操作，这里创建单个子通道
        const VkSubpassDescription spd = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,              // 图形管线绑定点
                .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size()), // 颜色附件数量
                .pColorAttachments = colorAttachmentReferences.data(),             // 颜色附件数组指针
                .pResolveAttachments = resolveAttachmentReferences.data(),         // 解析附件数组指针
                .pDepthStencilAttachment = depthStencilAttachmentReference.has_value() // 深度/模板附件指针
                                           ? &depthStencilAttachmentReference.value()  // 如果有深度附件，使用它
                                           : nullptr,                                   // 否则为空
        };

        // === 步骤6：创建子通道依赖关系 ===
        // 依赖关系确保正确的执行顺序和内存同步，这里设置两个依赖：
        // 1. 外部 -> 子通道0：等待之前的操作完成
        // 2. 子通道0 -> 外部：确保渲染结果可被后续操作使用
        std::array<VkSubpassDependency, 2> dependencies{};
        
        // === 子步骤6.1：外部到子通道的依赖 ===
        // 这个依赖确保在开始渲染之前，所有必要的资源都准备就绪
        dependencies[0] = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,                      // 源：外部操作（渲染通道之前）
                .dstSubpass = 0,                                        // 目标：我们的子通道（索引0）
                .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,   // 源阶段：管线底部（所有操作完成）
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |      // 目标阶段：颜色输出
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |         // 早期片段测试
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |          // 后期片段测试
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,               // 片段着色器
                .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,             // 源访问：内存读取
                .dstAccessMask =                                        // 目标访问：多种访问类型
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |     // 颜色附件读写
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |                                    // 深度模板读取
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,        // 深度模板写入和着色器读取
        };

        // === 子步骤6.2：子通道到外部的依赖 ===
        // 这个依赖确保渲染结果在被后续操作使用之前已经完成
        dependencies[1] = {
                .srcSubpass = 0,                                        // 源：我们的子通道（索引0）
                .dstSubpass = VK_SUBPASS_EXTERNAL,                      // 目标：外部操作（渲染通道之后）
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |      // 源阶段：颜色输出
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |         // 早期片段测试
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,           // 后期片段测试
                .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,     // 目标阶段：所有命令（保守设置）
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |               // 源访问：颜色附件读取
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |               // 颜色附件写入
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |        // 深度模板读取
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,        // 深度模板写入
                .dstAccessMask =                                        // 目标访问：多种访问类型
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |     // 颜色附件读写
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |                                    // 深度模板读取
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,        // 深度模板写入和着色器读取
        };

        // === 步骤7：创建VkRenderPassCreateInfo结构体 ===
        // 这个结构体包含了创建渲染通道所需的所有信息
        const VkRenderPassCreateInfo rpci = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,                 // 结构体类型
                .attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size()), // 附件数量
                .pAttachments = attachmentDescriptors.data(),                       // 附件描述数组
                .subpassCount = 1,                                                  // 子通道数量（这里只有1个）
                .pSubpasses = &spd,                                                 // 子通道描述数组
                .dependencyCount = 2,                                               // 依赖关系数量
                .pDependencies = dependencies.data(),                               // 依赖关系数组
                // 注意：这里的依赖关系设置较为宽泛，生产环境中应根据具体需求优化
        };

        // === 步骤8：创建VkRenderPass对象 ===
        // 调用Vulkan API创建实际的渲染通道对象
        VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));
        
        // === 步骤9：设置调试名称 ===
        // 为GPU调试工具设置有意义的名称，便于调试和性能分析
        context.setVkObjectname(renderPass_, VK_OBJECT_TYPE_RENDER_PASS,
                                "Render pass: " + name);
    }  // 第一个构造函数结束

    /**
     * @brief 基于格式的标准构造函数实现 - 最灵活的创建方式
     * 
     * 这是最灵活的构造函数，允许完全自定义所有参数。
     * 特别适合需要精确控制渲染通道行为的场景，包括多视图渲染。
     * 
     * ## 实现步骤概述：
     * 1. 验证参数一致性（严格的断言检查）
     * 2. 创建附件描述，支持自定义深度和模板处理
     * 3. 创建附件引用，支持指定的深度/模板附件索引
     * 4. 配置子通道描述
     * 5. 设置子通道依赖关系
     * 6. 可选的多视图配置（VR/AR应用）
     * 7. 创建VkRenderPass对象
     * 8. 设置调试名称
     * 
     * ## 与第一个构造函数的主要区别：
     * - 不依赖纹理对象，完全基于格式参数
     * - 支持精确指定深度和模板附件索引
     * - 支持多视图渲染（VR/AR双眼渲染）
     * - 允许自定义模板操作
     * - 更严格的参数验证
     */
    RenderPass::RenderPass(const Context& context, const std::vector<VkFormat>& formats,
                           const std::vector<VkImageLayout>& initialLayouts,
                           const std::vector<VkImageLayout>& finalLayouts,
                           const std::vector<VkAttachmentLoadOp>& loadOp,
                           const std::vector<VkAttachmentStoreOp>& storeOp,
                           VkPipelineBindPoint bindPoint,
                           std::vector<uint32_t> resolveAttachmentsIndices,
                           uint32_t depthAttachmentIndex, uint32_t stencilAttachmentIndex,
                           VkAttachmentLoadOp stencilLoadOp,
                           VkAttachmentStoreOp stencilStoreOp, bool multiview,
                           const std::string& name)
            : device_{context.device()} {  // 保存设备句柄用于析构
        // === 步骤1：严格的参数验证 ===
        // 检查所有数组的大小是否一致，这对于正确创建渲染通道至关重要
        const bool sameSizes =
                formats.size() == initialLayouts.size() && formats.size() == finalLayouts.size() &&
                formats.size() == loadOp.size() && formats.size() == storeOp.size();
        ASSERT(sameSizes,
               "The sizes of the attachments and their load and store operations and final "
               "layouts must match");

        // === 步骤2：准备数据结构 ===
        // 这些容器将存储所有附件信息，与第一个构造函数类似但更加精确
        std::vector<VkAttachmentDescription> attachmentDescriptors;    // 附件描述列表
        std::vector<VkAttachmentReference> colorAttachmentReferences;  // 颜色附件引用列表
        std::optional<VkAttachmentReference> depthStencilAttachmentReference; // 深度/模板附件引用（可选）
        // === 步骤3：处理所有附件 ===
        // 遍历所有格式，为每个附件创建描述和引用
        for (uint32_t index = 0; index < formats.size(); ++index) {
            // === 子步骤3.1：创建附件描述 ===
            // 基于用户提供的格式和参数创建描述
            attachmentDescriptors.emplace_back(VkAttachmentDescription{
                    .format = formats[index],                           // 用户指定的像素格式
                    .samples = VK_SAMPLE_COUNT_1_BIT,                   // 默认单采样（可根据需要修改）
                    .loadOp = loadOp[index],                            // 用户指定的加载操作
                    .storeOp = storeOp[index],                          // 用户指定的存储操作
                    .stencilLoadOp = index == stencilAttachmentIndex    // 模板加载操作
                                     ? stencilLoadOp                    // 如果是模板附件，使用指定操作
                                     : VK_ATTACHMENT_LOAD_OP_DONT_CARE, // 否则不关心
                    .stencilStoreOp = index == stencilAttachmentIndex   // 模板存储操作
                                      ? stencilStoreOp                  // 如果是模板附件，使用指定操作
                                      : VK_ATTACHMENT_STORE_OP_DONT_CARE, // 否则不关心
                    .initialLayout = initialLayouts[index],            // 用户指定的初始布局
                    .finalLayout = finalLayouts[index],                // 用户指定的最终布局
            });
            
            // === 子步骤3.2：创建附件引用并分类 ===
            // 根据索引判断是深度/模板附件还是颜色附件
            if (index == depthAttachmentIndex || index == stencilAttachmentIndex) {
                // 指定的深度或模板附件
                depthStencilAttachmentReference = VkAttachmentReference{
                        .attachment = index,                                        // 附件索引
                        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // 深度/模板最优布局
                };
            } else {
                // 其他附件作为颜色附件
                colorAttachmentReferences.emplace_back(VkAttachmentReference{
                        .attachment = index,                                // 附件索引
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // 颜色附件最优布局
                });
            }
        }

        // === 步骤4：创建子通道描述 ===
        // 配置单个子通道，与第一个构造函数相同的结构
        const VkSubpassDescription spd = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,              // 图形管线绑定点
                .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size()), // 颜色附件数量
                .pColorAttachments = colorAttachmentReferences.data(),             // 颜色附件数组指针
                .pDepthStencilAttachment = depthStencilAttachmentReference.has_value() // 深度/模板附件指针
                                           ? &depthStencilAttachmentReference.value()  // 如果有深度附件，使用它
                                           : nullptr,                                   // 否则为空
        };

        // === 步骤5：创建子通道依赖关系 ===
        // 与第一个构造函数相同的依赖关系设置，确保正确的同步
        std::array<VkSubpassDependency, 2> dependencies{};
        
        // === 子步骤5.1：外部到子通道的依赖 ===
        dependencies[0] = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,                      // 源：外部操作
                .dstSubpass = 0,                                        // 目标：子通道0
                .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,   // 源阶段：管线底部
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |      // 目标阶段：多个阶段
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,             // 源访问：内存读取
                .dstAccessMask =                                        // 目标访问：多种访问类型
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
        };

        // === 子步骤5.2：子通道到外部的依赖 ===
        dependencies[1] = {
                .srcSubpass = 0,                                        // 源：子通道0
                .dstSubpass = VK_SUBPASS_EXTERNAL,                      // 目标：外部操作
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |      // 源阶段：渲染输出阶段
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,   // 目标阶段：管线底部
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |               // 源访问：渲染结果
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,             // 目标访问：内存读取
        };

        // === 步骤6：配置多视图渲染（VR/AR支持） ===
        // 多视图渲染允许在单次渲染过程中生成多个视角的图像
        uint32_t viewmask = 0x00000003;         // 视图掩码：0b11表示视图0和视图1（双眼）
        uint32_t correlationMask = 0x00000003;  // 关联掩码：表示两个视图相关联
        const VkRenderPassMultiviewCreateInfo mvci = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,  // 结构体类型
                .subpassCount = 1,                                             // 子通道数量
                .pViewMasks = &viewmask,                                       // 视图掩码数组
                .correlationMaskCount = 1,                                     // 关联掩码数量
                .pCorrelationMasks = &correlationMask,                         // 关联掩码数组
        };

        // === 步骤7：创建VkRenderPassCreateInfo结构体 ===
        const VkRenderPassCreateInfo rpci = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,                 // 结构体类型
                .pNext = multiview ? &mvci : nullptr,                               // 多视图信息（可选）
                .attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size()), // 附件数量
                .pAttachments = attachmentDescriptors.data(),                       // 附件描述数组
                .subpassCount = 1,                                                  // 子通道数量
                .pSubpasses = &spd,                                                 // 子通道描述数组
                .dependencyCount = 2,                                               // 依赖关系数量
                .pDependencies = dependencies.data(),                               // 依赖关系数组
                // 注意：依赖关系设置较为宽泛，生产环境应根据具体需求优化
        };
        
        // === 步骤8：创建VkRenderPass对象 ===
        VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));
        
        // === 步骤9：设置调试名称 ===
        context.setVkObjectname(renderPass_, VK_OBJECT_TYPE_RENDER_PASS,
                                "Render pass: " + name);
    }  // 第二个构造函数结束

    /**
     * @brief 支持片段密度图的高级构造函数实现 - 性能优化专用
     * 
     * 这是最高级的构造函数，支持Vulkan的片段密度图扩展（VK_EXT_fragment_density_map）。
     * 片段密度图实现可变分辨率着色（VRS），是移动设备和VR应用的重要性能优化技术。
     * 
     * ## 片段密度图工作原理：
     * 1. **密度图定义**：一个小尺寸的纹理，每个像素定义对应区域的着色密度
     * 2. **可变着色率**：高密度区域每像素着色，低密度区域多像素共享着色
     * 3. **性能提升**：减少不重要区域的着色开销，提升整体性能
     * 4. **视觉优化**：模拟人眼视觉特性，中心清晰、边缘模糊
     * 
     * ## 实现步骤概述：
     * 1. 验证参数一致性
     * 2. 创建附件描述，包括片段密度图附件
     * 3. 创建附件引用，特殊处理片段密度图附件
     * 4. 配置子通道描述
     * 5. 设置子通道依赖关系
     * 6. 配置片段密度图扩展信息
     * 7. 可选的多视图配置
     * 8. 创建VkRenderPass对象
     * 9. 设置调试名称
     * 
     * ## 与前两个构造函数的区别：
     * - 支持VK_EXT_fragment_density_map扩展
     * - 需要特殊的片段密度图附件处理
     * - 与多视图渲染兼容
     * - 需要设备扩展支持
     */
    RenderPass::RenderPass(const Context& context, const std::vector<VkFormat>& formats,
                           const std::vector<VkImageLayout>& initialLayouts,
                           const std::vector<VkImageLayout>& finalLayouts,
                           const std::vector<VkAttachmentLoadOp>& loadOp,
                           const std::vector<VkAttachmentStoreOp>& storeOp,
                           VkPipelineBindPoint bindPoint,
                           std::vector<uint32_t> resolveAttachmentsIndices,
                           uint32_t depthAttachmentIndex, uint32_t fragmentDensityMapIndex,
                           uint32_t stencilAttachmentIndex, VkAttachmentLoadOp stencilLoadOp,
                           VkAttachmentStoreOp stencilStoreOp, bool multiview,
                           const std::string& name)
            : device_{context.device()} {  // 保存设备句柄用于析构
        // === 步骤1：严格的参数验证 ===
        const bool sameSizes =
                formats.size() == initialLayouts.size() && formats.size() == finalLayouts.size() &&
                formats.size() == loadOp.size() && formats.size() == storeOp.size();
        ASSERT(sameSizes,
               "The sizes of the attachments and their load and store operations and final "
               "layouts must match");

        // === 步骤2：准备数据结构 ===
        // 与前两个构造函数相似，但增加了片段密度图附件的处理
        std::vector<VkAttachmentDescription> attachmentDescriptors;    // 附件描述列表
        std::vector<VkAttachmentReference> colorAttachmentReferences;  // 颜色附件引用列表
        std::optional<VkAttachmentReference> depthStencilAttachmentReference; // 深度/模板附件引用（可选）
        uint32_t fragmentDensityAttachmentReference = UINT32_MAX;      // 片段密度图附件引用索引
        // === 步骤3：处理所有附件，包括片段密度图附件 ===
        for (uint32_t index = 0; index < formats.size(); ++index) {
            // === 子步骤3.1：创建附件描述 ===
            attachmentDescriptors.emplace_back(VkAttachmentDescription{
                    .format = formats[index],                           // 用户指定的像素格式
                    .samples = VK_SAMPLE_COUNT_1_BIT,                   // 默认单采样
                    .loadOp = loadOp[index],                            // 用户指定的加载操作
                    .storeOp = storeOp[index],                          // 用户指定的存储操作
                    .stencilLoadOp = index == stencilAttachmentIndex    // 模板加载操作
                                     ? stencilLoadOp
                                     : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = index == stencilAttachmentIndex   // 模板存储操作
                                      ? stencilStoreOp
                                      : VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = initialLayouts[index],            // 用户指定的初始布局
                    .finalLayout = finalLayouts[index],                // 用户指定的最终布局
            });
            
            // === 子步骤3.2：创建附件引用并分类 ===
            // 根据索引判断附件类型：深度/模板、片段密度图、或颜色附件
            if (index == depthAttachmentIndex || index == stencilAttachmentIndex) {
                // 深度或模板附件
                depthStencilAttachmentReference = VkAttachmentReference{
                        .attachment = index,
                        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                };
            } else if (index == fragmentDensityMapIndex) {
                // 片段密度图附件 - 特殊处理，不创建VkAttachmentReference
                // 而是保存索引，稍后用于VkRenderPassFragmentDensityMapCreateInfoEXT
                fragmentDensityAttachmentReference = index;
            } else {
                // 普通颜色附件
                colorAttachmentReferences.emplace_back(VkAttachmentReference{
                        .attachment = index,
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                });
            }
        }

        // === 步骤4：配置片段密度图扩展信息 ===
        // 只有在编译时支持VK_EXT_fragment_density_map扩展时才编译此代码
#if defined(VK_EXT_fragment_density_map)
        const VkRenderPassFragmentDensityMapCreateInfoEXT fdmAttachmentci = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT, // 结构体类型
                .fragmentDensityMapAttachment =                                              // 片段密度图附件引用
                        {
                                .attachment = fragmentDensityAttachmentReference,            // 附件索引
                                .layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, // 片段密度图专用布局
                        },
        };
#endif

        // === 步骤5：创建子通道描述 ===
        // 与前两个构造函数相同的子通道配置
        const VkSubpassDescription spd = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,              // 图形管线绑定点
                .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size()), // 颜色附件数量
                .pColorAttachments = colorAttachmentReferences.data(),             // 颜色附件数组指针
                .pDepthStencilAttachment = depthStencilAttachmentReference.has_value() // 深度/模板附件指针
                                           ? &depthStencilAttachmentReference.value()  // 如果有，使用它
                                           : nullptr,                                   // 否则为空
        };

        // === 步骤6：创建子通道依赖关系 ===
        // 与前两个构造函数相同的依赖关系设置
        std::array<VkSubpassDependency, 2> dependencies;
        dependencies[0] = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
        };

        dependencies[1] = {
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        };

        // === 步骤7：配置多视图渲染，支持片段密度图链接 ===
        uint32_t viewmask = 0x00000003;         // 视图掩码：双眼渲染
        uint32_t correlationMask = 0x00000003;  // 关联掩码：两个视图相关联
        const VkRenderPassMultiviewCreateInfo mvci = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,  // 结构体类型
#if defined(VK_EXT_fragment_density_map)
                // 如果有片段密度图附件，将其链接到多视图信息
                .pNext = fragmentDensityAttachmentReference < UINT32_MAX ? &fdmAttachmentci : nullptr,
#else
                .pNext = nullptr                                            // 无扩展支持时为空
#endif
                .subpassCount = 1,                                          // 子通道数量
                .pViewMasks = &viewmask,                                    // 视图掩码数组
                .correlationMaskCount = 1,                                  // 关联掩码数量
                .pCorrelationMasks = &correlationMask,                      // 关联掩码数组
        };

        // === 步骤7.1：创建VkRenderPassCreateInfo结构体 ===
        const VkRenderPassCreateInfo rpci = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,                 // 结构体类型
                .pNext = multiview ? &mvci : nullptr,                               // 多视图信息（可选）
                .attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size()), // 附件数量
                .pAttachments = attachmentDescriptors.data(),                       // 附件描述数组
                .subpassCount = 1,                                                  // 子通道数量
                .pSubpasses = &spd,                                                 // 子通道描述数组
                .dependencyCount = 2,                                               // 依赖关系数量
                .pDependencies = dependencies.data(),                               // 依赖关系数组
                // 注意：依赖关系设置较为宽泛，生产环境应根据具体需求优化
        };
        
        // === 步骤8：创建VkRenderPass对象 ===
        VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));
        
        // === 步骤9：设置调试名称 ===
        context.setVkObjectname(renderPass_, VK_OBJECT_TYPE_RENDER_PASS,
                                "Render pass (fdm support): " + name);
    }  // 第三个构造函数结束

    /**
     * @brief 析构函数 - 自动清理GPU资源
     * 
     * 使用RAII模式自动释放VkRenderPass对象。
     * 这确保了即使在异常情况下也不会发生GPU资源泄漏。
     * 
     * ## 清理过程：
     * 1. 调用vkDestroyRenderPass释放GPU资源
     * 2. 使用之前保存的device_句柄
     * 3. 传递nullptr作为分配器（使用默认分配器）
     * 
     * @note 在对象销毁时自动调用，无需手动管理
     * @warning 必须确保没有其他对象正在使用此RenderPass
     */
    RenderPass::~RenderPass() { 
        vkDestroyRenderPass(device_, renderPass_, nullptr); 
    }

    /**
     * @brief 获取底层的VkRenderPass对象
     * 
     * 返回原生的Vulkan渲染通道句柄，用于与其他Vulkan对象交互。
     * 这是一个const函数，不会修改对象状态。
     * 
     * ## 常见用途：
     * - 创建VkGraphicsPipeline时指定renderPass
     * - 创建VkFramebuffer时指定compatibleRenderPass
     * - 调用vkCmdBeginRenderPass时传递renderPass参数
     * - 查询渲染通道的兼容性
     * 
     * @return VkRenderPass 原生Vulkan渲染通道句柄
     * 
     * @note [[nodiscard]]属性提醒调用者不要忽略返回值
     * @warning 返回的句柄只在此对象生命周期内有效
     */
    VkRenderPass RenderPass::vkRenderPass() const { 
        return renderPass_; 
    }

}  // namespace VkCore

/**
 * @page renderpass_implementation_guide RenderPass.cpp 实现详解
 * 
 * ## 文件总结
 * 
 * RenderPass.cpp 实现了完整的Vulkan渲染通道管理功能，为初学者提供了三种不同的创建方式：
 * 
 * ### 1. 基于纹理对象的构造函数
 * 
 * **适用场景**：离屏渲染、后处理效果
 * **特点**：
 * - 自动从纹理对象提取格式和属性
 * - 自动识别深度/模板纹理
 * - 支持MSAA解析附件
 * - 实现简单直观
 * 
 * **核心流程**：
 * 1. 从Texture对象提取VkFormat、VkSampleCount等属性
 * 2. 根据isDepth()/isStencil()自动分类附件类型
 * 3. 创建标准的单子通道渲染通道
 * 4. 设置宽泛的依赖关系确保兼容性
 * 
 * ### 2. 基于格式的标准构造函数
 * 
 * **适用场景**：VR/AR应用、复杂多附件渲染
 * **特点**：
 * - 完全基于用户参数，不依赖纹理对象
 * - 支持多视图渲染（VR双眼）
 * - 可精确指定深度和模板附件索引
 * - 支持自定义模板操作
 * - 更严格的参数验证
 * 
 * **核心流程**：
 * 1. 严格验证所有参数数组大小一致性
 * 2. 根据用户指定的索引分类附件
 * 3. 可选的多视图配置（VkRenderPassMultiviewCreateInfo）
 * 4. 更精确的依赖关系设置
 * 
 * ### 3. 支持片段密度图的构造函数
 * 
 * **适用场景**：移动设备优化、VR注视点渲染
 * **特点**：
 * - 支持VK_EXT_fragment_density_map扩展
 * - 实现可变分辨率着色
 * - 显著提升移动设备性能
 * - 与多视图渲染兼容
 * 
 * **核心流程**：
 * 1. 配置片段密度图附件
 * 2. 设置VkRenderPassFragmentDensityMapCreateInfoEXT
 * 3. 与多视图信息链接
 * 4. 创建支持VRS的渲染通道
 * 
 * ## 关键设计模式
 * 
 * ### 1. RAII资源管理
 * ```cpp
 * // 构造函数中创建资源
 * VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));
 * 
 * // 析构函数中自动清理
 * ~RenderPass() { vkDestroyRenderPass(device_, renderPass_, nullptr); }
 * ```
 * 
 * ### 2. 移动语义优化
 * ```cpp
 * MOVABLE_ONLY(RenderPass);  // 只允许移动，禁止复制
 * ```
 * 
 * ### 3. 调试友好设计
 * ```cpp
 * // 为所有GPU对象设置有意义的名称
 * context.setVkObjectname(renderPass_, VK_OBJECT_TYPE_RENDER_PASS, "Render pass: " + name);
 * ```
 * 
 * ## 性能优化要点
 * 
 * ### 1. 依赖关系优化
 * 当前实现使用较为宽泛的依赖关系设置，生产环境中应该：
 * - 根据具体渲染需求精确设置srcStageMask和dstStageMask
 * - 只包含实际需要的访问掩码
 * - 避免不必要的VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
 * 
 * ### 2. 附件操作优化
 * ```cpp
 * // 优化loadOp和storeOp的选择
 * .loadOp = needsPreviousContent ? VK_ATTACHMENT_LOAD_OP_LOAD 
 *                                : VK_ATTACHMENT_LOAD_OP_CLEAR,
 * .storeOp = needsResult ? VK_ATTACHMENT_STORE_OP_STORE 
 *                        : VK_ATTACHMENT_STORE_OP_DONT_CARE
 * ```
 * 
 * ### 3. 移动设备特殊优化
 * - 尽量使用VK_ATTACHMENT_STORE_OP_DONT_CARE减少带宽
 * - 考虑使用片段密度图进行VRS优化
 * - 避免不必要的布局转换
 * 
 * ## 常见问题和解决方案
 * 
 * ### 1. 验证层错误："incompatible render pass"
 * **原因**：RenderPass与FrameBuffer或Pipeline不兼容
 * **解决**：确保格式、采样数、布局等完全匹配
 * 
 * ### 2. 性能问题
 * **原因**：依赖关系设置过于保守
 * **解决**：根据实际需求优化管线阶段和访问掩码
 * 
 * ### 3. 多视图渲染问题
 * **原因**：viewMask或correlationMask设置错误
 * **解决**：确保掩码正确反映视图数量和关联关系
 * 
 * ## 扩展建议
 * 
 * ### 1. 添加RenderPass2支持
 * ```cpp
 * // 使用更现代的VkRenderPassCreateInfo2
 * VkRenderPassCreateInfo2 createInfo2{};
 * // ... 配置
 * vkCreateRenderPass2(device_, &createInfo2, nullptr, &renderPass_);
 * ```
 * 
 * ### 2. 动态渲染支持
 * ```cpp
 * // 考虑支持VK_KHR_dynamic_rendering扩展
 * // 减少RenderPass的预先创建需求
 * ```
 * 
 * ### 3. 更多扩展支持
 * - VK_KHR_imageless_framebuffer：无图像帧缓冲
 * - VK_KHR_separate_depth_stencil_layouts：分离深度模板布局
 * - VK_EXT_multisampled_render_to_single_sampled：MSAA优化
 */