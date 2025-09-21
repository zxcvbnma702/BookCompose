//
// Created by nio on 2025/7/23.
//
// Vulkan 命令队列管理器
//
// Vulkan 命令系统基础概念：
// - Command Buffer（命令缓冲区）：类似于一个"任务清单"，记录了要让GPU执行的操作
// - Command Pool（命令池）：用于分配命令缓冲区的内存池，类似于内存管理器
// - Queue（队列）：GPU的执行队列，命令缓冲区提交到队列后才会被GPU执行
// - Fence（栅栏）：用于CPU和GPU之间同步的信号量，CPU可以等待GPU完成特定操作
//

#ifndef BOOKCOMPOSE_COMMANDQUEUEMANAGER_H
#define BOOKCOMPOSE_COMMANDQUEUEMANAGER_H

#include <functional>
#include <string>
#include <vector>

#include "Buffer.h"
#include "Common.h"
#include "Utils.h"

namespace VkCore {

    // 前向声明
    class Context;

    /**
     * @brief Vulkan命令队列管理器
     * 
     * 这个类管理Vulkan的命令缓冲区、命令池和同步对象。它简化了复杂的
     * Vulkan命令提交和同步流程，为上层提供易用的接口。
     * 
     * Vulkan命令系统工作原理：
     * 1. 命令缓冲区（Command Buffer）：
     *    - 类似于一个"录音机"，记录你要GPU执行的所有操作
     *    - 例如：绘制三角形、复制数据、设置渲染状态等
     *    - 必须先"开始录制"，然后记录命令，最后"结束录制"
     * 
     * 2. 命令池（Command Pool）：
     *    - 管理命令缓冲区内存的分配器
     *    - 每个线程通常需要自己的命令池
     *    - 可以重置整个池来回收所有命令缓冲区
     * 
     * 3. 队列（Queue）：
     *    - GPU的执行管道，接收并执行命令缓冲区
     *    - 不同类型的队列支持不同操作（图形、计算、传输等）
     *    - 命令提交到队列后异步执行
     * 
     * 4. 栅栏（Fence）：
     *    - CPU-GPU同步机制，类似于"完成通知"
     *    - CPU可以等待特定的GPU操作完成
     *    - 用于确保资源在GPU使用完毕后才被释放
     * 
     * 使用场景：
     * - 每帧渲染的命令提交
     * - 资源上传和下载
     * - 计算着色器执行
     * - 多线程命令录制管理
     * 
     * @note 这个类实现了"飞行中命令"（commands in flight）的概念，
     *       允许CPU在GPU执行前一帧的同时准备下一帧，提高并行性能。
     */
    class CommandQueueManager {
    public:
        /**
         * @brief 构造命令队列管理器
         * 
         * 创建一个完整的命令管理系统，包括命令池、命令缓冲区和同步对象。
         * 
         * @param context Vulkan上下文，包含设备信息和调试工具
         * @param device Vulkan逻辑设备句柄
         * @param count 要创建的命令缓冲区数量
         * @param concurrentNumCommands 并发执行的命令数量（"飞行中"的命令数）
         * @param queueFamilyIndex 队列族索引，决定这些命令缓冲区属于哪种类型的队列
         * @param queue GPU队列句柄，命令将提交到这个队列执行
         * @param flags 命令池创建标志，默认允许重置单个命令缓冲区
         * @param name 调试名称，用于性能分析和调试工具
         * 
         * @note 队列族概念：
         * - 不同的队列族支持不同类型的操作
         * - 图形队列族：支持绘制操作
         * - 计算队列族：支持计算着色器
         * - 传输队列族：支持内存传输操作
         * - 有些队列族支持多种操作类型
         * 
         * @note 并发命令概念：
         * - GPU和CPU可以并行工作
         * - 当GPU执行第N帧时，CPU可以准备第N+1帧
         * - concurrentNumCommands通常设为2-3，平衡性能和内存使用
         * 
         * @code
         * // 使用示例：
         * auto cmdManager = std::make_unique<CommandQueueManager>(
         *     context, device, 
         *     3,           // 3个命令缓冲区
         *     2,           // 2个并发命令
         *     0,           // 图形队列族索引
         *     graphicsQueue, 
         *     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
         *     "MainRenderer"
         * );
         * @endcode
         */
        explicit CommandQueueManager(
                const Context& context, VkDevice device, uint32_t count,
                uint32_t concurrentNumCommands, uint32_t queueFamilyIndex, VkQueue queue,
                VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                const std::string& name = "");

        /**
         * @brief 析构函数
         * 
         * 自动清理所有Vulkan资源：
         * - 等待所有GPU操作完成
         * - 释放待处理的资源
         * - 销毁栅栏对象
         * - 释放命令缓冲区
         * - 销毁命令池
         */
        ~CommandQueueManager();

        /**
         * @brief 提交命令到GPU队列
         * 
         * 将录制好的命令缓冲区提交到GPU执行。这是异步操作，
         * 函数返回时GPU可能还在执行命令。
         * 
         * @param submitInfo Vulkan提交信息结构体，包含要提交的命令缓冲区
         * 
         * @note 提交过程：
         * 1. 重置当前栅栏（准备接收新的完成信号）
         * 2. 将命令提交到GPU队列
         * 3. 关联栅栏，GPU完成时会发出信号
         * 4. 标记当前索引为"已提交"状态
         * 
         * @warning 提交后不要修改命令缓冲区，直到GPU执行完毕
         */
        void submit(const VkSubmitInfo* submitInfo);

        /**
         * @brief 切换到下一个命令缓冲区
         * 
         * 在多缓冲系统中循环使用不同的命令缓冲区和栅栏，
         * 实现CPU-GPU并行处理。
         * 
         * @note 循环缓冲区概念：
         * - 假设有3个命令缓冲区：A、B、C
         * - 当GPU执行A时，CPU可以录制B
         * - 当GPU执行B时，CPU可以录制C
         * - 当GPU执行C时，CPU可以重新使用A（如果A已完成）
         * - 这样形成一个循环，提高并行效率
         */
        void goToNextCmdBuffer();

        /**
         * @brief 等待当前提交完成
         * 
         * 阻塞CPU线程，直到GPU完成当前索引对应的命令执行。
         * 完成后会自动清理相关资源。
         * 
         * @note 同步的必要性：
         * - 确保资源在GPU使用完毕后才被释放
         * - 避免在GPU还在使用时修改数据
         * - 实现帧率控制和资源管理
         * 
         * @note 性能考虑：
         * - 频繁等待会降低并行性能
         * - 通常只在必要时调用（如程序结束、资源不足等）
         */
        void waitUntilSubmitIsComplete();

        /**
         * @brief 等待所有提交完成
         * 
         * 等待所有"飞行中"的命令完成，清理所有待处理资源。
         * 通常在程序结束或重大状态改变时使用。
         * 
         * @note 使用场景：
         * - 程序退出前的清理
         * - 窗口大小改变时的资源重建
         * - 切换渲染模式时的同步
         */
        void waitUntilAllSubmitsAreComplete();

        /**
         * @brief 延迟释放Buffer资源
         * 
         * 将Buffer标记为"待释放"，当对应的GPU命令完成时自动释放。
         * 这确保了资源不会在GPU还在使用时被意外释放。
         * 
         * @param buffer 要延迟释放的Buffer智能指针
         * 
         * @note 资源管理策略：
         * - GPU操作是异步的，资源可能在提交后很久才被使用
         * - 直接释放可能导致GPU访问已释放的内存
         * - 延迟释放确保资源在安全的时机被释放
         * 
         * @code
         * // 使用示例：
         * auto tempBuffer = createStagingBuffer(data);
         * // 提交使用tempBuffer的命令
         * cmdManager->submit(&submitInfo);
         * // 标记延迟释放，GPU完成后自动清理
         * cmdManager->disposeWhenSubmitCompletes(tempBuffer);
         * @endcode
         */
        void disposeWhenSubmitCompletes(std::shared_ptr<Buffer> buffer);

        /**
         * @brief 延迟执行自定义清理函数
         * 
         * 注册一个清理函数，在GPU命令完成后执行。
         * 适用于复杂的资源清理逻辑。
         * 
         * @param deallocator 清理函数，GPU完成后会被调用
         * 
         * @code
         * // 使用示例：
         * cmdManager->disposeWhenSubmitCompletes([=]() {
         *     // 自定义清理逻辑
         *     releaseCustomResource();
         *     updateStatistics();
         * });
         * @endcode
         */
        void disposeWhenSubmitCompletes(std::function<void()>&& deallocator);

        /**
         * @brief 获取可开始录制的命令缓冲区
         * 
         * 返回一个已经调用了vkBeginCommandBuffer的命令缓冲区，
         * 可以立即开始录制命令。
         * 
         * @return 已开始录制的命令缓冲区句柄
         * 
         * @note 自动处理的操作：
         * 1. 等待对应的栅栏完成（确保之前的使用已结束）
         * 2. 重置命令缓冲区（清空之前的命令）
         * 3. 开始录制（调用vkBeginCommandBuffer）
         * 
         * @note 使用模式：
         * - 获取命令缓冲区
         * - 录制命令（vkCmd*系列函数）
         * - 结束录制（调用endCmdBuffer）
         * - 提交执行（调用submit）
         * 
         * @code
         * // 使用示例：
         * VkCommandBuffer cmd = cmdManager->getCmdBufferToBegin();
         * vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
         * vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
         * vkCmdDraw(cmd, 3, 1, 0, 0);
         * vkCmdEndRenderPass(cmd);
         * cmdManager->endCmdBuffer(cmd);
         * @endcode
         */
        VkCommandBuffer getCmdBufferToBegin();

        /**
         * @brief 获取新的命令缓冲区
         * 
         * 从命令池分配一个新的命令缓冲区，但不开始录制。
         * 适用于需要手动控制录制生命周期的场景。
         * 
         * @return 新分配的命令缓冲区句柄
         * 
         * @note 与getCmdBufferToBegin的区别：
         * - 这个函数只分配，不开始录制
         * - 需要手动调用vkBeginCommandBuffer
         * - 适用于特殊的录制需求
         */
        VkCommandBuffer getCmdBuffer();

        /**
         * @brief 结束命令缓冲区录制
         * 
         * 调用vkEndCommandBuffer结束命令录制，之后命令缓冲区
         * 就可以被提交到队列执行了。
         * 
         * @param cmdBuffer 要结束录制的命令缓冲区
         * 
         * @note 命令缓冲区状态：
         * - Initial（初始）：刚分配，还不能录制
         * - Recording（录制中）：正在录制命令
         * - Executable（可执行）：录制完成，可以提交
         * - Pending（等待中）：已提交，GPU正在执行
         * - Invalid（无效）：执行完成或出错，需要重置
         */
        void endCmdBuffer(VkCommandBuffer cmdBuffer);

        /**
         * @brief 获取队列族索引
         * @return 这个管理器关联的队列族索引
         * 
         * @note 队列族索引的用途：
         * - 确定资源兼容性
         * - 设置内存屏障
         * - 多队列协作时的同步
         */
        [[nodiscard]] uint32_t queueFamilyIndex() const { return queueFamilyIndex_; }

    private:
        /**
         * @brief 释放所有待处理的资源
         * 
         * 执行所有注册的清理函数，释放延迟释放的资源。
         * 通常在GPU操作完成后自动调用。
         */
        void deallocateResources();

    private:
        // === 核心配置参数 ===
        
        /** 
         * @brief 并发执行的命令数量
         * 
         * 决定了可以同时"飞行中"的命令数量。通常设为2-3：
         * - 2: 双缓冲，CPU准备下一帧时GPU执行当前帧
         * - 3: 三缓冲，提供更好的并行性，但占用更多内存
         */
        uint32_t commandsInFlight_ = 2;
        
        /** 
         * @brief 队列族索引
         * 
         * 标识这个管理器使用的队列族类型：
         * - 0: 通常是图形队列族（支持绘制操作）
         * - 其他值: 计算队列族、传输队列族等
         */
        uint32_t queueFamilyIndex_ = 0;
        
        // === Vulkan核心对象 ===
        
        /** @brief GPU队列句柄，命令提交的目标 */
        VkQueue queue_ = VK_NULL_HANDLE;
        
        /** @brief Vulkan逻辑设备句柄 */
        VkDevice device_ = VK_NULL_HANDLE;
        
        /** 
         * @brief 命令池句柄
         * 
         * 命令池是命令缓冲区的内存管理器：
         * - 所有命令缓冲区都从这个池分配
         * - 可以一次性重置整个池来回收所有命令缓冲区
         * - 每个线程通常需要独立的命令池
         */
        VkCommandPool commandPool_ = VK_NULL_HANDLE;
        
        // === 命令缓冲区管理 ===
        
        /** 
         * @brief 命令缓冲区数组
         * 
         * 存储所有可用的命令缓冲区句柄。通过索引循环使用，
         * 实现多缓冲并行处理。
         */
        std::vector<VkCommandBuffer> commandBuffers_;
        
        /** 
         * @brief 当前命令缓冲区索引
         * 
         * 指向当前正在使用的命令缓冲区。每次调用goToNextCmdBuffer()
         * 时会循环递增，实现轮转使用。
         */
        uint32_t commandBufferCurrentIndex_ = 0;
        
        // === 同步对象管理 ===
        
        /** 
         * @brief 栅栏对象数组
         * 
         * 每个"飞行中"的命令对应一个栅栏：
         * - 栅栏用于CPU-GPU同步
         * - GPU完成命令时会发出栅栏信号
         * - CPU可以等待栅栏来确认GPU完成
         */
        std::vector<VkFence> fences_;
        
        /** 
         * @brief 提交状态数组
         * 
         * 记录每个栅栏对应的命令是否已提交：
         * - true: 已提交到GPU，正在执行或等待执行
         * - false: 未提交或已完成
         */
        std::vector<bool> isSubmitted_;
        
        /** 
         * @brief 当前栅栏索引
         * 
         * 指向当前使用的栅栏。与commandBufferCurrentIndex_配合，
         * 实现命令缓冲区和栅栏的同步管理。
         */
        uint32_t fenceCurrentIndex_ = 0;
        
        // === 资源延迟释放系统 ===
        
        /** 
         * @brief 待释放Buffer数组
         * 
         * 二维数组结构：bufferToDispose_[fenceIndex][bufferIndex]
         * - 第一维：对应不同的栅栏（即不同的提交批次）
         * - 第二维：该批次中所有待释放的Buffer
         * 
         * 当栅栏信号时，对应索引的所有Buffer会被自动释放。
         */
        std::vector<std::vector<std::shared_ptr<Buffer>>> bufferToDispose_;
        
        /** 
         * @brief 自定义清理函数数组
         * 
         * 二维数组结构：deallocators_[fenceIndex][functionIndex]
         * - 存储用户注册的自定义清理函数
         * - 当对应的栅栏信号时，这些函数会被调用
         * - 用于复杂的资源清理逻辑
         */
        std::vector<std::vector<std::function<void()>>> deallocators_;
    };

} // VkCore

#endif //BOOKCOMPOSE_COMMANDQUEUEMANAGER_H
