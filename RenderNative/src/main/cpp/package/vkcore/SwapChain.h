//
// Created by nio on 2025/10/8.
//

#pragma once

#include <memory>
#include <vector>

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    class Context;
    class Framebuffer;
    class PhysicalDevice;
    class Texture;

    /**
     * Vulkan 交换链(SwapChain)封装类
     * 
     * 什么是交换链？
     * 交换链是现代图形API中的一个核心概念，它管理着用于显示的图像缓冲区。想象一下：
     * 
     * 问题：如果GPU直接在显示器正在显示的缓冲区中绘制，会发生什么？
     * 答案：画面撕裂！用户会看到一半是旧画面，一半是新画面。
     * 
     * 解决方案：交换链！
     * 交换链维护多个图像缓冲区（通常是2-3个），实现"双缓冲"或"三缓冲"：
     * 
     * 双缓冲模式：
     * - 前缓冲区：显示器正在显示的图像
     * - 后缓冲区：GPU正在绘制的图像
     * - 绘制完成后，"交换"两个缓冲区的角色
     * 
     * 三缓冲模式：
     * - 显示缓冲区：显示器正在显示
     * - 渲染缓冲区：GPU正在绘制
     * - 准备缓冲区：已完成渲染，等待显示
     * 
     * 交换链的主要功能：
     * 1. 管理多个图像缓冲区
     * 2. 协调GPU渲染和显示器刷新的时序
     * 3. 防止画面撕裂和闪烁
     * 4. 支持垂直同步(V-Sync)和各种显示模式
     * 
     * 工作流程：
     * 1. acquireImage() - 获取下一个可用的图像缓冲区
     * 2. GPU在该缓冲区中进行渲染
     * 3. present() - 将渲染完成的图像提交给显示器
     * 4. 重复上述过程
     * 
     * 同步机制：
     * - 使用信号量(Semaphore)协调渲染和显示的时序
     * - 使用栅栏(Fence)确保CPU和GPU的同步
     */
    class Swapchain final {
    public:
        /**
         * 默认构造函数 - 创建空的交换链对象
         */
        explicit Swapchain() = default;
        
        /**
         * 完整的交换链构造函数
         * 
         * @param context Vulkan上下文，提供设备等基础信息
         * @param physicalDevice 物理设备，用于查询显示能力
         * @param surface Vulkan表面，代表操作系统的窗口或显示区域
         * @param presentQueue 呈现队列，负责将图像提交给显示器
         * @param imageFormat 图像格式
         *   - VK_FORMAT_B8G8R8A8_SRGB: 常用的SRGB颜色空间格式
         *   - VK_FORMAT_R8G8B8A8_UNORM: 线性颜色空间格式  
         *   - 选择合适的格式影响颜色准确性和性能
         * @param imageClorSpace 颜色空间
         *   - VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: 标准SRGB颜色空间
         *   - 影响颜色的显示效果和色彩管理
         * @param presentMode 呈现模式，决定显示同步策略
         *   - VK_PRESENT_MODE_IMMEDIATE_KHR: 立即模式，可能导致撕裂但延迟最低
         *   - VK_PRESENT_MODE_FIFO_KHR: 垂直同步模式，等待显示器刷新，防止撕裂
         *   - VK_PRESENT_MODE_FIFO_RELAXED_KHR: 宽松垂直同步，允许偶尔撕裂换取更低延迟
         *   - VK_PRESENT_MODE_MAILBOX_KHR: 三缓冲模式，既防撕裂又保持低延迟
         * @param extent 交换链图像的分辨率（宽度x高度）
         * @param name 调试名称，便于在调试工具中识别
         */
        explicit Swapchain(const Context& context,
                           const PhysicalDevice& physicalDevice, VkSurfaceKHR surface,
                           VkQueue presentQueue, VkFormat imageFormat,
                           VkColorSpaceKHR imageClorSpace,
                           VkPresentModeKHR presentMode, VkExtent2D extent,
                           const std::string& name = "");

        /**
         * 析构函数 - 清理交换链资源
         * 
         * 确保所有GPU操作完成后再销毁资源，避免资源泄露
         */
        ~Swapchain();

        /**
         * 获取交换链中图像的数量
         * 
         * @return 图像缓冲区的数量（通常是2-3个）
         * 
         * 为什么需要多个图像？
         * - 2个图像：双缓冲，基本防撕裂
         * - 3个图像：三缓冲，更好的性能和更低的延迟
         */
        uint32_t numberImages() const {
            return static_cast<uint32_t>(images_.size());
        }

        /**
         * 获取当前图像的索引
         * 
         * @return 当前正在使用的图像在images_数组中的索引
         */
        size_t currentImageIndex() const { return imageIndex_; }

        /**
         * 获取下一个可用于渲染的图像
         * 
         * 这是渲染循环的第一步：
         * 1. 等待前一帧的GPU操作完成
         * 2. 从交换链中获取下一个可用的图像缓冲区
         * 3. 返回该图像的Texture对象供渲染使用
         * 
         * @return 可用于渲染的纹理对象
         * 
         * 注意：这个函数可能会阻塞，直到有图像可用
         */
        std::shared_ptr<Texture> acquireImage();

        /**
         * 获取交换链图像的格式
         * @return Vulkan图像格式枚举值
         */
        VkFormat imageFormat() const { return imageFormat_; }

        /**
         * 获取交换链图像的分辨率
         * @return 包含宽度和高度的结构体
         */
        VkExtent2D extent() const { return extent_; }

        /**
         * 将渲染完成的图像呈现到显示器
         * 
         * 这是渲染循环的最后一步：
         * 1. 等待GPU渲染完成
         * 2. 将当前图像提交给显示系统
         * 3. 根据呈现模式决定显示时机（立即显示或等待垂直同步）
         * 
         * 注意：调用此函数后，当前图像就不能再被修改了
         */
        void present() const;

        /**
         * 创建GPU命令提交信息
         * 
         * 在Vulkan中，GPU命令需要通过队列提交执行。这个函数创建提交信息，
         * 包含了同步机制，确保渲染和显示的正确时序。
         * 
         * @param buffer 要执行的命令缓冲区指针
         * @param submitStageMask 指定在哪个渲染管线阶段等待信号量
         *   - VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT: 在颜色附件输出阶段等待
         *   - 确保在开始写入颜色缓冲区之前，图像已经可用
         * @param waitForImageAvailable 是否等待图像可用信号量
         *   - true: 等待acquireImage()完成后再开始渲染
         *   - false: 立即开始执行命令（用于不依赖交换链的操作）
         * @param signalImagePresented 是否在渲染完成后发出信号
         *   - true: 渲染完成后通知present()可以显示图像
         *   - false: 不发出完成信号（用于中间处理步骤）
         * @return Vulkan提交信息结构体
         * 
         * 同步流程说明：
         * 1. acquireImage() 完成 → imageAvailable_信号量被触发
         * 2. GPU等待imageAvailable_，然后开始渲染
         * 3. 渲染完成 → imageRendered_信号量被触发  
         * 4. present()等待imageRendered_，然后显示图像
         */
        VkSubmitInfo createSubmitInfo(const VkCommandBuffer* buffer,
                                      const VkPipelineStageFlags* submitStageMask,
                                      bool waitForImageAvailable = true,
                                      bool signalImagePresented = true) const;

        /**
         * 根据索引获取指定的纹理对象
         * 
         * @param index 图像索引，必须小于numberImages()
         * @return 指定索引的纹理对象
         * 
         * 注意：通常不需要直接调用此函数，应该使用acquireImage()
         * 来获取当前可用的图像
         */
        std::shared_ptr<Texture> texture(uint32_t index) const {
            ASSERT(index < images_.size(),
                   "Index is greater than number of images in the swapchain");
            return images_[index];
        }

    private:
        /**
         * 私有辅助函数
         */
        
        /**
         * 创建交换链的纹理对象
         * 
         * 从Vulkan交换链获取原生图像，并将其封装成Texture对象
         * 便于统一的资源管理和使用
         * 
         * @param context Vulkan上下文
         * @param imageFormat 图像格式
         * @param extent 图像分辨率
         */
        void createTextures(const Context& context, VkFormat imageFormat,
                            const VkExtent2D& extent);

        /**
         * 创建同步用的信号量
         * 
         * 创建两个信号量：
         * - imageAvailable_: 图像可用信号量，acquireImage()完成时触发
         * - imageRendered_: 图像渲染完成信号量，GPU渲染结束时触发
         * 
         * @param context Vulkan上下文
         */
        void createSemaphores(const Context& context);

    private:
        /**
         * 私有成员变量
         */
        VkDevice device_ = VK_NULL_HANDLE;                      // Vulkan逻辑设备句柄
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;             // Vulkan交换链句柄
        VkQueue presentQueue_ = VK_NULL_HANDLE;                 // 负责图像呈现的队列
        std::vector<std::shared_ptr<Texture>> images_;         // 交换链中的所有图像缓冲区
        VkSemaphore imageAvailable_ = VK_NULL_HANDLE;           // 图像可用信号量（GPU同步）
        VkSemaphore imageRendered_ = VK_NULL_HANDLE;            // 图像渲染完成信号量（GPU同步）
        uint32_t imageIndex_ = 0;                               // 当前使用的图像索引
        VkExtent2D extent_;                                     // 交换链图像的分辨率
        VkFormat imageFormat_;                                  // 交换链图像的格式
        VkFence acquireFence_ = VK_NULL_HANDLE;                 // 图像获取栅栏（CPU-GPU同步）
    };

}  // namespace VkCore  // 注意：这里应该是VkCore，不是VulkanCore
