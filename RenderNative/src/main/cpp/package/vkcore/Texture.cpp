/**
 * @file Texture.cpp
 * @brief Vulkan纹理管理类的实现 - 为初学者详细解释
 * 
 * 本文件实现了Texture类的所有功能，包括：
 * - 纹理的创建和销毁
 * - 纹理数据的上传
 * - Mipmap的生成
 * - 图像布局的转换
 * - 多队列间的资源共享
 * 
 * 每个函数都包含详细的步骤说明和Vulkan概念解释，
 * 帮助初学者理解GPU纹理管理的底层原理。
 */

#include "Texture.h"

#ifdef _WIN32
#include <vulkan/vk_enum_string_helper.h>  // Windows平台的Vulkan枚举字符串辅助工具
#endif


#include "Buffer.h"   // 缓冲区类，用于数据传输
#include "Context.h"  // Vulkan上下文类，管理设备和资源

namespace VkCore {

    /**
     * @brief 主构造函数 - 创建新的Vulkan纹理
     * 
     * 这个构造函数执行以下关键步骤：
     * 1. 初始化成员变量
     * 2. 验证输入参数的合法性
     * 3. 创建VkImage对象
     * 4. 分配GPU内存
     * 5. 创建默认的ImageView
     * 
     * ## Vulkan纹理创建流程解释：
     * 在Vulkan中，创建纹理需要多个步骤，不像OpenGL那样简单。
     * 这是因为Vulkan给了开发者更精细的控制权，但也增加了复杂性。
     */
    Texture::Texture(const Context& context, VkImageType type, VkFormat format,
                     VkImageCreateFlags flags, VkImageUsageFlags usageFlags,
                     VkExtent3D extents, uint32_t numMipLevels, uint32_t layerCount,
                     VkMemoryPropertyFlags memoryFlags, bool generateMips,
                     VkSampleCountFlagBits msaaSamples, const std::string& name,
                     bool multiview, VkImageTiling imageTiling)
            // 成员初始化列表 - 在构造函数体执行前初始化所有成员变量
            : context_{context},                          // 保存Vulkan上下文的引用
              vmaAllocator_{context.memoryAllocator()},   // 获取VMA内存分配器
              usageFlags_{usageFlags},                    // 保存使用标志
              flags_{flags},                              // 保存创建标志
              type_{type},                                // 保存图像类型
              format_{format},                            // 保存像素格式
              extents_{extents},                          // 保存图像尺寸
              ownsVkImage_{true},                         // 标记我们拥有VkImage所有权
              mipLevels_(numMipLevels),                   // 保存Mipmap层级数
              layerCount_(layerCount),                    // 保存数组层数
              multiview_(multiview),                      // 保存多视图标志
              generateMips_(generateMips),                // 保存是否生成Mipmap
              msaaSamples_(msaaSamples),                  // 保存MSAA采样数
              imageTiling_(imageTiling),                  // 保存内存排列方式
              debugName_(name) {                          // 保存调试名称
        // === 步骤1：参数验证 ===
        // 确保纹理尺寸合法（不能为0）
        ASSERT(extents.width > 0 && extents.height > 0,
               "Texture cannot have dimensions equal to 0");
        // 确保至少有一个Mipmap层级
        ASSERT(mipLevels_ > 0, "Texture must have at least one mip level");

        // === 步骤2：自动计算Mipmap层级数 ===
        if (generateMips_) {
            // 如果启用自动Mipmap生成，计算完整的Mipmap链层级数
            // 从原始尺寸一直到1x1像素
            mipLevels_ = getMipLevelsCount(extents.width, extents.height);
        }

        // === 步骤3：MSAA与Mipmap兼容性检查 ===
        // Vulkan规定：多重采样图像不能有多个Mipmap层级
        // 这是因为MSAA和Mipmap的内存布局冲突
        ASSERT(!(mipLevels_ > 1 && msaaSamples_ != VK_SAMPLE_COUNT_1_BIT),
               "Multisampled images cannot have more than 1 mip level");

        // === 步骤4：创建VkImageCreateInfo结构体 ===
        // 这个结构体告诉Vulkan如何创建图像对象
        const VkImageCreateInfo imageInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,    // 结构体类型标识
                .flags = flags,                                  // 创建标志（如立方体贴图）
                .imageType = type,                               // 图像类型（1D/2D/3D）
                .format = format,                                // 像素格式（RGBA8等）
                .extent = extents,                               // 图像尺寸（宽x高x深）
                .mipLevels = mipLevels_,                        // Mipmap层级数
                .arrayLayers = layerCount,                       // 数组层数（立方体贴图为6）
                .samples = msaaSamples_,                         // 多重采样级别
                .tiling = imageTiling_,                          // 内存排列方式
                .usage = usageFlags,                             // 使用方式（采样、渲染目标等）
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,        // 独占模式（单队列访问）
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,      // 初始布局（未定义）
        };

        // === 步骤5：配置VMA内存分配 ===
        // VMA（Vulkan Memory Allocator）简化了GPU内存管理
        const VmaAllocationCreateInfo allocCreateInfo = {
                .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,  // 使用专用内存分配
                // 根据内存标志选择最佳内存类型：
                .usage = memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST      // CPU可见内存（较慢但可直接访问）
                         : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,   // GPU专用内存（最快但CPU不能直接访问）
                .priority = 1.0f,  // 最高内存分配优先级
        };

        // === 步骤6：创建VkImage并分配GPU内存 ===
        // 这是最关键的步骤：同时创建图像对象和分配内存
        // VMA自动处理内存分配、绑定等复杂操作
        VK_CHECK(vmaCreateImage(vmaAllocator_, &imageInfo, &allocCreateInfo, &image_,
                                &vmaAllocation_, nullptr));

        // === 步骤7：获取内存使用信息 ===
        if (vmaAllocation_ != nullptr) {
            VmaAllocationInfo allocationInfo;
            vmaGetAllocationInfo(vmaAllocator_, vmaAllocation_, &allocationInfo);
            deviceSize_ = allocationInfo.size;  // 记录实际分配的GPU内存大小
        }

        // === 步骤8：设置调试名称 ===
        // 在GPU调试工具中显示有意义的名称，便于问题定位
        context.setVkObjectname(image_, VK_OBJECT_TYPE_IMAGE, "Image: " + name);

        // === 步骤9：确定ImageView类型 ===
        // 根据图像类型、创建标志和多视图设置确定合适的视图类型
        const VkImageViewType imageViewType =
                VkCore::imageTypeToImageViewType(type, flags, multiview_);

        viewType_ = imageViewType;  // 保存视图类型供后续使用

        // === 步骤10：创建默认ImageView ===
        // ImageView定义了如何访问VkImage中的数据
        // 这个默认视图包含所有Mipmap层级和数组层
        imageView_ =
                createImageView(context, imageViewType, format_, mipLevels_, layerCount, name);
    }

    /**
     * @brief 包装构造函数 - 包装已存在的VkImage对象
     * 
     * 这个构造函数用于包装外部创建的VkImage（如交换链图像）。
     * 与主构造函数不同，这里不分配新的GPU内存，只是创建管理接口。
     * 
     * ## 使用场景：
     * - 交换链图像：用于显示到屏幕的最终图像
     * - 外部库创建的图像
     * - 从文件加载器获得的图像
     * 
     * ## 关键区别：
     * - ownsVkImage_ = false：不拥有VkImage所有权
     * - 不使用VMA分配内存
     * - 只创建ImageView用于访问
     */
    Texture::Texture(const Context& context, VkDevice device, VkImage image, VkFormat format,
                     VkExtent3D extents, uint32_t numlayers, bool multiview,
                     const std::string& name)
            // 成员初始化列表 - 注意与主构造函数的区别
            : context_{context},        // 保存Vulkan上下文引用
              image_{image},            // 保存外部传入的VkImage（不是我们创建的）
              format_{format},          // 保存像素格式
              extents_{extents},        // 保存图像尺寸
              layerCount_(numlayers),   // 保存数组层数
              multiview_(multiview),    // 保存多视图标志
              ownsVkImage_{false},      // 关键：标记我们不拥有VkImage所有权
              debugName_{name} {        // 保存调试名称
        
        // === 步骤1：设置调试名称 ===
        // 即使是外部图像，也要设置调试名称便于识别
        context.setVkObjectname(image_, VK_OBJECT_TYPE_IMAGE, "Image: " + name);

        // === 步骤2：创建ImageView ===
        // 根据是否多视图选择合适的视图类型
        // 多视图：VK_IMAGE_VIEW_TYPE_2D_ARRAY（VR双眼渲染）
        // 单视图：VK_IMAGE_VIEW_TYPE_2D（普通2D纹理）
        imageView_ = createImageView(
                context, 
                !multiview_ ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY, 
                format,
                1,              // 只有1个Mipmap层级（外部图像通常不带Mipmap）
                layerCount_,    // 数组层数
                name);
    }

    /**
     * @brief 析构函数 - 清理所有GPU资源
     * 
     * 析构函数负责释放所有分配的Vulkan资源，防止内存泄漏。
     * 清理顺序很重要：先清理依赖的资源，再清理基础资源。
     * 
     * ## 清理顺序：
     * 1. 销毁所有特定Mipmap层级的ImageView
     * 2. 销毁默认的ImageView
     * 3. 如果拥有所有权，销毁VkImage和释放GPU内存
     * 
     * ## 所有权管理：
     * 只有当ownsVkImage_=true时才销毁VkImage，
     * 否则说明图像由外部管理（如交换链）。
     */
    Texture::~Texture() {
        // === 步骤1：销毁特定Mipmap层级的ImageView ===
        // 遍历imageViewFramebuffers_映射，销毁所有按需创建的ImageView
        for (const auto imageView : imageViewFramebuffers_) {
            // imageView.first 是Mipmap层级索引
            // imageView.second 是对应的VkImageView句柄
            vkDestroyImageView(context_.device(), imageView.second, nullptr);
        }

        // === 步骤2：销毁默认ImageView ===
        // 这是在构造函数中创建的主要ImageView
        vkDestroyImageView(context_.device(), imageView_, nullptr);

        // === 步骤3：根据所有权决定是否销毁VkImage ===
        if (ownsVkImage_) {
            // 我们拥有VkImage所有权，需要销毁它并释放GPU内存
            // vmaDestroyImage同时销毁VkImage和释放VMA分配的内存
            vmaDestroyImage(vmaAllocator_, image_, vmaAllocation_);
        }
        // 如果ownsVkImage_=false，说明VkImage由外部管理，
        // 我们不应该销毁它（如交换链图像由交换链管理）
    }

    /**
     * @brief 获取指定Mipmap层级的图像视图
     * 
     * 这个函数实现了按需创建ImageView的机制。对于经常使用的默认视图，
     * 直接返回；对于特定Mipmap层级的视图，按需创建并缓存。
     * 
     * ## 工作原理：
     * 1. 验证请求的Mipmap层级是否有效
     * 2. 如果请求默认视图，直接返回
     * 3. 如果请求特定层级视图，检查缓存
     * 4. 如果缓存中没有，创建新视图并缓存
     * 
     * ## 性能优化：
     * - 使用缓存避免重复创建相同的ImageView
     * - 按需创建，避免创建不使用的视图
     */
    VkImageView Texture::vkImageView(uint32_t mipLevel) {
        // === 步骤1：验证Mipmap层级有效性 ===
        // UINT32_MAX表示请求默认视图（所有层级）
        // 否则检查层级索引是否在有效范围内
        ASSERT(mipLevel == UINT32_MAX || mipLevel < mipLevels_, "Invalid mip level");

        // === 步骤2：处理默认视图请求 ===
        if (mipLevel == UINT32_MAX) {
            // 返回包含所有Mipmap层级的默认视图
            // 这是最常用的视图，用于普通的纹理采样
            return imageView_;
        }

        // === 步骤3：检查特定层级视图缓存 ===
        if (imageViewFramebuffers_.find(mipLevel) == imageViewFramebuffers_.end()) {
            // 缓存中没有找到，需要创建新的ImageView
            
            // 确定视图类型（与原图像保持一致）
            const VkImageViewType imageViewType =
                    VkCore::imageTypeToImageViewType(type_, flags_, multiview_);

            // 创建只包含单个Mipmap层级的ImageView
            // 参数说明：
            // - imageViewType: 视图类型（2D、立方体等）
            // - format_: 像素格式
            // - 1: 只包含1个Mipmap层级（当前请求的层级）
            // - VK_REMAINING_ARRAY_LAYERS: 包含所有数组层
            imageViewFramebuffers_[mipLevel] =
                    createImageView(context_, imageViewType, format_, 1, VK_REMAINING_ARRAY_LAYERS,
                                    "Image View for Framebuffer: " + debugName_);
        }

        // === 步骤4：返回缓存的视图 ===
        return imageViewFramebuffers_[mipLevel];
    }

    /**
     * @brief 上传纹理数据并生成Mipmap链
     * 
     * 这是一个便利函数，组合了数据上传和Mipmap生成两个操作。
     * 适用于需要完整Mipmap链的纹理（如用于渲染的贴图）。
     * 
     * ## 执行流程：
     * 1. 调用uploadOnly()上传基础层级（层级0）的数据
     * 2. 生成所有Mipmap层级
     * 3. 确保最终布局为着色器只读优化
     * 
     * ## 什么是Mipmap？
     * Mipmap是同一纹理的多个分辨率版本：
     * - 层级0: 原始分辨率（如512x512）
     * - 层级1: 一半分辨率（如256x256）
     * - 层级2: 四分之一分辨率（如128x128）
     * - ...直到1x1像素
     * 
     * GPU会根据距离自动选择合适的层级，提高性能和视觉质量。
     */
    void Texture::uploadAndGenMips(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                                   void* data) {
        // === 步骤1：上传基础数据 ===
        // 上传层级0（原始分辨率）的纹理数据
        uploadOnly(cmdBuffer, stagingBuffer, data);
        
        // === 步骤2：开始调试标签 ===
        // 在GPU调试工具中标记这个操作组，便于性能分析
        // context_.beginDebugUtilsLabel(cmdBuffer, "Transition to Shader_Read_Only & Generate mips", {1.0f, 0.0f, 0.0f, 1.0f});
        
        // === 步骤3：生成Mipmap链 ===
        // 从层级0开始，逐层生成缩小版本，直到1x1像素
        generateMips(cmdBuffer);
        
        // === 步骤4：确保最终布局正确 ===
        // generateMips()通常会设置正确的布局，但为了保险起见再检查一次
        if (layout_ != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            transitionImageLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // === 步骤5：结束调试标签 ===
        context_.endDebugUtilsLabel(cmdBuffer);
    }

    /**
     * @brief 仅上传纹理数据（不生成Mipmap）
     * 
     * 这个函数执行纯粹的数据上传操作，将CPU内存中的像素数据
     * 复制到GPU纹理中的指定数组层。
     * 
     * ## 为什么需要Staging Buffer？
     * GPU内存通常不能直接从CPU访问，需要通过中间缓冲区：
     * CPU内存 → Staging Buffer → GPU纹理
     * 
     * ## 执行流程：
     * 1. 将CPU数据复制到staging buffer
     * 2. 确保图像布局适合数据传输
     * 3. 执行buffer到image的复制操作
     * 
     * ## 数组层概念：
     * - layer=0: 普通2D纹理
     * - layer=0-5: 立方体贴图的6个面
     * - layer=0-N: 纹理数组的不同层
     */
    void Texture::uploadOnly(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                             void* data, uint32_t layer) {
        // === 步骤1：开始调试标签 ===
        // context_.beginDebugUtilsLabel(cmdBuffer, "Uploading image", {1.0f, 0.0f, 0.0f, 1.0f});

        // === 步骤2：将CPU数据复制到Staging Buffer ===
        // 计算需要复制的字节数：像素大小 × 宽 × 高 × 深度
        stagingBuffer->copyDataToBuffer(
                data, pixelSizeInBytes() * extents_.width * extents_.height * extents_.depth);

        // === 步骤3：确保图像布局适合数据传输 ===
        if (layout_ == VK_IMAGE_LAYOUT_UNDEFINED) {
            // 如果图像布局未定义（初始状态），转换为传输目标布局
            // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: 优化接收数据传输的布局
            transitionImageLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }

        // === 步骤4：确定图像方面掩码 ===
        // 不同类型的图像需要不同的方面掩码：
        // - 深度图像：VK_IMAGE_ASPECT_DEPTH_BIT
        // - 模板图像：VK_IMAGE_ASPECT_STENCIL_BIT
        // - 普通颜色图像：VK_IMAGE_ASPECT_COLOR_BIT
        const VkImageAspectFlags aspectMask =
                isDepth() ? isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                                        : VK_IMAGE_ASPECT_DEPTH_BIT
                          : VK_IMAGE_ASPECT_COLOR_BIT;
        
        // === 步骤5：配置Buffer到Image的复制操作 ===
        const VkBufferImageCopy bufCopy = {
                .bufferOffset = 0,                          // 从buffer的起始位置开始复制
                .imageSubresource =                         // 目标图像的子资源信息
                        {
                                .aspectMask = aspectMask,           // 图像方面（颜色/深度/模板）
                                .mipLevel = 0,                      // 目标Mipmap层级（层级0是原始分辨率）
                                .baseArrayLayer = layer,            // 目标数组层索引
                                .layerCount = 1,                    // 复制1个数组层
                        },
                .imageOffset =                              // 图像内的偏移（从哪里开始写入）
                        {
                                .x = 0,                             // X偏移为0（从左边开始）
                                .y = 0,                             // Y偏移为0（从顶部开始）
                                .z = 0,                             // Z偏移为0（从前面开始）
                        },
                .imageExtent = extents_,                    // 复制的区域大小（整个图像）
        };
        
        // === 步骤6：执行Buffer到Image的复制 ===
        // 这是实际的数据传输操作，由GPU执行
        vkCmdCopyBufferToImage(cmdBuffer, 
                               stagingBuffer->vkBuffer(),          // 源buffer
                               image_,                              // 目标image
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // 目标布局
                               1,                                   // 复制操作数量
                               &bufCopy);                           // 复制操作描述
        
        // === 步骤7：结束调试标签 ===
        context_.endDebugUtilsLabel(cmdBuffer);
    }

    /**
     * @brief 添加释放屏障 - 多队列资源所有权转移的第一阶段
     * 
     * 在多队列Vulkan应用中，当资源需要从一个队列家族转移到另一个时，
     * 需要使用两阶段屏障：释放屏障和获取屏障。这是释放阶段。
     * 
     * ## 什么是队列家族？
     * 队列家族是支持相同操作类型的队列集合：
     * - 图形队列家族：支持绘制操作
     * - 计算队列家族：支持计算着色器
     * - 传输队列家族：支持数据复制
     * 
     * ## 为什么需要所有权转移？
     * 不同队列家族可能有独立的缓存系统，需要确保数据一致性。
     * 
     * ## 使用流程：
     * 1. 在源队列执行释放屏障（此函数）
     * 2. 提交源队列命令
     * 3. 在目标队列执行获取屏障
     */
    void Texture::addReleaseBarrier(VkCommandBuffer cmdBuffer, uint32_t srcQueueFamilyIndex,
                                    uint32_t dstQueueFamilyIndex) {
        // === 创建释放屏障结构体 ===
        VkImageMemoryBarrier2 releaseBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,      // 结构体类型
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,       // 源管线阶段（传输完成后）
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,          // 源访问类型（传输写入）
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,   // 目标管线阶段（所有后续命令）
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,             // 目标访问类型（着色器读取）
                .srcQueueFamilyIndex = srcQueueFamilyIndex,             // 源队列家族索引
                .dstQueueFamilyIndex = dstQueueFamilyIndex,             // 目标队列家族索引
                .image = image_,                                        // 要转移的图像
                .subresourceRange = {                                   // 子资源范围
                    VK_IMAGE_ASPECT_COLOR_BIT,                          // 颜色方面
                    0,                                                  // 基础Mip层级
                    mipLevels_,                                         // Mip层级数量
                    0,                                                  // 基础数组层
                    1                                                   // 数组层数量
                },
        };

        // === 创建依赖信息结构体 ===
        VkDependencyInfo dependency_info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,             // 结构体类型
                .imageMemoryBarrierCount = 1,                           // 图像内存屏障数量
                .pImageMemoryBarriers = &releaseBarrier,                // 图像内存屏障指针
        };

        // === 插入管线屏障命令 ===
        // 这个命令告诉GPU在继续执行前等待屏障条件满足
        vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
    }

    /**
     * @brief 添加获取屏障 - 多队列资源所有权转移的第二阶段
     * 
     * 这是多队列所有权转移的第二阶段，必须与释放屏障配对使用。
     * 在目标队列上执行此屏障，以获得资源的所有权。
     * 
     * ## 完整的所有权转移流程：
     * ```cpp
     * // 在图形队列上释放纹理所有权
     * texture.addReleaseBarrier(graphicsCmd, graphicsFamily, computeFamily);
     * vkQueueSubmit(graphicsQueue, ...);
     * 
     * // 在计算队列上获取纹理所有权
     * texture.addAcquireBarrier(computeCmd, graphicsFamily, computeFamily);
     * vkQueueSubmit(computeQueue, ...);
     * ```
     * 
     * ## 何时需要所有权转移？
     * - 在图形队列上渲染纹理，然后在计算队列上处理
     * - 在专用传输队列上上传数据，然后在图形队列上使用
     * - 任何跨队列家族的资源访问场景
     */
    void Texture::addAcquireBarrier(VkCommandBuffer cmdBuffer, uint32_t srcQueueFamilyIndex,
                                    uint32_t dstQueueFamilyIndex) {
        // === 创建获取屏障结构体 ===
        VkImageMemoryBarrier2 acquireBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,          // 结构体类型
                .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,    // 目标管线阶段（片段着色器）
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,               // 目标访问类型（内存读取）
                .srcQueueFamilyIndex = srcQueueFamilyIndex,                 // 源队列家族索引（与释放屏障相同）
                .dstQueueFamilyIndex = dstQueueFamilyIndex,                 // 目标队列家族索引（与释放屏障相同）
                .image = image_,                                            // 要获取的图像
                .subresourceRange = {                                       // 子资源范围
                    VK_IMAGE_ASPECT_COLOR_BIT,                              // 颜色方面
                    0,                                                      // 基础Mip层级
                    mipLevels_,                                             // Mip层级数量
                    0,                                                      // 基础数组层
                    1                                                       // 数组层数量
                },
        };

        // === 创建依赖信息结构体 ===
        VkDependencyInfo dependency_info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,                 // 结构体类型
                .imageMemoryBarrierCount = 1,                               // 图像内存屏障数量
                .pImageMemoryBarriers = &acquireBarrier,                    // 图像内存屏障指针
        };

        // === 插入管线屏障命令 ===
        // 这个命令确保在访问资源前，所有权转移已完成
        vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
    }

    /**
     * @brief 转换图像布局 - Vulkan中最重要的操作之一
     * 
     * 图像布局转换是Vulkan的核心概念。不同的操作需要不同的布局以获得最佳性能：
     * - 上传数据时需要TRANSFER_DST布局
     * - 着色器采样时需要SHADER_READ_ONLY布局
     * - 渲染目标时需要COLOR_ATTACHMENT布局
     * 
     * 这个函数自动处理复杂的布局转换逻辑，包括：
     * - 确定正确的管线阶段
     * - 设置适当的访问掩码
     * - 插入内存屏障确保同步
     * 
     * ## 为什么需要布局转换？
     * GPU为不同的操作优化了不同的内存访问模式：
     * - 渲染目标需要快速的随机写入访问
     * - 纹理采样需要优化的读取访问模式
     * - 数据传输需要线性的内存访问
     * 
     * ## 管线阶段和访问掩码：
     * - 管线阶段：定义GPU执行的哪个阶段需要等待
     * - 访问掩码：定义内存的哪种访问类型需要同步
     */
    void Texture::transitionImageLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout) {
        // === 初始化屏障参数 ===
        VkAccessFlags srcAccessMask = VK_ACCESS_NONE;                           // 源访问掩码
        VkAccessFlags dstAccessMask = VK_ACCESS_NONE;                           // 目标访问掩码
        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;   // 源管线阶段
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // 目标管线阶段

        // === 定义常用的管线阶段掩码 ===
        // 深度测试相关的管线阶段
        constexpr VkPipelineStageFlags depthStageMask =
                0 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |    // 早期深度测试
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;          // 晚期深度测试

        // 着色器采样相关的管线阶段
        constexpr VkPipelineStageFlags sampledStageMask =
                0 | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |           // 顶点着色器
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |             // 片段着色器
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;               // 计算着色器

        // === 调试输出（已注释） ===
        // 在调试时可以启用此代码来跟踪布局转换
        /*std::cerr << "[transImgeLayot] Transitioning image " << debugName_ << " from "
                  << string_VkImageLayout(layout_) << " to " << string_VkImageLayout(newLayout)
                  << std::endl;*/

        // === 获取当前布局 ===
        auto oldLayout = layout_;

        // === 优化：如果布局相同，直接返回 ===
        if (oldLayout == newLayout) {
            return;  // 不需要转换，节省GPU时间
        }

        // === 根据源布局确定源屏障参数 ===
        // 这个switch语句为每种可能的源布局设置正确的管线阶段和访问掩码
        switch (oldLayout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // 未定义布局：图像刚创建，内容未定义
                // 不需要等待任何操作，因为没有有效数据
                break;

            case VK_IMAGE_LAYOUT_GENERAL:
                // 通用布局：支持所有操作但性能不是最优
                sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;      // 等待所有命令完成
                srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;            // 同步内存写入
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // 颜色附件优化布局：用作渲染目标
                sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // 等待颜色输出完成
                srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;         // 同步颜色写入
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // 深度模板附件优化布局：用作深度缓冲
                sourceStage = depthStageMask;                                 // 等待深度测试完成
                srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // 同步深度写入
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                // 深度模板只读优化布局：深度缓冲同时被采样
                sourceStage = depthStageMask | sampledStageMask;  // 等待深度测试和着色器采样
                // 只读操作，不需要同步写入
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // 着色器只读优化布局：在着色器中被采样
                sourceStage = sampledStageMask;  // 等待着色器采样完成
                // 只读操作，不需要同步写入
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // 传输源优化布局：用作复制操作的源
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;  // 等待传输操作完成
                // 源操作是读取，不需要同步写入
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // 传输目标优化布局：用作复制操作的目标
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;     // 等待传输操作完成
                srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;     // 同步传输写入
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // 预初始化布局：CPU已写入数据
                sourceStage = VK_PIPELINE_STAGE_HOST_BIT;         // 等待CPU操作完成
                srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;         // 同步CPU写入
                break;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                // 显示源布局：用于交换链显示
                // vkQueuePresentKHR会自动处理可见性操作，不需要额外同步
                break;

            default:
                // 未知布局：这是一个编程错误
                ASSERT(false, "Unknown image layout.");
                break;
        }

        // === 根据目标布局确定目标屏障参数 ===
        // 这个switch语句为每种可能的目标布局设置正确的管线阶段和访问掩码
        switch (newLayout) {
            case VK_IMAGE_LAYOUT_GENERAL:
            case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
                // 通用布局/片段密度图优化布局：支持所有操作
                destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;                  // 在所有阶段前完成
                dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT; // 允许读写访问
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // 颜色附件优化布局：准备用作渲染目标
                destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;       // 在颜色输出阶段前完成
                dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |                   // 允许颜色读取
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;                   // 允许颜色写入
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // 深度模板附件优化布局：准备用作深度缓冲
                destinationStage = depthStageMask;                                      // 在深度测试阶段前完成
                dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |           // 允许深度读取
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;           // 允许深度写入
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                // 深度模板只读优化布局：深度缓冲同时用于测试和采样
                destinationStage = depthStageMask | sampledStageMask;                   // 在深度测试和采样前完成
                dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |           // 允许深度读取
                                VK_ACCESS_SHADER_READ_BIT |                             // 允许着色器读取
                                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;                    // 允许输入附件读取
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // 着色器只读优化布局：准备在着色器中被采样
                destinationStage = sampledStageMask;                                    // 在着色器阶段前完成
                dstAccessMask = VK_ACCESS_SHADER_READ_BIT |                             // 允许着色器读取
                                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;                    // 允许输入附件读取
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // 传输源优化布局：准备用作复制操作的源
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;                      // 在传输阶段前完成
                dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;                            // 允许传输读取
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // 传输目标优化布局：准备用作复制操作的目标
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;                      // 在传输阶段前完成
                dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;                           // 允许传输写入
                break;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                // 显示源布局：准备用于交换链显示
                // vkQueuePresentKHR会自动执行可见性操作，不需要显式的访问掩码
                break;

            default:
                // 未知布局：这是一个编程错误
                ASSERT(false, "Unknown image layout.");
                break;
        }

        // === 确定图像方面掩码 ===
        // 根据图像类型选择正确的方面掩码
        const VkImageAspectFlags aspectMask =
                isDepth() ? isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT  // 深度+模板
                                        : VK_IMAGE_ASPECT_DEPTH_BIT                              // 仅深度
                          : VK_IMAGE_ASPECT_COLOR_BIT;                                           // 颜色

        // === 创建图像内存屏障 ===
        const VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,    // 结构体类型
                .srcAccessMask = srcAccessMask,                     // 源访问掩码（之前的操作类型）
                .dstAccessMask = dstAccessMask,                     // 目标访问掩码（后续的操作类型）
                .oldLayout = layout_,                               // 当前布局
                .newLayout = newLayout,                             // 目标布局
                .image = image_,                                    // 要转换的图像
                .subresourceRange =                                 // 子资源范围
                        {
                                .aspectMask = aspectMask,                   // 图像方面（颜色/深度/模板）
                                .baseMipLevel = 0,                          // 起始Mip层级
                                .levelCount = mipLevels_,                   // Mip层级数量
                                .baseArrayLayer = 0,                        // 起始数组层
                                .layerCount = multiview_ ? VK_REMAINING_ARRAY_LAYERS : 1,  // 数组层数量
                        },
        };

        // === 插入管线屏障命令 ===
        // 这是实际执行布局转换的关键命令
        // 参数说明：
        // - cmdBuffer: 命令缓冲区
        // - sourceStage: 源管线阶段（等待这些阶段完成）
        // - destinationStage: 目标管线阶段（在这些阶段前完成转换）
        // - 0: 依赖标志（通常为0）
        // - 0, nullptr: 内存屏障数量和指针（这里不使用）
        // - 0, nullptr: 缓冲区内存屏障数量和指针（这里不使用）
        // - 1, &barrier: 图像内存屏障数量和指针
        vkCmdPipelineBarrier(cmdBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        // === 更新内部状态 ===
        // 记录新的布局状态，供后续操作使用
        layout_ = newLayout;
    }

    /**
     * @brief 检查纹理是否为深度格式
     * 
     * 深度纹理用于存储场景中每个像素的深度值，是3D渲染中实现深度测试的关键。
     * 通过检查像素格式来确定是否为深度格式。
     * 
     * @return true 如果是深度格式，false 否则
     */
    bool Texture::isDepth() const {
        return (format_ == VK_FORMAT_D16_UNORM ||           // 16位无符号归一化深度
                format_ == VK_FORMAT_D16_UNORM_S8_UINT ||   // 16位深度 + 8位模板
                format_ == VK_FORMAT_D24_UNORM_S8_UINT ||   // 24位深度 + 8位模板（常用）
                format_ == VK_FORMAT_D32_SFLOAT ||          // 32位浮点深度（高精度）
                format_ == VK_FORMAT_D32_SFLOAT_S8_UINT ||  // 32位浮点深度 + 8位模板
                format_ == VK_FORMAT_X8_D24_UNORM_PACK32);  // 打包的24位深度格式
    }

    /**
     * @brief 检查纹理是否包含模板分量
     * 
     * 模板缓冲区用于实现复杂的渲染效果，通过逐像素的条件测试控制渲染。
     * 常用于阴影体、反射效果等高级渲染技术。
     * 
     * @return true 如果包含模板分量，false 否则
     */
    bool Texture::isStencil() const {
        return (format_ == VK_FORMAT_S8_UINT ||             // 纯8位无符号整数模板
                format_ == VK_FORMAT_D16_UNORM_S8_UINT ||   // 16位深度 + 8位模板
                format_ == VK_FORMAT_D24_UNORM_S8_UINT ||   // 24位深度 + 8位模板
                format_ == VK_FORMAT_D32_SFLOAT_S8_UINT);   // 32位浮点深度 + 8位模板
    }

    /**
     * @brief 获取单个像素的字节大小
     * 
     * 调用工具函数计算当前像素格式下单个像素占用的字节数。
     * 这个信息用于计算内存占用、数据传输大小等。
     * 
     * @return uint32_t 单个像素的字节数
     */
    uint32_t Texture::pixelSizeInBytes() const { 
        return bytesPerPixel(format_); 
    }

    /**
     * @brief 获取Mipmap层级总数
     * 
     * 返回此纹理包含的Mipmap级别数量。
     * 
     * @return uint32_t Mipmap层级数（至少为1）
     */
    uint32_t Texture::numMipLevels() const { 
        return mipLevels_; 
    }

    /**
     * @brief 计算给定尺寸纹理的完整Mipmap链层级数
     * 
     * 使用标准公式：floor(log2(max(width, height))) + 1
     * 计算从原始尺寸到1x1像素所需的层级数。
     * 
     * ## 计算示例：
     * - 512x512: floor(log2(512)) + 1 = 9 + 1 = 10层
     * - 1024x256: floor(log2(1024)) + 1 = 10 + 1 = 11层
     * 
     * @param texWidth 纹理宽度（像素）
     * @param texHeight 纹理高度（像素）
     * @return uint32_t 完整Mipmap链的层级数
     */
    uint32_t Texture::getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) const {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    }

    /**
     * @brief 生成完整的Mipmap链
     * 
     * 这个函数从基础层级（层级0）自动生成所有较低分辨率的Mipmap层级。
     * 使用GPU的图像混合(blit)操作实现高效的下采样。
     * 
     * ## 生成过程：
     * 1. 检查GPU是否支持线性过滤混合
     * 2. 逐级生成：从层级i生成层级i+1
     * 3. 每次将分辨率减半（宽和高分别除以2）
     * 4. 使用线性过滤进行高质量下采样
     * 5. 处理必要的布局转换和内存屏障
     * 
     * ## 前提条件：
     * - 纹理必须在创建时启用generateMips选项
     * - 纹理格式必须支持VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
     * - 纹理必须包含VK_IMAGE_USAGE_TRANSFER_SRC_BIT和VK_IMAGE_USAGE_TRANSFER_DST_BIT使用标志
     * - 基础层级（层级0）必须包含有效数据
     * 
     * ## 工作原理：
     * Mipmap生成使用GPU的blit操作，从高分辨率层级复制并缩放到低分辨率层级。
     * 每个层级的尺寸都是上一级的一半，直到达到1x1像素。
     */
    void Texture::generateMips(VkCommandBuffer cmdBuffer) {
        // === 步骤1：检查是否启用Mipmap生成 ===
        if (!generateMips_) {
            return;  // 如果未启用，直接返回
        }
        
        // === 步骤2：开始调试标签 ===
        // context_.beginDebugUtilsLabel(cmdBuffer, "Generate Mips", {0.0f, 1.0f, 0.0f, 1.0f});
        
        // === 步骤3：检查格式支持 ===
        // 查询物理设备对当前格式的支持情况
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(context_.physicalDevice().vkPhysicalDevice(),
                                            format_, &formatProperties);

        // 验证格式是否支持线性过滤blit操作
        // 这是生成高质量Mipmap的必要条件
        ASSERT(formatProperties.optimalTilingFeatures &
               VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
               "Device doesn't support linear blit, can't generate mips");

        // === 步骤4：确定图像方面掩码 ===
        // 根据图像类型选择正确的方面掩码
        const VkImageAspectFlags aspectMask =
                isDepth() ? isStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT  // 深度+模板
                                        : VK_IMAGE_ASPECT_DEPTH_BIT                              // 仅深度
                          : VK_IMAGE_ASPECT_COLOR_BIT;                                           // 颜色

        // === 步骤5：创建Mipmap生成用的内存屏障 ===
        // 这个屏障用于在每个Mipmap层级生成过程中同步GPU操作
        VkImageMemoryBarrier barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,        // 结构体类型
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,          // 源访问：传输写入（上传完成）
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,           // 目标访问：传输读取（用作blit源）
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,      // 旧布局：传输目标（刚接收数据）
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,      // 新布局：传输源（用于blit操作）
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,         // 不涉及队列家族转移
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,         // 不涉及队列家族转移
                .image = image_,                                        // 要处理的图像
                .subresourceRange = {                                   // 子资源范围
                        .aspectMask = aspectMask,                       // 图像方面（颜色/深度/模板）
                        .baseMipLevel = 0,                              // 起始Mip层级（会动态更新）
                        .levelCount = 1,                                // 每次只处理1个层级
                        .baseArrayLayer = 0,                            // 基础数组层
                        .layerCount = 1,                                // 数组层数量
                }};

        // === 步骤6：初始化当前层级的尺寸 ===
        // 从原始纹理尺寸开始，每次生成下一层级时会减半
        int32_t mipWidth = extents_.width;      // 当前Mip层级的宽度
        int32_t mipHeight = extents_.height;    // 当前Mip层级的高度

        // === 步骤7：逐层生成Mipmap ===
        // 从层级1开始（层级0是原始数据），逐层生成到最高层级
        for (uint32_t i = 1; i <= mipLevels_; i++) {
            // === 子步骤7.1：设置当前要处理的层级 ===
            barrier.subresourceRange.baseMipLevel = i - 1;  // 处理第i-1层（从0开始）
            
            // === 子步骤7.2：将当前层级从DST转换为SRC布局 ===
            // 这样当前层级就可以作为blit操作的源，用于生成下一层级
            vkCmdPipelineBarrier(cmdBuffer, 
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,    // 源管线阶段：传输阶段
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,    // 目标管线阶段：传输阶段
                                 0,                                 // 依赖标志
                                 0, nullptr,                        // 内存屏障
                                 0, nullptr,                        // 缓冲区内存屏障
                                 1, &barrier);                      // 图像内存屏障

            // === 子步骤7.3：检查是否为最后一层 ===
            if (i == mipLevels_) {
                // 最后一层不需要生成下一层，只需要布局转换
                break;
            }

            // === 子步骤7.4：计算下一层级的尺寸 ===
            // 每个维度减半，但最小为1像素（使用位移运算提高效率）
            const int32_t newMipWidth = mipWidth > 1 ? mipWidth >> 1 : mipWidth;    // >>1 等价于 /2
            const int32_t newMipHeight = mipHeight > 1 ? mipHeight >> 1 : mipHeight;

            // === 子步骤7.5：配置图像Blit操作 ===
            // Blit是GPU的高效图像复制和缩放操作，支持硬件加速的过滤
            VkImageBlit blit{
                    // 源子资源：从当前层级读取数据
                    .srcSubresource =
                            {
                                    .aspectMask = aspectMask,       // 图像方面（颜色/深度/模板）
                                    .mipLevel = i - 1,              // 源Mip层级（当前层级）
                                    .baseArrayLayer = 0,            // 源数组层起始索引
                                    .layerCount = 1,                // 源数组层数量
                            },
                    // 源区域：定义源图像的采样区域
                    .srcOffsets = {{
                                           0,                       // 源区域左上角X坐标
                                           0,                       // 源区域左上角Y坐标
                                           0,                       // 源区域左上角Z坐标
                                   },
                                   {
                                           mipWidth,                // 源区域右下角X坐标（当前层级宽度）
                                           mipHeight,               // 源区域右下角Y坐标（当前层级高度）
                                           1,                       // 源区域右下角Z坐标（2D纹理深度为1）
                                   }},
                    // 目标子资源：写入到下一层级
                    .dstSubresource =
                            {
                                    .aspectMask = aspectMask,       // 图像方面（与源相同）
                                    .mipLevel = i,                  // 目标Mip层级（下一层级）
                                    .baseArrayLayer = 0,            // 目标数组层起始索引
                                    .layerCount = 1,                // 目标数组层数量
                            },
                    // 目标区域：定义目标图像的写入区域
                    .dstOffsets = {{
                                           0,                       // 目标区域左上角X坐标
                                           0,                       // 目标区域左上角Y坐标
                                           0,                       // 目标区域左上角Z坐标
                                   },
                                   {
                                           newMipWidth,             // 目标区域右下角X坐标（新层级宽度）
                                           newMipHeight,            // 目标区域右下角Y坐标（新层级高度）
                                           1,                       // 目标区域右下角Z坐标（2D纹理深度为1）
                                   }},
            };

            // === 子步骤7.6：执行Blit操作 ===
            // 从当前层级blit到下一层级，同时进行缩放和过滤
            vkCmdBlitImage(cmdBuffer, 
                           image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,    // 源图像和布局
                           image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,    // 目标图像和布局（同一个图像）
                           1, &blit,                                        // blit操作数量和描述
                           VK_FILTER_LINEAR);                               // 线性过滤（高质量缩放）

            // === 子步骤7.7：更新当前层级尺寸 ===
            // 为下一次循环准备新的尺寸值
            mipWidth = newMipWidth;     // 更新宽度为新层级的宽度
            mipHeight = newMipHeight;   // 更新高度为新层级的高度
        }  // 循环结束，所有Mipmap层级生成完成

        // === 步骤8：创建最终布局转换屏障 ===
        // 将所有Mipmap层级从传输源布局转换为着色器只读布局
        const VkImageMemoryBarrier finalBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,            // 结构体类型
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,              // 源访问：传输写入（blit操作完成）
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,                 // 目标访问：着色器读取（准备采样）
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,          // 旧布局：传输源（blit后的状态）
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,      // 新布局：着色器只读（最终状态）
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,             // 不涉及队列家族转移
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,             // 不涉及队列家族转移
                .image = image_,                                            // 要转换的图像
                .subresourceRange =                                         // 子资源范围
                        {
                                .aspectMask = aspectMask,                   // 图像方面（颜色/深度/模板）
                                .baseMipLevel = 0,                          // 从第0层开始
                                .levelCount = VK_REMAINING_MIP_LEVELS,      // 包含所有剩余层级
                                .baseArrayLayer = 0,                        // 从第0个数组层开始
                                .layerCount = 1,                            // 包含1个数组层
                        },
        };

        // === 步骤9：执行最终布局转换 ===
        // 这个屏障确保所有Mipmap层级都转换为着色器只读布局
        vkCmdPipelineBarrier(cmdBuffer, 
                             VK_PIPELINE_STAGE_TRANSFER_BIT,        // 源管线阶段：传输完成
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // 目标管线阶段：片段着色器使用前
                             0,                                     // 依赖标志
                             0, nullptr,                            // 内存屏障
                             0, nullptr,                            // 缓冲区内存屏障
                             1, &finalBarrier);                     // 图像内存屏障

        // === 步骤10：更新内部状态并结束调试标签 ===
        layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // 更新内部布局状态
        context_.endDebugUtilsLabel(cmdBuffer);              // 结束调试标签
    }

    /**
     * @brief 为每个Mipmap层级生成独立的图像视图
     * 
     * 这个函数创建一个视图数组，其中每个视图只包含一个特定的Mipmap层级。
     * 这对于需要访问特定分辨率层级的高级渲染技术很有用。
     * 
     * ## 使用场景：
     * - 渲染到特定的Mipmap层级（如实时反射）
     * - 在计算着色器中处理特定层级
     * - 实现自定义的Mipmap生成算法
     * - 多分辨率渲染技术
     * 
     * ## 返回值特点：
     * - 每个智能指针管理一个VkImageView
     * - 视图索引对应Mipmap层级索引
     * - 自动内存管理，无需手动释放
     * - 每个视图只包含单个Mipmap层级
     * 
     * @return std::vector<std::shared_ptr<VkImageView>> 
     *         包含每个Mipmap层级视图的智能指针数组
     */
    std::vector<std::shared_ptr<VkImageView>> Texture::generateViewForEachMips() {
        // === 步骤1：初始化输出数组 ===
        std::vector<std::shared_ptr<VkImageView>> output;
        output.reserve(mipLevels_);  // 预分配空间避免重复分配

        // === 步骤2：为每个Mipmap层级创建视图 ===
        for (int i = 0; i < mipLevels_; ++i) {
            // === 子步骤2.1：确定图像方面掩码 ===
            const VkImageAspectFlags aspectMask =
                    isDepth() ? (isStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT  // 深度+模板
                                             : VK_IMAGE_ASPECT_DEPTH_BIT)                              // 仅深度
                              : VK_IMAGE_ASPECT_COLOR_BIT;                                           // 颜色
            
            // === 子步骤2.2：配置ImageView创建信息 ===
            const VkImageViewCreateInfo imageViewInfo = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,  // 结构体类型
                    .image = image_,                                    // 源图像对象
                    .viewType = viewType_,                              // 视图类型（2D、立方体等）
                    .format = format_,                                  // 像素格式
                    .components =                                       // 颜色分量映射
                            {
                                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,     // 红色分量保持原样
                                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,     // 绿色分量保持原样
                                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,     // 蓝色分量保持原样
                                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,     // Alpha分量保持原样
                            },
                    .subresourceRange = {                               // 子资源范围
                            .aspectMask = aspectMask,                   // 图像方面
                            .baseMipLevel = uint32_t(i),                // 当前Mipmap层级
                            .levelCount = 1,                            // 只包含1个层级
                            .baseArrayLayer = 0,                        // 基础数组层
                            .layerCount = layerCount_,                  // 包含的数组层数
                    }};

            // === 子步骤2.3：创建智能指针并添加到数组 ===
            output.push_back(std::make_shared<VkImageView>(VK_NULL_HANDLE));

            // === 子步骤2.4：创建VkImageView对象 ===
            VK_CHECK(vkCreateImageView(context_.device(), &imageViewInfo, nullptr,
                                       output.back().get()));
            
            // === 子步骤2.5：设置调试名称 ===
            context_.setVkObjectname(*output.back(), VK_OBJECT_TYPE_IMAGE_VIEW, 
                                    "Image view per mip level " + std::to_string(i));
        }
        
        // === 步骤3：返回视图数组 ===
        return output;
    }

    /**
     * @brief 获取多重采样抗锯齿级别
     * 
     * 返回此纹理的MSAA采样级别。MSAA通过对每个像素进行多次采样
     * 来减少锯齿效果，提高渲染质量。
     * 
     * ## 常见采样级别：
     * - VK_SAMPLE_COUNT_1_BIT: 无MSAA（1x采样，默认）
     * - VK_SAMPLE_COUNT_2_BIT: 2x MSAA（轻量抗锯齿）
     * - VK_SAMPLE_COUNT_4_BIT: 4x MSAA（常用级别）
     * - VK_SAMPLE_COUNT_8_BIT: 8x MSAA（高质量）
     * - VK_SAMPLE_COUNT_16_BIT: 16x MSAA（最高质量，性能开销大）
     * 
     * ## 性能权衡：
     * - 优点：显著改善边缘质量，减少锯齿现象
     * - 缺点：增加内存使用和渲染开销
     * - 建议：移动设备通常使用较低级别（1x-4x）
     * 
     * @return VkSampleCountFlagBits 当前的MSAA采样级别
     */
    VkSampleCountFlagBits Texture::VkSampleCount() const { 
        return msaaSamples_; 
    }

    /**
     * @brief 创建图像视图的内部辅助函数
     * 
     * 这是一个私有辅助函数，用于统一创建各种类型的VkImageView。
     * 它封装了ImageView创建的复杂逻辑，确保所有视图都使用一致的配置。
     * 
     * ## 图像视图的作用：
     * ImageView定义了如何解释和访问VkImage中的数据：
     * - 指定访问的Mipmap层级范围
     * - 指定访问的数组层范围
     * - 定义颜色分量的重排列（swizzle）
     * - 选择图像的特定方面（颜色/深度/模板）
     * 
     * ## 视图类型说明：
     * - VK_IMAGE_VIEW_TYPE_1D: 一维纹理视图
     * - VK_IMAGE_VIEW_TYPE_2D: 二维纹理视图（最常用）
     * - VK_IMAGE_VIEW_TYPE_3D: 三维体积纹理视图
     * - VK_IMAGE_VIEW_TYPE_CUBE: 立方体贴图视图
     * - VK_IMAGE_VIEW_TYPE_2D_ARRAY: 二维纹理数组视图
     * 
     * @param context Vulkan上下文，提供设备信息
     * @param viewType 图像视图类型，决定着色器中的访问方式
     * @param format 像素格式，必须与图像格式兼容
     * @param numMipLevels 视图包含的Mipmap层级数
     * @param layers 视图包含的数组层数
     * @param name 调试名称，用于GPU调试工具识别
     * @return VkImageView 创建的图像视图句柄
     * 
     * @note 调用者负责管理返回的VkImageView的生命周期
     * @warning 此函数假设图像格式和视图类型兼容
     */
    VkImageView Texture::createImageView(const Context& context, VkImageViewType viewType,
                                         VkFormat format, uint32_t numMipLevels,
                                         uint32_t layers, const std::string& name) {
        // === 注释的断言检查 ===
        // 这个检查在某些情况下可能过于严格，因为深度模板格式可以同时包含两者
        // ASSERT(isDepth() ^ isStencil(),
        //        "It's illegal to create an image view with both the depth and stencil bits. You can only use one");

        // === 步骤1：确定图像方面掩码 ===
        // 根据纹理类型选择正确的方面掩码
        const VkImageAspectFlags aspectMask =
                isDepth() ? VK_IMAGE_ASPECT_DEPTH_BIT                      // 深度格式
                          : (isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT      // 模板格式
                                         : VK_IMAGE_ASPECT_COLOR_BIT);      // 颜色格式

        // === 步骤2：配置ImageView创建信息 ===
        const VkImageViewCreateInfo imageViewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,  // 结构体类型
                // 创建标志（注释的代码是片段密度图的高级功能）
                .flags = /*usageFlags_ & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT ?
                           VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT :*/
                         VkImageViewCreateFlags(0),                 // 无特殊标志
                .image = image_,                                    // 源图像对象
                .viewType = viewType,                               // 视图类型
                .format = format,                                   // 像素格式
                .components =                                       // 颜色分量重排列
                        {
                                .r = VK_COMPONENT_SWIZZLE_IDENTITY,     // 红色分量保持原样
                                .g = VK_COMPONENT_SWIZZLE_IDENTITY,     // 绿色分量保持原样
                                .b = VK_COMPONENT_SWIZZLE_IDENTITY,     // 蓝色分量保持原样
                                .a = VK_COMPONENT_SWIZZLE_IDENTITY,     // Alpha分量保持原样
                        },
                .subresourceRange = {                               // 子资源范围
                        .aspectMask = aspectMask,                   // 图像方面
                        .baseMipLevel = 0,                          // 起始Mipmap层级
                        .levelCount = numMipLevels,                 // Mipmap层级数量
                        .baseArrayLayer = 0,                        // 起始数组层
                        .layerCount = multiview_ ? VK_REMAINING_ARRAY_LAYERS : layers,  // 数组层数量
                }};

        // === 步骤3：创建VkImageView对象 ===
        VkImageView imageView{VK_NULL_HANDLE};  // 初始化为空句柄
        VK_CHECK(vkCreateImageView(context_.device(), &imageViewInfo, nullptr, &imageView));

        // === 步骤4：设置调试名称 ===
        context.setVkObjectname(imageView, VK_OBJECT_TYPE_IMAGE_VIEW, "Image view: " + name);

        // === 步骤5：返回创建的视图 ===
        return imageView;
    }

}  // namespace VkCore

/**
 * @page texture_implementation_guide Texture.cpp 实现指南
 * 
 * ## 文件总结
 * 
 * Texture.cpp 实现了完整的 Vulkan 纹理管理功能，为初学者提供了详细的注释和解释。
 * 本文件包含了从纹理创建到销毁的完整生命周期管理。
 * 
 * ## 核心功能实现
 * 
 * ### 1. 纹理创建
 * - **主构造函数**: 创建新的 VkImage 并分配 GPU 内存
 * - **包装构造函数**: 包装外部创建的 VkImage（如交换链图像）
 * - **自动内存管理**: 使用 VMA 简化内存分配和释放
 * - **调试支持**: 自动设置调试名称便于问题定位
 * 
 * ### 2. 数据上传
 * - **uploadOnly()**: 纯粹的数据上传，支持纹理数组层
 * - **uploadAndGenMips()**: 上传数据并自动生成 Mipmap 链
 * - **Staging Buffer**: 通过中间缓冲区实现 CPU 到 GPU 的数据传输
 * - **布局转换**: 自动处理上传前后的图像布局转换
 * 
 * ### 3. Mipmap 生成
 * - **generateMips()**: 使用 GPU blit 操作生成高质量 Mipmap
 * - **格式检查**: 验证 GPU 是否支持线性过滤 blit
 * - **逐级生成**: 从原始分辨率逐步缩小到 1x1 像素
 * - **自动优化**: 最终转换为着色器只读布局
 * 
 * ### 4. 布局转换
 * - **transitionImageLayout()**: 核心布局转换函数
 * - **自动推导**: 根据源布局和目标布局自动确定屏障参数
 * - **管线同步**: 正确设置管线阶段和访问掩码
 * - **多格式支持**: 支持颜色、深度、模板等各种格式
 * 
 * ### 5. 多队列支持
 * - **addReleaseBarrier()**: 释放资源所有权给其他队列家族
 * - **addAcquireBarrier()**: 从其他队列家族获取资源所有权
 * - **两阶段转移**: 确保跨队列资源访问的安全性
 * - **缓存一致性**: 处理不同队列家族间的缓存同步
 * 
 * ### 6. ImageView 管理
 * - **vkImageView()**: 按需创建和缓存特定 Mipmap 层级的视图
 * - **默认视图**: 包含所有层级的完整视图
 * - **特定视图**: 用于渲染到特定 Mipmap 层级
 * - **自动缓存**: 避免重复创建相同的视图
 * 
 * ### 7. 格式检测
 * - **isDepth()**: 检测深度格式，支持各种深度精度
 * - **isStencil()**: 检测模板格式，支持深度模板组合
 * - **pixelSizeInBytes()**: 计算像素大小，用于内存计算
 * - **格式兼容性**: 处理不同格式的特殊需求
 * 
 * ## 关键设计模式
 * 
 * ### RAII 资源管理
 * ```cpp
 * // 构造函数分配资源
 * Texture::Texture(...) {
 *     // 创建 VkImage
 *     // 分配 GPU 内存
 *     // 创建 ImageView
 * }
 * 
 * // 析构函数自动清理
 * Texture::~Texture() {
 *     // 销毁 ImageView
 *     // 释放 GPU 内存
 *     // 销毁 VkImage
 * }
 * ```
 * 
 * ### 移动语义
 * ```cpp
 * MOVABLE_ONLY(Texture);  // 只允许移动，禁止复制
 * ```
 * 
 * ### 按需创建
 * ```cpp
 * VkImageView vkImageView(uint32_t mipLevel) {
 *     if (cached) return cached_view;
 *     create_new_view();
 *     cache_and_return();
 * }
 * ```
 * 
 * ## 性能优化技巧
 * 
 * ### 1. 内存优化
 * - 使用 VMA 进行高效内存管理
 * - 根据使用模式选择合适的内存类型
 * - 避免不必要的内存分配和复制
 * 
 * ### 2. 布局优化
 * - 批量执行布局转换减少屏障数量
 * - 根据使用场景选择最优布局
 * - 避免不必要的布局转换
 * 
 * ### 3. Mipmap 优化
 * - 使用 GPU 硬件加速的 blit 操作
 * - 线性过滤提供高质量缩放
 * - 一次性生成完整 Mipmap 链
 * 
 * ### 4. 同步优化
 * - 精确的管线阶段和访问掩码设置
 * - 最小化同步开销
 * - 支持异步操作
 * 
 * ## 常见使用场景
 * 
 * ### 纹理加载
 * ```cpp
 * // 1. 创建纹理
 * Texture texture(context, VK_IMAGE_TYPE_2D, format, ...);
 * 
 * // 2. 上传数据
 * texture.uploadOnly(cmd, stagingBuffer, pixelData);
 * 
 * // 3. 转换布局
 * texture.transitionImageLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
 * ```
 * 
 * ### 渲染目标
 * ```cpp
 * // 1. 创建渲染目标纹理
 * Texture renderTarget(context, VK_IMAGE_TYPE_2D, format,
 *                      0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, ...);
 * 
 * // 2. 转换为渲染目标布局
 * renderTarget.transitionImageLayout(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
 * 
 * // 3. 使用作为渲染目标
 * // ... 渲染操作 ...
 * 
 * // 4. 转换为采样布局
 * renderTarget.transitionImageLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
 * ```
 * 
 * ### 立方体贴图
 * ```cpp
 * // 1. 创建立方体贴图
 * Texture cubemap(context, VK_IMAGE_TYPE_2D, format,
 *                 VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, ..., 6);  // 6个面
 * 
 * // 2. 为每个面上传数据
 * for (int face = 0; face < 6; ++face) {
 *     cubemap.uploadOnly(cmd, stagingBuffer, faceData[face], face);
 * }
 * 
 * // 3. 生成 Mipmap
 * cubemap.generateMips(cmd);
 * ```
 * 
 * ## 调试技巧
 * 
 * ### 1. 调试名称
 * 所有纹理都会自动设置调试名称，在 GPU 调试工具中易于识别。
 * 
 * ### 2. 断言检查
 * 关键参数都有断言检查，帮助发现编程错误。
 * 
 * ### 3. 布局跟踪
 * 内部状态始终与实际 GPU 状态同步，便于调试布局问题。
 * 
 * ## 扩展建议
 * 
 * ### 1. 压缩纹理支持
 * 可以扩展支持 DXT、ASTC 等压缩格式。
 * 
 * ### 2. 异步加载
 * 可以添加异步纹理加载支持。
 * 
 * ### 3. 纹理流送
 * 可以实现大纹理的流送加载。
 * 
 * ### 4. 内存池
 * 可以添加纹理内存池以提高分配效率。
 * 
 * @note 这个实现为 Vulkan 初学者提供了完整的纹理管理解决方案，
 *       涵盖了从基础概念到高级优化的所有方面。
 */