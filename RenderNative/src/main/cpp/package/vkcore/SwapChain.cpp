//
// Created by nio on 2025/10/8.
//

#include "SwapChain.h"

#include <algorithm>  // std::clamp 等算法函数
#include <array>      // std::array 容器
#include <set>        // std::set 容器

#include "Context.h"      // Vulkan上下文
#include "Framebuffer.h"  // 帧缓冲区
#include "PhysicalDevice.h" // 物理设备查询
#include "Texture.h"      // 纹理/图像封装

namespace VkCore {

    // ========================================

    // 交换链构造函数实现
    // ========================================
    
    /**
     * 交换链构造函数的实现
     * 
     * 这个构造函数是整个交换链系统的核心，它负责：
     * 1. 查询并配置交换链参数
     * 2. 创建Vulkan交换链对象
     * 3. 创建交换链中的所有图像缓冲区
     * 4. 设置同步机制（信号量和栅栏）
     */
    Swapchain::Swapchain(const Context& context, const PhysicalDevice& physicalDevice,
                         VkSurfaceKHR surface, VkQueue presentQueue, VkFormat imageFormat,
                         VkColorSpaceKHR imageClorSpace, VkPresentModeKHR presentMode,
                         VkExtent2D extent, const std::string& name)
            : device_{context.device()},     // 保存Vulkan设备句柄
              presentQueue_{presentQueue},  // 保存呈现队列
              extent_{extent} {             // 保存图像分辨率
        // ========================================
        // 第一步：确定交换链图像数量
        // ========================================
        
        // 计算最优的图像数量
        // 原理：在最小数量基础上+1，但不超过最大数量
        // 优点：一个额外的缓冲区可以提高性能，减少等待时间
        const uint32_t numImages = std::clamp(
            physicalDevice.surfaceCapabilities().minImageCount + 1,  // 期望值：最小数量+1
            physicalDevice.surfaceCapabilities().minImageCount,     // 下限：硬件最小支持数量
            physicalDevice.surfaceCapabilities().maxImageCount      // 上限：硬件最大支持数量
        );
        
        // 常见情况：
        // - 双缓冲：minImageCount=2, 我们要求numImages=3（三缓冲）
        // - 但如果maxImageCount=2，则只能使用双缓冲

        // ========================================
        // 第二步：检查和配置队列家族
        // ========================================
        
        // 获取呈现队列家族索引
        const auto presentationFamilyIndex = physicalDevice.presentationFamilyIndex();
        ASSERT(presentationFamilyIndex.has_value(),
               "There are no presentation queues available for the swapchain");

        // 检查图形队列和呈现队列是否在同一个家族
        // 这影响图像的共享模式：
        // - 同一家族：可以使用EXCLUSIVE模式（更高效）
        // - 不同家族：必须使用CONCURRENT模式（需要额外同步）
        const bool presentationQueueIsShared =
                physicalDevice.graphicsFamilyIndex().value() == presentationFamilyIndex.value();

        // 准备队列家族索引数组（用于不同家族的情况）
        std::array<uint32_t, 2> familyIndices{
            physicalDevice.graphicsFamilyIndex().value(),      // 图形队列家族索引
            presentationFamilyIndex.value()                    // 呈现队列家族索引
        };
        
        // ========================================
        // 第三步：创建交换链配置结构体
        // ========================================
        
        const VkSwapchainCreateInfoKHR swapchainInfo = {
            // 结构体基本信息
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,  // 结构体类型标识
            
            // 表面和图像基本参数
            .surface = surface,                    // 操作系统窗口表面
            .minImageCount = numImages,            // 交换链中的图像数量
            .imageFormat = imageFormat,            // 图像像素格式（如BGRA8）
            .imageColorSpace = imageClorSpace,     // 颜色空间（如sRGB）
            .imageExtent = extent,                 // 图像分辨率（宽度x高度）
            .imageArrayLayers = 1,                 // 图像层数（1=普通2D图像）
            
            // 图像用途标志：指定图像的使用方式
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |    // 作为颜色附件（渲染目标）
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,      // 作为传输目标（可以复制数据到此图像）
            
            // 图像共享模式：决定不同队列家族如何访问图像
            .imageSharingMode = presentationQueueIsShared 
                              ? VK_SHARING_MODE_EXCLUSIVE     // 独占模式：性能更好，但只能一个队列访问
                              : VK_SHARING_MODE_CONCURRENT,   // 并发模式：多个队列可同时访问
            
            // 队列家族配置：仅在CONCURRENT模式下需要
            .queueFamilyIndexCount = presentationQueueIsShared ? 0u : 2u,                    // 队列家族数量
            .pQueueFamilyIndices = presentationQueueIsShared ? nullptr : familyIndices.data(), // 队列家族索引数组
            
            // 图像变换和合成设置
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,  // 不对图像进行变换（旋转、翻转等）
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,   // 不使用alpha混合（不透明）
            
            // 呈现模式和裁剪设置
            .presentMode = presentMode,            // 呈现模式（垂直同步、立即模式等）
            .clipped = VK_TRUE,                   // 允许裁剪被遮挡的像素（优化性能）
            .oldSwapchain = VK_NULL_HANDLE,       // 旧交换链（用于重建，这里是新建）
        };
        // ========================================
        // 第四步：创建Vulkan交换链对象
        // ========================================
        
        // 调用Vulkan API创建交换链
        VK_CHECK(vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &swapchain_));
        
        // 设置调试名称，便于在GPU调试工具中识别
        context.setVkObjectname(swapchain_, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "Swapchain: " + name);

        // ========================================
        // 第五步：创建交换链相关资源
        // ========================================
        
        // 从交换链获取图像并封装成Texture对象
        createTextures(context, imageFormat, extent);
        imageFormat_ = imageFormat;  // 保存格式供后续查询

        // 创建用于同步的信号量
        createSemaphores(context);

        // 创建用于CPU-GPU同步的栅栏
        // 初始状态为“已信号”，表示第一次调用acquireImage时不需要等待
        const VkFenceCreateInfo fenceci = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,  // 初始为信号状态
        };
        VK_CHECK(vkCreateFence(device_, &fenceci, nullptr, &acquireFence_));
    }

    // ========================================
    // 交换链析构函数
    // ========================================
    
    /**
     * 交换链析构函数 - 清理所有资源
     * 
     * 正确的清理顺序非常重要：
     * 1. 等待所有GPU操作完成
     * 2. 按照与Vulkan对象创建相反的顺序销毁
     * 3. 避免在GPU仍在使用时销毁资源
     */
    Swapchain::~Swapchain() {
        // 等待所有在飞行中的GPU操作完成
        // 这确保没有GPU仍在访问我们即将销毁的资源
        VK_CHECK(vkWaitForFences(device_, 1, &acquireFence_, VK_TRUE, UINT64_MAX));
        
        // 按照与创建相反的顺序销毁资源
        vkDestroyFence(device_, acquireFence_, nullptr);          // 销毁获取栅栏
        vkDestroySemaphore(device_, imageRendered_, nullptr);     // 销毁渲染完成信号量
        vkDestroySemaphore(device_, imageAvailable_, nullptr);    // 销毁图像可用信号量
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);     // 销毁交换链本身
        
        // 注意：images_中的Texture对象会在shared_ptr析构时自动清理
        // 交换链图像不需要手动销毁，它们是交换链的一部分
    }

    // ========================================
    // 图像获取和呈现操作
    // ========================================
    
    /**
     * 获取下一个可用于渲柕的图像
     * 
     * 这是渲柕循环的起始点。每一帧都必须先调用此函数获取一个
     * 可用的图像缓冲区，然后在其中进行渲柕。
     * 
     * 同步机制说明：
     * 1. vkWaitForFences: 等待上一次acquireImage操作完成
     * 2. vkResetFences: 重置栅栏为未信号状态
     * 3. vkAcquireNextImageKHR: 从交换链获取下一个图像
     *    - 成功时会触发imageAvailable_信号量
     *    - 也会触发acquireFence_栅栏
     */
    std::shared_ptr<Texture> Swapchain::acquireImage() {
        // 性能分析标记（在Debug版本中可用于性能追踪）
        // ZoneScopedN("Swapchain: acquireImage");

        // 等待上一次获取操作完成（CPU-GPU同步）
        VK_CHECK(vkWaitForFences(device_, 1, &acquireFence_, VK_TRUE, UINT64_MAX));
        
        // 重置栅栏为未信号状态，准备下一次使用
        VK_CHECK(vkResetFences(device_, 1, &acquireFence_));

        // 从交换链获取下一个可用的图像索引
        // 参数说明：
        // - device_: Vulkan设备
        // - swapchain_: 交换链对象
        // - UINT64_MAX: 无限等待时间（阻塞到有图像可用）
        // - imageAvailable_: 获取成功后触发的信号量
        // - acquireFence_: 获取成功后触发的栅栏
        // - &imageIndex_: 输出参数，获取到的图像索引
        VK_CHECK(vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_,
                                       acquireFence_, &imageIndex_));
        
        // 返回获取到的图像对象，供渲柕使用
        return images_[imageIndex_];
    }

    /**
     * 创建GPU命令提交信息
     * 
     * 这个函数创建的VkSubmitInfo结构体包含了完整的同步链：
     * 
     * 同步流程：
     * acquireImage() → imageAvailable_信号 → GPU渲柕 → imageRendered_信号 → present()
     * 
     * 这确保了：
     * 1. GPU只有在图像可用后才开始渲柕
     * 2. present()只有在渲柕完成后才显示图像
     * 3. 不同的GPU队列之间正确同步
     */
    VkSubmitInfo Swapchain::createSubmitInfo(const VkCommandBuffer* buffer,
                                             const VkPipelineStageFlags* submitStageMask,
                                             bool waitForImageAvailable,
                                             bool signalImagePresented) const {
        const VkSubmitInfo si = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            
            // 等待的信号量配置
            .waitSemaphoreCount = waitForImageAvailable 
                                ? (imageAvailable_ ? 1u : 0)  // 如果需要等待且信号量有效
                                : 0,                          // 不等待任何信号量
            .pWaitSemaphores = waitForImageAvailable ? &imageAvailable_ : VK_NULL_HANDLE,
            .pWaitDstStageMask = submitStageMask,  // 指定在渲柕管线的哪个阶段等待信号量
            
            // 要执行的命令缓冲区
            .commandBufferCount = 1,        // 只提交一个命令缓冲区
            .pCommandBuffers = buffer,      // 指向命令缓冲区的指针
            
            // 完成后要触发的信号量配置
            .signalSemaphoreCount = signalImagePresented 
                                  ? (imageRendered_ ? 1u : 0)  // 如果需要发信号且信号量有效
                                  : 0,                         // 不发任何信号
            .pSignalSemaphores = signalImagePresented ? &imageRendered_ : VK_NULL_HANDLE,
        };
        return si;
    }

    /**
     * 将渲柕完成的图像呈现到显示器
     * 
     * 这是渲柕循环的最后一步。此函数将当前渲柕完成的图像
     * 提交给显示系统，根据设置的呈现模式决定显示时机。
     * 
     * 呈现模式影响：
     * - IMMEDIATE: 立即显示，可能导致撕裂
     * - FIFO: 等待垂直同步，防止撕裂但可能增加延迟
     * - MAILBOX: 三缓冲模式，平衡性能和质量
     */
    void Swapchain::present() const {
        // 性能分析标记
        // ZoneScopedN("Swapchain: present");
        
        // 创建呈现信息结构体
        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,  // 结构体类型
            
            // 等待渲柕完成信号量
            .waitSemaphoreCount = 1,               // 等待一个信号量
            .pWaitSemaphores = &imageRendered_,    // 等待GPU渲柕完成信号
            
            // 要呈现的交换链和图像
            .swapchainCount = 1,                  // 呈现一个交换链
            .pSwapchains = &swapchain_,           // 指向交换链对象
            .pImageIndices = &imageIndex_,        // 指向要呈现的图像索引
        };
        
        // 提交呈现命令到呈现队列
        // 此函数可能会阻塞，直到显示系统准备好接受新图像
        VK_CHECK(vkQueuePresentKHR(presentQueue_, &presentInfo));
    }

    // ========================================
    // 私有辅助函数实现
    // ========================================
    
    /**
     * 创建交换链的纹理对象
     * 
     * 交换链创建后，需要从中获取实际的图像对象。这些图像由Vulkan驱动
     * 管理，我们只需要获取它们的句柄并封装成易用的Texture对象。
     * 
     * 注意：交换链图像不能直接创建或销毁，它们的生命周期由交换链管理。
     */
    void Swapchain::createTextures(const Context& context, VkFormat imageFormat,
                                   const VkExtent2D& extent) {
        // 第一次查询：获取图像数量
        uint32_t imageCount{0};
        VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr));
        
        // 创建容器存放图像句柄
        std::vector<VkImage> images(imageCount);
        
        // 第二次查询：获取实际的图像句柄
        VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images.data()));

        // 为纹理对象数组预留空间，避免重新分配
        images_.reserve(imageCount);
        
        // 将每个Vulkan图像封装成Texture对象
        for (size_t index = 0; index < imageCount; ++index) {
            images_.emplace_back(
                std::make_shared<Texture>(
                    context,                    // Vulkan上下文
                    device_,                   // Vulkan设备
                    images[index],             // 交换链图像句柄
                    imageFormat,               // 图像格式
                    VkExtent3D{                // 3D尺寸（交换链图像是2D的）
                        .width = extent.width,   // 宽度
                        .height = extent.height, // 高度
                        .depth = 1,             // 深度固定为1（2D图像）
                    },
                    1,     // 层数：1层（非数组纹理）
                    false, // 不拥有图像：由交换链管理，不需要我们销毁
                    "Swapchain image " + std::to_string(index)  // 调试名称
                )
            );
        }
    }

    /**
     * 创建同步用的信号量
     * 
     * 信号量是GPU之间的同步原语，用于协调不同的GPU操作。
     * 交换链需要两个信号量来实现正确的渲柕-呈现同步。
     * 
     * imageAvailable_: 图像可用信号量
     * - 在acquireImage()成功后触发
     * - 告诉GPU可以开始在该图像上渲柕
     * 
     * imageRendered_: 图像渲柕完成信号量
     * - 在GPU渲柕完成后触发
     * - 告诉present()可以安全地显示该图像
     */
    void Swapchain::createSemaphores(const Context& context) {
        // 创建信号量的标准配置（没有特殊参数）
        const VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,  // 结构体类型
        };
        
        // 创建图像可用信号量
        VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailable_));
        context.setVkObjectname(imageAvailable_, VK_OBJECT_TYPE_SEMAPHORE,
                                "Semaphore: swapchain image available semaphore");

        // 创建图像渲柕完成信号量
        VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageRendered_));
        context.setVkObjectname(imageRendered_, VK_OBJECT_TYPE_SEMAPHORE,
                                "Semaphore: swapchain image presented semaphore");
    }

}  // namespace VkCore

/*
 * ========================================
 * Vulkan交换链系统的总结和学习指南
 * ========================================
 * 
 * 一、交换链的核心概念
 * 
 * 1. 什么是交换链？
 *    交换链是现代图形程序的核心组件，解决了一个基本问题：
 *    如何在GPU渲柕的同时保持显示器显示稳定的画面？
 * 
 * 2. 没有交换链会怎样？
 *    - 画面撕裂：显示器在扫描时看到一半旧画面、一半新画面
 *    - 闪烁：渲柕速度与显示器刷新率不同步导致的闪烁
 *    - 性能问题：CPU/GPU需要等待显示器刷新完成
 * 
 * 3. 交换链的解决方案：
 *    使用多个图像缓冲区，让GPU在一个缓冲区中渲柕，
 *    同时显示器从另一个缓冲区显示完成的画面。
 * 
 * 二、常见的缓冲模式
 * 
 * 1. 双缓冲 (Double Buffering):
 *    - 前缓冲区 (Front Buffer): 显示器正在显示的图像
 *    - 后缓冲区 (Back Buffer): GPU正在绘制的图像
 *    - 渲柕完成后“交换”两个缓冲区的角色
 * 
 * 2. 三缓冲 (Triple Buffering):
 *    - 显示缓冲区: 显示器正在显示
 *    - 渲柕缓冲区: GPU正在绘制
 *    - 准备缓冲区: 已完成渲柕，等待显示
 *    - 优点：更低的延迟和更高的帧率
 * 
 * 三、呈现模式详解
 * 
 * 1. VK_PRESENT_MODE_IMMEDIATE_KHR (立即模式):
 *    - 描述：不等待垂直同步，立即显示新帧
 *    - 优点：最低延迟
 *    - 缺点：可能产生撕裂
 *    - 适用场景：竞技游戏、对延迟极度敏感的应用
 * 
 * 2. VK_PRESENT_MODE_FIFO_KHR (垂直同步模式):
 *    - 描述：等待显示器的垂直同步信号，然后显示新帧
 *    - 优点：完全消除撕裂，稳定的帧率
 *    - 缺点：可能增加延迟
 *    - 适用场景：大多数游戏和应用的默认选择
 * 
 * 3. VK_PRESENT_MODE_FIFO_RELAXED_KHR (宽松垂直同步):
 *    - 描述：大部分时间等待垂直同步，但如果已经错过了则立即显示
 *    - 优点：平衡了性能和质量
 *    - 缺点：偶尔可能有撕裂
 *    - 适用场景：可变帧率的应用
 * 
 * 4. VK_PRESENT_MODE_MAILBOX_KHR (邮箱模式):
 *    - 描述：三缓冲模式，新帧替换队列中的旧帧
 *    - 优点：低延迟 + 无撕裂，最优体验
 *    - 缺点：需要更多内存，不是所有设备都支持
 *    - 适用场景：高端游戏、专业图形应用
 * 
 * 四、同步机制详解
 * 
 * 1. 信号量 (Semaphore) - GPU之间的同步:
 *    - imageAvailable_: 图像可用信号量
 *      触发时机: acquireImage()成功后
 *      作用: 告诉GPU可以开始渲柕
 *    
 *    - imageRendered_: 渲柕完成信号量
 *      触发时机: GPU渲柕命令完成后
 *      作用: 告诉present()可以安全地显示图像
 * 
 * 2. 栅栏 (Fence) - CPU和GPU之间的同步:
 *    - acquireFence_: 图像获取栅栏
 *      触发时机: acquireImage()成功后
 *      作用: 告诉CPU图像已经可用，可以开始下一次获取
 * 
 * 3. 同步流程:
 *    acquireImage() → imageAvailable_ → GPU渲柕 → imageRendered_ → present()
 *                    → acquireFence_   →                →
 *    
 *    CPU等待栅栏             GPU等待信号量        present等待信号量
 * 
 * 五、常见问题和解决方案
 * 
 * 1. 问题：交换链失效 (Out of Date)
 *    原因：窗口大小改变、全屏切换等
 *    解决：重新创建交换链，更新图像尺寸和格式
 * 
 * 2. 问题：性能下降
 *    原因：不适合的呈现模式、过多的图像缓冲区
 *    解决：根据应用类型选择合适的呈现模式
 * 
 * 3. 问题：内存使用过多
 *    原因：创建了过多的交换链图像
 *    解决：使用最少需要的图像数量，通常是2-3个
 * 
 * 六、最佳实践
 * 
 * 1. 图像数量选择：
 *    - 最小数量 + 1：提供额外的缓冲，提高性能
 *    - 不超过最大数量：避免资源浪费
 *    - 考虑内存限制：高分辨率下少用图像
 * 
 * 2. 呈现模式选择：
 *    - 竞技游戏：IMMEDIATE 或 MAILBOX
 *    - 普通游戏：FIFO 或 MAILBOX
 *    - 省电应用：FIFO
 *    - 始终检查设备支持情况
 * 
 * 3. 同步管理：
 *    - 正确使用信号量和栅栏
 *    - 避免循环依赖和死锁
 *    - 在多线程环境中注意线程安全
 * 
 * 4. 错误处理：
 *    - 始终检查Vulkan返回值
 *    - 处理交换链失效情况
 *    - 实现优雅的降级机制
 * 
 * 七、实际应用中的使用模式
 * 
 * 典型的渲柕循环：
 * ```cpp
 * while (running) {
 *     // 1. 获取下一个可用图像
 *     auto image = swapchain.acquireImage();
 *     
 *     // 2. 在图像上渲柕
 *     recordRenderCommands(image);
 *     
 *     // 3. 提交渲柕命令
 *     auto submitInfo = swapchain.createSubmitInfo(...);
 *     vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr);
 *     
 *     // 4. 呈现到显示器
 *     swapchain.present();
 * }
 * ```
 * 
 * 这个Swapchain类封装了所有这些复杂性，为开发者提供了一个简单易用的接口。
 * 通过合理使用这个类，可以实现高质量、高性能的图形应用。
 */