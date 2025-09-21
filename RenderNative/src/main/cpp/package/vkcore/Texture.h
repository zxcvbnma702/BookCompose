/**
 * @file Texture.h
 * @brief Vulkan纹理管理类 - 为初学者详细解释
 * @author nio
 * @date 2025/7/27
 * 
 * 本文件定义了Vulkan纹理(Texture)管理类，用于处理2D/3D图像数据。
 * 
 * ## 什么是纹理(Texture)？
 * 纹理是图形渲染中用来给3D模型表面贴图的2D或3D图像数据。
 * 在Vulkan中，纹理实际上是VkImage对象，存储在GPU内存中。
 * 
 * ## 核心概念解释：
 * 1. **VkImage**: Vulkan中的图像对象，存储像素数据
 * 2. **VkImageView**: 图像视图，定义如何访问VkImage的特定部分
 * 3. **Mipmap**: 多级渐远纹理，用于远距离渲染优化
 * 4. **Image Layout**: 图像布局，定义GPU如何组织像素数据
 * 5. **MSAA**: 多重采样抗锯齿，提高渲染质量
 * 6. **Staging Buffer**: 临时缓冲区，用于CPU到GPU的数据传输
 */

#ifndef BOOKCOMPOSE_TEXTURE_H
#define BOOKCOMPOSE_TEXTURE_H

#include <unordered_map>

#include "Common.h"
#include "Utils.h"
#include "vk_mem_alloc.h"  // Vulkan Memory Allocator - 简化内存管理

namespace VkCore {

    class Context;  // Vulkan上下文类，管理设备和队列
    class Buffer;   // 缓冲区类，用于数据传输

    /**
     * @class Texture
     * @brief Vulkan纹理管理类 - 封装VkImage和相关资源
     * 
     * 这个类负责管理Vulkan纹理的完整生命周期，包括：
     * - 创建和销毁VkImage对象
     * - 管理GPU内存分配
     * - 创建和管理ImageView
     * - 处理纹理布局转换
     * - 生成Mipmap层级
     * - 上传纹理数据
     * 
     * @note 使用final关键字防止继承，确保资源管理的安全性
     */
    class Texture final{
    public:
        /**
         * @brief 移动语义宏 - 禁用拷贝，只允许移动
         * 
         * 纹理对象管理昂贵的GPU资源，不应该被意外复制。
         * 只允许移动语义确保资源所有权的唯一性。
         */
        MOVABLE_ONLY(Texture);

        /**
         * @brief 主构造函数 - 创建新的Vulkan纹理
         * 
         * 这是最常用的构造函数，用于创建全新的纹理对象。
         * 它会分配GPU内存并创建相应的VkImage。
         * 
         * @param context Vulkan上下文，提供设备和内存分配器
         * @param type 图像类型：
         *   - VK_IMAGE_TYPE_1D: 一维纹理（如渐变）
         *   - VK_IMAGE_TYPE_2D: 二维纹理（最常用，如照片）
         *   - VK_IMAGE_TYPE_3D: 三维纹理（如体积纹理）
         * @param format 像素格式，定义每个像素的数据布局：
         *   - VK_FORMAT_R8G8B8A8_UNORM: 8位RGBA格式（最常用）
         *   - VK_FORMAT_R32G32B32A32_SFLOAT: 32位浮点RGBA
         *   - VK_FORMAT_D32_SFLOAT: 32位深度格式
         * @param flags 图像创建标志：
         *   - 0: 普通纹理
         *   - VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT: 立方体贴图
         * @param usageFlags 使用标志，告诉GPU如何使用这个纹理：
         *   - VK_IMAGE_USAGE_SAMPLED_BIT: 可以在着色器中采样
         *   - VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT: 可以作为渲染目标
         *   - VK_IMAGE_USAGE_TRANSFER_DST_BIT: 可以接收数据传输
         * @param extents 纹理尺寸（宽x高x深度）
         * @param numMipLevels Mipmap层级数：
         *   - 1: 只有原始尺寸
         *   - >1: 包含缩小版本，用于远距离渲染优化
         * @param layerCount 纹理层数：
         *   - 1: 普通纹理
         *   - 6: 立方体贴图（6个面）
         *   - >1: 纹理数组
         * @param memoryFlags 内存属性：
         *   - VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: GPU本地内存（最快）
         * @param generateMips 是否自动生成Mipmap
         * @param msaaSamples 多重采样级别：
         *   - VK_SAMPLE_COUNT_1_BIT: 无抗锯齿
         *   - VK_SAMPLE_COUNT_4_BIT: 4x抗锯齿
         * @param name 调试名称，便于调试时识别
         * @param multiview 是否支持多视图渲染（VR应用）
         * @param imageTiling 内存排列方式：
         *   - VK_IMAGE_TILING_OPTIMAL: GPU优化排列（推荐）
         *   - VK_IMAGE_TILING_LINEAR: 线性排列（CPU访问友好）
         * 
         * @note 这个构造函数会分配GPU内存，失败时会抛出异常
         */
        explicit Texture(const Context& context, VkImageType type, VkFormat format,
                         VkImageCreateFlags flags, VkImageUsageFlags usageFlags,
                         VkExtent3D extents, uint32_t numMipLevels,
                         uint32_t layerCount, VkMemoryPropertyFlags memoryFlags,
                         bool generateMips = false,
                         VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
                         const std::string& name = "", bool multiview = false,
                         VkImageTiling = VK_IMAGE_TILING_OPTIMAL);

        /**
         * @brief 包装构造函数 - 包装现有的VkImage
         * 
         * 用于包装已经存在的VkImage对象，比如交换链图像。
         * 这种情况下，我们不拥有VkImage的所有权，只是创建一个视图。
         * 
         * @param context Vulkan上下文
         * @param device Vulkan逻辑设备
         * @param image 现有的VkImage对象（如交换链图像）
         * @param format 图像的像素格式
         * @param extents 图像尺寸
         * @param numlayers 图像层数，默认为1
         * @param multiview 是否支持多视图
         * @param name 调试名称
         * 
         * @warning 使用此构造函数时，Texture对象不会销毁传入的VkImage
         * @note 主要用于交换链图像或其他外部创建的图像
         */
        explicit Texture(const Context& context, VkDevice device, VkImage image,
                         VkFormat format, VkExtent3D extents, uint32_t numlayers = 1,
                         bool multiview = false, const std::string& name = "");

        /**
         * @brief 析构函数 - 清理所有GPU资源
         * 
         * 自动释放以下资源：
         * - VkImageView对象
         * - VkImage对象（如果拥有所有权）
         * - GPU内存分配
         * 
         * @note 析构函数会确保所有GPU资源都被正确释放，防止内存泄漏
         */
        ~Texture();

        /**
         * @brief 获取纹理的像素格式
         * 
         * @return VkFormat 像素格式枚举值
         * @note [[nodiscard]]属性提醒调用者不要忽略返回值
         */
        [[nodiscard]] VkFormat vkFormat() const { return format_; }

        /**
         * @brief 获取指定Mipmap层级的图像视图
         *
         * 图像视图(ImageView)定义了如何访问VkImage的特定部分。
         * 这个函数可以返回整个纹理的视图，或者特定Mipmap层级的视图。
         * 
         * ## Mipmap层级说明：
         * - 层级0：原始尺寸（如512x512）
         * - 层级1：一半尺寸（如256x256）
         * - 层级2：四分之一尺寸（如128x128）
         * - 依此类推...
         *
         * @param mipLevel 要获取的Mipmap层级：
         *   - UINT32_MAX: 返回默认的完整图像视图（包含所有层级）
         *   - 0,1,2...: 返回特定层级的视图，用于渲染到特定层级
         * @return VkImageView 请求层级的Vulkan图像视图
         * 
         * @note 如果请求的层级不存在，函数会创建新的视图
         * @warning 返回的VkImageView由Texture对象管理，不要手动销毁
         */
        VkImageView vkImageView(uint32_t mipLevel = UINT32_MAX);

        /**
         * @brief 获取底层的VkImage对象
         * 
         * VkImage是Vulkan中实际存储像素数据的对象。
         * 通常用于绑定到描述符集或作为渲染目标。
         * 
         * @return VkImage 底层的Vulkan图像对象
         * @warning 不要直接销毁返回的VkImage，它由Texture对象管理
         */
        [[nodiscard]] VkImage vkImage() const { return image_; }

        /**
         * @brief 获取纹理的3D尺寸信息
         * 
         * VkExtent3D包含宽度、高度和深度信息：
         * - 2D纹理：depth = 1
         * - 3D纹理：depth > 1
         * 
         * @return VkExtent3D 包含width、height、depth的结构体
         */
        [[nodiscard]] VkExtent3D vkExtents() const { return extents_; }

        /**
         * @brief 获取当前的图像布局
         * 
         * 图像布局(Layout)定义了GPU如何组织像素数据：
         * - VK_IMAGE_LAYOUT_UNDEFINED: 未定义（初始状态）
         * - VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: 着色器只读优化
         * - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: 颜色附件优化
         * - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: 传输目标优化
         * 
         * @return VkImageLayout 当前的图像布局
         * @note 在使用纹理前，必须将其转换到合适的布局
         */
        [[nodiscard]] VkImageLayout vkLayout() const { return layout_; }

        /**
         * @brief 设置图像布局（仅更新内部状态）
         * 
         * 这个函数只更新Texture对象内部记录的布局状态，
         * 不会实际执行GPU上的布局转换操作。
         * 
         * @param layout 新的图像布局
         * @warning 这不会执行实际的布局转换！
         *          要执行真正的转换，请使用transitionImageLayout()
         * @note 通常在执行布局转换后调用此函数同步状态
         */
        void setImageLayout(VkImageLayout layout) { layout_ = layout; }

        /**
         * @brief 获取纹理在GPU内存中占用的字节数
         * 
         * 这个值包括所有Mipmap层级和数组层的内存占用。
         * 用于内存使用统计和调试。
         * 
         * @return VkDeviceSize GPU内存占用大小（字节）
         * @note VkDeviceSize是uint64_t的别名，可以表示大尺寸纹理
         */
        [[nodiscard]] VkDeviceSize vkDeviceSize() const { return deviceSize_; }

        /**
         * @brief 上传纹理数据并生成Mipmap
         * 
         * 这个函数执行两个关键操作：
         * 1. 将CPU内存中的纹理数据上传到GPU
         * 2. 自动生成所有Mipmap层级
         * 
         * ## 执行流程：
         * 1. 使用staging buffer将数据从CPU传输到GPU
         * 2. 转换图像布局为传输目标
         * 3. 执行数据拷贝
         * 4. 生成Mipmap链（每层都是上一层的一半尺寸）
         * 5. 转换最终布局为着色器只读
         * 
         * @param cmdBuffer 命令缓冲区，用于记录GPU命令
         * @param stagingBuffer 临时缓冲区，包含要上传的纹理数据
         * @param data CPU内存中的像素数据指针
         * 
         * @note 要求纹理创建时设置了generateMips_=true
         * @warning 确保stagingBuffer足够大以容纳完整的纹理数据
         * @todo 此函数还在开发中，可能会有API变化
         */
        void uploadAndGenMips(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                              void* data);

        /**
         * @brief 仅上传纹理数据（不生成Mipmap）
         * 
         * 这个函数只执行数据上传，不生成额外的Mipmap层级。
         * 适用于不需要Mipmap的纹理，或者手动管理Mipmap的情况。
         * 
         * ## 执行流程：
         * 1. 转换图像布局为传输目标
         * 2. 从staging buffer拷贝数据到指定层
         * 3. 转换布局为着色器只读
         * 
         * @param cmdBuffer 命令缓冲区，用于记录GPU命令
         * @param stagingBuffer 包含纹理数据的临时缓冲区
         * @param data CPU内存中的像素数据指针
         * @param layer 目标纹理层索引：
         *   - 0: 普通2D纹理
         *   - 0-5: 立方体贴图的6个面
         *   - 0-N: 纹理数组的不同层
         * 
         * @note 这个函数不会生成Mipmap，如需要请单独调用generateMips()
         */
        void uploadOnly(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
                        void* data, uint32_t layer = 0);

        /**
         * @brief 添加释放屏障 - 用于多队列间的资源所有权转移
         * 
         * 在多队列Vulkan应用中，当一个队列完成对纹理的使用后，
         * 需要"释放"所有权，以便其他队列可以"获取"并使用它。
         * 
         * ## 使用场景：
         * - 图形队列渲染完成后，传输给计算队列处理
         * - 计算队列处理完成后，传输回图形队列显示
         * 
         * @param cmdBuffer 当前队列的命令缓冲区
         * @param srcQueueFamilyIndex 源队列族索引（当前拥有者）
         * @param dstQueueFamilyIndex 目标队列族索引（将要获取的队列）
         * 
         * @note 必须与目标队列的addAcquireBarrier()配对使用
         * @warning 队列族索引必须有效，否则会导致未定义行为
         */
        void addReleaseBarrier(VkCommandBuffer cmdBuffer,
                               uint32_t srcQueueFamilyIndex,
                               uint32_t dstQueueFamilyIndex);

        /**
         * @brief 添加获取屏障 - 从其他队列获取纹理所有权
         *
         * 这个方法用于多队列Vulkan设置中，从源队列获取图像所有权到目标队列。
         * 它使用vkCmdPipelineBarrier2插入VkImageMemoryBarrier2。
         * 
         * ## 工作原理：
         * 1. 等待源队列的释放屏障完成
         * 2. 获取纹理的所有权
         * 3. 确保内存可见性和一致性
         * 4. 准备在新队列中使用纹理
         * 
         * ## 典型用法：
         * ```cpp
         * // 在图形队列中释放
         * texture.addReleaseBarrier(graphicsCmd, graphicsQueueFamily, computeQueueFamily);
         * 
         * // 提交图形队列命令...
         * 
         * // 在计算队列中获取
         * texture.addAcquireBarrier(computeCmd, graphicsQueueFamily, computeQueueFamily);
         * ```
         *
         * @param cmdBuffer 目标队列的命令缓冲区
         * @param srcQueueFamilyIndex 源队列族索引（之前的拥有者）
         * @param dstQueueFamilyIndex 目标队列族索引（新的拥有者）
         * 
         * @note 必须在源队列执行addReleaseBarrier()之后调用
         * @warning 队列同步失败可能导致数据竞争和渲染错误
         */
        void addAcquireBarrier(VkCommandBuffer cmdBuffer,
                               uint32_t srcQueueFamilyIndex,
                               uint32_t dstQueueFamilyIndex);

        /**
         * @brief 转换图像布局 - 优化GPU访问模式
         *
         * 图像布局转换是Vulkan中的核心概念。不同的操作需要不同的布局：
         * - 上传数据时需要TRANSFER_DST布局
         * - 着色器采样时需要SHADER_READ_ONLY布局  
         * - 渲染目标时需要COLOR_ATTACHMENT布局
         * 
         * 这个函数自动处理复杂的布局转换逻辑，包括：
         * - 确定正确的管线阶段
         * - 设置适当的访问掩码
         * - 插入内存屏障确保同步
         * 
         * ## 常见布局转换：
         * - UNDEFINED → TRANSFER_DST: 准备接收数据
         * - TRANSFER_DST → SHADER_READ_ONLY: 数据上传完成，准备采样
         * - SHADER_READ_ONLY → COLOR_ATTACHMENT: 准备作为渲染目标
         * 
         * @param cmdBuffer 用于记录转换命令的命令缓冲区
         * @param newLayout 目标图像布局
         * 
         * @note 函数会自动更新内部的layout_状态
         * @warning 不正确的布局转换可能导致渲染错误或性能下降
         * @see setImageLayout() 仅更新内部状态，不执行实际转换
         */
        void transitionImageLayout(VkCommandBuffer cmdBuffer,
                                   VkImageLayout newLayout);

        /**
         * @brief 检查纹理是否为深度格式
         * 
         * 深度纹理用于存储场景中每个像素的深度信息，
         * 是3D渲染中实现深度测试的关键组件。
         * 
         * ## 深度格式示例：
         * - VK_FORMAT_D32_SFLOAT: 32位浮点深度
         * - VK_FORMAT_D24_UNORM_S8_UINT: 24位深度+8位模板
         * - VK_FORMAT_D16_UNORM: 16位深度（移动设备常用）
         * 
         * @return bool true表示是深度格式，false表示不是
         * @note 深度纹理不能用作普通颜色纹理采样
         */
        [[nodiscard]] bool isDepth() const;

        /**
         * @brief 检查纹理是否包含模板分量
         * 
         * 模板缓冲区用于实现复杂的渲染效果，如：
         * - 阴影体渲染
         * - 反射/折射效果
         * - 复杂的遮罩操作
         * 
         * ## 包含模板的格式：
         * - VK_FORMAT_D24_UNORM_S8_UINT: 24位深度+8位模板
         * - VK_FORMAT_D32_SFLOAT_S8_UINT: 32位深度+8位模板
         * - VK_FORMAT_S8_UINT: 纯8位模板
         * 
         * @return bool true表示包含模板分量，false表示不包含
         * @note 模板测试可以实现像素级的精确控制
         */
        [[nodiscard]] bool isStencil() const;

        /**
         * @brief 获取单个像素的字节大小
         * 
         * 不同的像素格式占用不同的内存空间：
         * - R8G8B8A8: 4字节（每通道1字节）
         * - R32G32B32A32_SFLOAT: 16字节（每通道4字节浮点）
         * - R5G6B5: 2字节（压缩RGB格式）
         * 
         * 这个信息用于：
         * - 计算纹理总内存占用
         * - 设置数据上传的步长
         * - 验证缓冲区大小
         * 
         * @return uint32_t 单个像素占用的字节数
         * @note 对于压缩格式，返回值可能是块的平均字节数
         */
        [[nodiscard]] uint32_t pixelSizeInBytes() const;

        /**
         * @brief 获取Mipmap层级总数
         * 
         * Mipmap是纹理的多个分辨率版本：
         * - 层级0: 原始分辨率（如512x512）
         * - 层级1: 一半分辨率（如256x256）
         * - 层级2: 四分之一分辨率（如128x128）
         * - ...直到1x1
         * 
         * ## Mipmap的作用：
         * - 远距离渲染时使用低分辨率版本
         * - 减少纹理带宽和缓存miss
         * - 提高渲染性能和质量
         * 
         * @return uint32_t Mipmap层级总数
         * @note 层级数 = floor(log2(max(width, height))) + 1
         */
        [[nodiscard]] uint32_t numMipLevels() const;

        /**
         * @brief 生成Mipmap链
         * 
         * 自动生成纹理的所有Mipmap层级。每一层都是上一层的一半尺寸，
         * 通过双线性滤波生成，直到达到1x1像素。
         * 
         * ## 生成过程：
         * 1. 从层级0开始（原始纹理）
         * 2. 对每一层执行blit操作到下一层
         * 3. 使用GPU硬件滤波器进行缩放
         * 4. 重复直到最小尺寸（1x1）
         * 
         * ## 前提条件：
         * - 纹理格式必须支持blit操作
         * - 纹理必须有TRANSFER_SRC和TRANSFER_DST使用标志
         * - numMipLevels > 1
         * 
         * @param cmdBuffer 用于记录生成命令的命令缓冲区
         * 
         * @note 这个操作会修改所有Mipmap层级的布局
         * @warning 确保纹理格式支持线性滤波，否则操作会失败
         */
        void generateMips(VkCommandBuffer cmdBuffer);

        /**
         * @brief 为每个Mipmap层级创建独立的图像视图
         * 
         * 创建一个视图数组，每个视图对应一个特定的Mipmap层级。
         * 这些视图可以用于：
         * - 渲染到特定的Mipmap层级
         * - 在计算着色器中处理特定层级
         * - 实现自定义的Mipmap生成算法
         * 
         * ## 返回的视图特点：
         * - 每个视图只包含一个Mipmap层级
         * - 视图索引对应Mipmap层级
         * - 使用智能指针自动管理生命周期
         * 
         * @return std::vector<std::shared_ptr<VkImageView>> 
         *         包含每个层级视图的智能指针数组
         * 
         * @note 返回的视图由智能指针管理，会自动释放
         * @warning 不要在Texture对象销毁后使用返回的视图
         */
        std::vector<std::shared_ptr<VkImageView>> generateViewForEachMips();

        /**
         * @brief 获取多重采样抗锯齿级别
         * 
         * MSAA通过对每个像素进行多次采样来减少锯齿效果：
         * - VK_SAMPLE_COUNT_1_BIT: 无MSAA（1x采样）
         * - VK_SAMPLE_COUNT_2_BIT: 2x MSAA
         * - VK_SAMPLE_COUNT_4_BIT: 4x MSAA（常用）
         * - VK_SAMPLE_COUNT_8_BIT: 8x MSAA（高质量）
         * 
         * ## MSAA的权衡：
         * - 优点：显著改善边缘质量，减少锯齿
         * - 缺点：增加内存使用和渲染开销
         * 
         * @return VkSampleCountFlagBits 当前的采样级别
         * @note 移动设备通常支持较低的MSAA级别
         */
        VkSampleCountFlagBits VkSampleCount() const;

    private:
        /**
         * @brief 计算给定尺寸纹理所需的Mipmap层级数
         *
         * 使用公式：floor(log2(max(width, height))) + 1
         * 这个公式计算从完整尺寸到1x1所需的总层级数。
         * 
         * ## 计算示例：
         * - 512x512纹理：floor(log2(512)) + 1 = 9 + 1 = 10层
         *   (512→256→128→64→32→16→8→4→2→1)
         * - 1024x256纹理：floor(log2(1024)) + 1 = 10 + 1 = 11层
         *   (取较大的维度1024)
         * 
         * ## 为什么需要这个计算？
         * - 确保Mipmap链完整（直到1x1）
         * - 避免分配过多或过少的层级
         * - GPU硬件通常要求2的幂次尺寸
         *
         * @param texWidth  纹理宽度（像素）
         * @param texHeight 纹理高度（像素）
         * @return uint32_t 生成Mipmap时使用的层级数
         * 
         * @note 这是一个数学计算，不涉及GPU操作
         */
        uint32_t getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) const;

        /**
         * @brief 创建VkImageView对象
         * 
         * ImageView定义了如何解释和访问VkImage中的数据。
         * 不同的视图类型适用于不同的用途。
         * 
         * ## 视图类型说明：
         * - VK_IMAGE_VIEW_TYPE_2D: 标准2D纹理视图
         * - VK_IMAGE_VIEW_TYPE_CUBE: 立方体贴图视图（6个面）
         * - VK_IMAGE_VIEW_TYPE_2D_ARRAY: 2D纹理数组视图
         * - VK_IMAGE_VIEW_TYPE_3D: 3D体积纹理视图
         * 
         * ## 参数作用：
         * - viewType: 决定着色器中如何采样纹理
         * - format: 像素数据的解释方式
         * - numMipLevels: 视图包含的Mipmap层级数
         * - layers: 视图包含的数组层数
         * 
         * @param context Vulkan上下文，提供设备信息
         * @param viewType 图像视图类型
         * @param format 像素格式
         * @param numMipLevels 包含的Mipmap层级数
         * @param layers 包含的数组层数
         * @param name 调试名称（可选）
         * @return VkImageView 创建的图像视图
         * 
         * @note 这是一个内部辅助函数，用于统一视图创建逻辑
         * @warning 调用者负责管理返回的VkImageView的生命周期
         */
        VkImageView createImageView(const Context& context, VkImageViewType viewType,
                                    VkFormat format, uint32_t numMipLevels,
                                    uint32_t layers, const std::string& name = "");

    private:
        // === 核心Vulkan对象 ===
        
        /**
         * @brief Vulkan上下文引用
         * 
         * 提供访问Vulkan设备、队列、内存分配器等核心对象的接口。
         * 使用引用而非指针，确保Context在Texture生命周期内始终有效。
         */
        const Context& context_;
        
        /**
         * @brief VMA内存分配器
         * 
         * Vulkan Memory Allocator简化了GPU内存管理：
         * - 自动选择合适的内存堆
         * - 处理内存碎片整理
         * - 提供内存使用统计
         * 
         * @note nullptr表示使用默认分配器或外部管理的图像
         */
        VmaAllocator vmaAllocator_ = nullptr;
        
        /**
         * @brief VMA内存分配句柄
         * 
         * 代表这个纹理在GPU内存中的具体分配。
         * VMA使用这个句柄来跟踪和管理内存。
         * 
         * @note nullptr表示图像内存由外部管理（如交换链）
         */
        VmaAllocation vmaAllocation_ = nullptr;
        
        /**
         * @brief GPU内存占用大小（字节）
         * 
         * 包含所有Mipmap层级和数组层的总内存占用。
         * 用于内存预算管理和性能分析。
         */
        VkDeviceSize deviceSize_ = 0;
        
        // === 图像属性配置 ===
        
        /**
         * @brief 图像使用标志位组合
         * 
         * 告诉Vulkan这个纹理将如何被使用：
         * - VK_IMAGE_USAGE_SAMPLED_BIT: 着色器采样
         * - VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT: 渲染目标
         * - VK_IMAGE_USAGE_TRANSFER_DST_BIT: 数据传输目标
         * - VK_IMAGE_USAGE_TRANSFER_SRC_BIT: 数据传输源
         * 
         * GPU驱动根据这些标志优化内存布局和访问模式。
         */
        VkImageUsageFlags usageFlags_ = 0;
        
        /**
         * @brief 图像创建标志
         * 
         * 控制图像的特殊属性：
         * - 0: 普通纹理
         * - VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT: 立方体贴图兼容
         * - VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT: 2D数组兼容
         */
        VkImageCreateFlags flags_ = 0;
        
        /**
         * @brief 图像类型
         * 
         * 定义纹理的维度：
         * - VK_IMAGE_TYPE_1D: 一维纹理（如渐变条）
         * - VK_IMAGE_TYPE_2D: 二维纹理（最常用）
         * - VK_IMAGE_TYPE_3D: 三维体积纹理
         */
        VkImageType type_ = VK_IMAGE_TYPE_2D;
        
        /**
         * @brief Vulkan图像对象句柄
         * 
         * 实际存储像素数据的GPU资源。
         * VK_NULL_HANDLE表示未初始化或已销毁。
         */
        VkImage image_ = VK_NULL_HANDLE;
        
        /**
         * @brief 默认图像视图
         * 
         * 包含所有Mipmap层级和数组层的完整视图。
         * 这是最常用的视图，用于普通的纹理采样。
         */
        VkImageView imageView_ = VK_NULL_HANDLE;
        
        /**
         * @brief 特定Mipmap层级的图像视图缓存
         * 
         * 键：Mipmap层级索引
         * 值：对应层级的VkImageView
         * 
         * 用于渲染到特定Mipmap层级或在计算着色器中处理特定层级。
         * 按需创建，避免不必要的视图分配。
         */
        std::unordered_map<uint32_t, VkImageView> imageViewFramebuffers_;
        
        // === 像素格式和布局 ===
        
        /**
         * @brief 像素格式
         * 
         * 定义每个像素的数据布局和解释方式：
         * - VK_FORMAT_R8G8B8A8_UNORM: 8位RGBA（最常用）
         * - VK_FORMAT_R32G32B32A32_SFLOAT: 32位浮点RGBA
         * - VK_FORMAT_D32_SFLOAT: 32位深度
         * 
         * VK_FORMAT_UNDEFINED表示格式未确定或无效。
         */
        VkFormat format_ = VK_FORMAT_UNDEFINED;
        
        /**
         * @brief 图像3D尺寸
         * 
         * width: 纹理宽度（像素）
         * height: 纹理高度（像素）  
         * depth: 纹理深度（2D纹理为1，3D纹理>1）
         */
        VkExtent3D extents_;
        
        /**
         * @brief 当前图像布局
         * 
         * GPU如何组织像素数据以优化不同类型的访问：
         * - VK_IMAGE_LAYOUT_UNDEFINED: 未定义（初始状态）
         * - VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: 着色器只读优化
         * - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: 颜色附件优化
         * - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: 传输目标优化
         * 
         * 布局转换是昂贵的操作，需要仔细管理。
         */
        VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        
        // === 所有权和生命周期 ===
        
        /**
         * @brief 是否拥有VkImage的所有权
         * 
         * true: Texture对象负责销毁VkImage（普通纹理）
         * false: VkImage由外部管理，不应销毁（如交换链图像）
         */
        bool ownsVkImage_ = false;
        
        // === Mipmap和数组配置 ===
        
        /**
         * @brief Mipmap层级总数
         * 
         * 1: 只有原始分辨率，无Mipmap
         * >1: 包含多个分辨率层级，从原始尺寸到1x1
         * 
         * 层级数通常为 floor(log2(max(width, height))) + 1
         */
        uint32_t mipLevels_ = 1;
        
        /**
         * @brief 纹理数组层数
         * 
         * 1: 普通单层纹理
         * 6: 立方体贴图（6个面）
         * >1: 纹理数组，可以在着色器中索引不同层
         */
        uint32_t layerCount_ = 1;
        
        /**
         * @brief 是否支持多视图渲染
         * 
         * 多视图渲染允许单次绘制调用渲染到多个视图，
         * 主要用于VR应用中的双眼渲染优化。
         */
        bool multiview_ = false;
        
        /**
         * @brief 是否自动生成Mipmap
         * 
         * true: 上传数据后自动生成所有Mipmap层级
         * false: 手动管理Mipmap或不使用Mipmap
         */
        bool generateMips_ = false;
        
        /**
         * @brief 图像视图类型
         * 
         * 决定着色器中如何访问纹理：
         * - VK_IMAGE_VIEW_TYPE_2D: texture2D采样
         * - VK_IMAGE_VIEW_TYPE_CUBE: textureCube采样
         * - VK_IMAGE_VIEW_TYPE_2D_ARRAY: texture2DArray采样
         */
        VkImageViewType viewType_;
        
        // === 抗锯齿和内存排列 ===
        
        /**
         * @brief 多重采样抗锯齿级别
         * 
         * 每个像素的采样点数量：
         * - VK_SAMPLE_COUNT_1_BIT: 无抗锯齿（1个采样点）
         * - VK_SAMPLE_COUNT_4_BIT: 4x MSAA（4个采样点）
         * - VK_SAMPLE_COUNT_8_BIT: 8x MSAA（8个采样点）
         * 
         * 更高的采样数提供更好的抗锯齿效果，但消耗更多内存和性能。
         */
        VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;
        
        /**
         * @brief 图像内存排列方式
         * 
         * VK_IMAGE_TILING_OPTIMAL: GPU优化排列（推荐）
         *   - 最佳GPU性能
         *   - CPU无法直接访问
         *   - 内存布局由驱动优化
         * 
         * VK_IMAGE_TILING_LINEAR: 线性排列
         *   - CPU可以直接访问
         *   - GPU性能较低
         *   - 内存按行连续排列
         */
        VkImageTiling imageTiling_ = VK_IMAGE_TILING_OPTIMAL;
        
        /**
         * @brief 调试名称
         * 
         * 用于调试工具中识别这个纹理对象。
         * 在发布版本中可以为空以节省内存。
         */
        std::string debugName_;
    };

} // VkCore

#endif //BOOKCOMPOSE_TEXTURE_H

/**
 * @page texture_usage_guide Texture类使用指南
 * 
 * ## 基础概念
 * 
 * ### 什么是纹理？
 * 纹理是存储在GPU内存中的图像数据，用于给3D模型表面贴图。在Vulkan中，
 * 纹理实际上是VkImage对象，配合VkImageView来定义如何访问图像数据。
 * 
 * ### 核心组件
 * - **VkImage**: 实际的图像数据存储
 * - **VkImageView**: 定义如何解释和访问图像数据
 * - **VkImageLayout**: 定义GPU如何组织像素数据以优化访问
 * - **Mipmap**: 多级渐远纹理，用于远距离渲染优化
 * 
 * ## 典型使用流程
 * 
 * ### 1. 创建纹理
 * ```cpp
 * // 创建一个512x512的RGBA纹理，支持采样和数据传输
 * Texture texture(context, 
 *                 VK_IMAGE_TYPE_2D,                    // 2D纹理
 *                 VK_FORMAT_R8G8B8A8_UNORM,           // RGBA格式
 *                 0,                                   // 无特殊标志
 *                 VK_IMAGE_USAGE_SAMPLED_BIT |         // 可采样
 *                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,     // 可接收数据
 *                 {512, 512, 1},                       // 512x512尺寸
 *                 1,                                   // 1个Mipmap层级
 *                 1,                                   // 1个数组层
 *                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU本地内存
 *                 false,                               // 不自动生成Mipmap
 *                 VK_SAMPLE_COUNT_1_BIT,              // 无抗锯齿
 *                 "MyTexture");                        // 调试名称
 * ```
 * 
 * ### 2. 上传纹理数据
 * ```cpp
 * // 假设已有像素数据和staging buffer
 * VkCommandBuffer cmd = commandQueueManager.getCmdBufferToBegin();
 * 
 * // 上传数据到纹理
 * texture.uploadOnly(cmd, stagingBuffer, pixelData);
 * 
 * commandQueueManager.submit(cmd);
 * commandQueueManager.goToNextCmdBuffer();
 * ```
 * 
 * ### 3. 布局转换
 * ```cpp
 * VkCommandBuffer cmd = commandQueueManager.getCmdBufferToBegin();
 * 
 * // 转换布局以便着色器采样
 * texture.transitionImageLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
 * 
 * commandQueueManager.submit(cmd);
 * ```
 * 
 * ### 4. 在着色器中使用
 * ```cpp
 * // 获取图像视图用于描述符集绑定
 * VkImageView imageView = texture.vkImageView();
 * 
 * // 绑定到描述符集...
 * // 在着色器中使用texture2D()采样
 * ```
 * 
 * ## 高级功能
 * 
 * ### Mipmap生成
 * ```cpp
 * // 创建支持Mipmap的纹理
 * Texture texture(context, VK_IMAGE_TYPE_2D, format, 0,
 *                 VK_IMAGE_USAGE_SAMPLED_BIT | 
 *                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
 *                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
 *                 {512, 512, 1}, 
 *                 texture.getMipLevelsCount(512, 512), // 自动计算层级数
 *                 1, memoryFlags, true); // generateMips = true
 * 
 * // 上传数据并生成Mipmap
 * texture.uploadAndGenMips(cmd, stagingBuffer, pixelData);
 * ```
 * 
 * ### 立方体贴图
 * ```cpp
 * // 创建立方体贴图（6个面）
 * Texture cubemap(context, VK_IMAGE_TYPE_2D, format,
 *                 VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, // 立方体兼容
 *                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
 *                 {512, 512, 1}, 1, 6, // 6个层（6个面）
 *                 memoryFlags);
 * 
 * // 为每个面上传数据
 * for (int face = 0; face < 6; ++face) {
 *     cubemap.uploadOnly(cmd, stagingBuffer, faceData[face], face);
 * }
 * ```
 * 
 * ### 多队列资源共享
 * ```cpp
 * // 在图形队列中释放纹理所有权
 * texture.addReleaseBarrier(graphicsCmd, graphicsQueueFamily, computeQueueFamily);
 * // 提交图形队列命令...
 * 
 * // 在计算队列中获取纹理所有权
 * texture.addAcquireBarrier(computeCmd, graphicsQueueFamily, computeQueueFamily);
 * // 现在可以在计算着色器中使用纹理
 * ```
 * 
 * ## 性能优化建议
 * 
 * ### 1. 内存类型选择
 * - **设备本地内存**: 最快的GPU访问，用于频繁采样的纹理
 * - **主机可见内存**: CPU可直接访问，用于动态更新的纹理
 * 
 * ### 2. 格式选择
 * - **压缩格式**: 节省内存和带宽（DXT、ASTC等）
 * - **浮点格式**: 用于HDR渲染和计算
 * - **整数格式**: 用于普通颜色纹理
 * 
 * ### 3. Mipmap策略
 * - **自动生成**: 方便但质量一般
 * - **预生成**: 更好的质量和性能
 * - **运行时生成**: 适用于动态内容
 * 
 * ### 4. 布局管理
 * - 批量转换布局以减少屏障数量
 * - 在适当的管线阶段执行转换
 * - 避免不必要的布局转换
 * 
 * ## 常见错误和解决方案
 * 
 * ### 1. 验证层错误
 * ```
 * 错误：Image layout mismatch
 * 解决：确保在使用前转换到正确的布局
 * ```
 * 
 * ### 2. 内存不足
 * ```
 * 错误：VK_ERROR_OUT_OF_DEVICE_MEMORY
 * 解决：使用压缩格式、减少纹理尺寸、释放未使用的纹理
 * ```
 * 
 * ### 3. 格式不支持
 * ```
 * 错误：VK_ERROR_FORMAT_NOT_SUPPORTED
 * 解决：查询设备支持的格式，使用备选格式
 * ```
 * 
 * ### 4. 同步问题
 * ```
 * 错误：渲染结果错误或崩溃
 * 解决：正确使用屏障和栅栏确保CPU-GPU同步
 * ```
 * 
 * @note 这个指南提供了Texture类的基本使用方法，实际应用中还需要
 *       根据具体需求调整参数和优化策略。
 */
