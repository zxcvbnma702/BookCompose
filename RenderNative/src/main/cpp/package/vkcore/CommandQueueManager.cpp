//
// Created by nio on 2025/7/23.
//
// Vulkan 命令队列管理器实现
//
// 这个文件实现了复杂的Vulkan命令管理系统，包括：
// - 命令池和命令缓冲区的创建与管理
// - CPU-GPU同步机制（栅栏）
// - 多缓冲并行处理系统
// - 资源延迟释放系统
// - 命令提交和执行管理
//

#include "CommandQueueManager.h"

#include <algorithm>
#include <iostream>

#include "Context.h"

namespace VkCore {
    /**
     * @brief 命令队列管理器构造函数实现
     * 
     * 这个构造函数创建了一个完整的Vulkan命令管理系统，包括：
     * 1. 命令池的创建
     * 2. 多个命令缓冲区的分配
     * 3. 栅栏同步对象的创建
     * 4. 资源管理系统的初始化
     */
    CommandQueueManager::CommandQueueManager(const Context& context, VkDevice device,
                                             uint32_t count, uint32_t concurrentNumCommands,
                                             uint32_t queueFamilyIndex, VkQueue queue,
                                             VkCommandPoolCreateFlags flags,
                                             const std::string& name)
            : commandsInFlight_(concurrentNumCommands),  // 设置并发命令数量
              queueFamilyIndex_(queueFamilyIndex),       // 保存队列族索引
              queue_(queue),                             // 保存GPU队列句柄
              device_(device) {                          // 保存设备句柄
        
        // === 预分配内存空间 ===
        // 为各种容器预先分配空间，避免后续的内存重新分配
        fences_.reserve(commandsInFlight_);           // 栅栏数组预分配
        isSubmitted_.reserve(commandsInFlight_);      // 提交状态数组预分配
        bufferToDispose_.resize(commandsInFlight_);   // 待释放Buffer数组初始化
        deallocators_.resize(commandsInFlight_);      // 清理函数数组初始化

        // === 创建命令池 ===
        // 命令池是管理命令缓冲区内存的对象
        const VkCommandPoolCreateInfo commandPoolInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // 结构体类型标识
                .flags = flags,                                      // 创建标志（如是否允许重置）
                .queueFamilyIndex = queueFamilyIndex_,               // 关联的队列族索引
        };
        
        // 调用Vulkan API创建命令池
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool_));
        
        // === 设置调试名称 ===
        // 为命令池设置可读的名称，便于在调试工具中识别
        context.setVkObjectname(commandPool_, VK_OBJECT_TYPE_COMMAND_POOL,
                                "Command pool: " + name);

        // === 分配命令缓冲区 ===
        // 设置命令缓冲区分配信息
        const VkCommandBufferAllocateInfo commandBufferInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // 结构体类型
                .commandPool = commandPool_,                             // 从哪个命令池分配
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // 主命令缓冲区（可直接提交）
                .commandBufferCount = 1,                                 // 每次分配1个
        };
        
        // 循环创建指定数量的命令缓冲区
        for (size_t i = 0; i < count; ++i) {
            VkCommandBuffer cmdBuffer;
            
            // 从命令池分配一个命令缓冲区
            VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferInfo, &cmdBuffer));
            
            // 为每个命令缓冲区设置独特的调试名称
            context.setVkObjectname(cmdBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                    "Command buffer: " + name + " " + std::to_string(i));

            // 将新创建的命令缓冲区添加到数组中
            commandBuffers_.push_back(cmdBuffer);
        }

        // === 创建栅栏同步对象 ===
        // 为每个并发命令创建一个栅栏
        for (size_t i = 0; i < commandsInFlight_; ++i) {
            VkFence fence;
            
            // 设置栅栏创建信息
            const VkFenceCreateInfo fenceInfo = {
                    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,  // 结构体类型
                    // VK_FENCE_CREATE_SIGNALED_BIT: 创建时就处于“信号”状态
                    // 这意味着初始时栅栏已经“完成”，可以立即使用
                    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            };
            
            // 创建栅栏对象
            VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

            // 将栅栏添加到管理数组中
            fences_.push_back(std::move(fence));
            
            // 初始化提交状态为“未提交”
            isSubmitted_.push_back(false);
        }
    }

    /**
     * @brief 命令队列管理器析构函数实现
     * 
     * 按照Vulkan资源管理的最佳实践，按正确的顺序清理所有资源。
     * 释放顺序很重要：先释放依赖资源，再释放主要资源。
     */
    CommandQueueManager::~CommandQueueManager() {
        // === 第1步：清理延迟释放的资源 ===
        // 执行所有注册的清理函数，释放延迟释放的Buffer
        deallocateResources();

        // === 第2步：销毁栅栏对象 ===
        // 栅栏用于同步，必须在命令缓冲区之前释放
        for (size_t i = 0; i < commandsInFlight_; ++i) {
            vkDestroyFence(device_, fences_[i], nullptr);
        }

        // === 第3步：释放命令缓冲区 ===
        // 将所有命令缓冲区返还给命令池
        for (auto & commandBuffer : commandBuffers_) {
            // vkFreeCommandBuffers将命令缓冲区内存返还给命令池
            vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        }

        // === 第4步：销毁命令池 ===
        // 最后销毁命令池本身，这会释放所有剩余的内存
        vkDestroyCommandPool(device_, commandPool_, nullptr);
    }

    /**
     * @brief 提交命令到GPU队列的实现
     * 
     * 这是整个系统的核心函数，负责将CPU上录制的命令提交给GPU执行。
     */
    void CommandQueueManager::submit(const VkSubmitInfo* submitInfo) {
        // === 第1步：重置栅栏 ===
        // 将当前栅栏从“信号”状态重置为“未信号”状态
        // 这样GPU完成新命令时才会发出新的信号
        VK_CHECK(vkResetFences(device_, 1, &fences_[fenceCurrentIndex_]));
        
        // === 第2步：提交命令 ===
        // 将命令缓冲区提交到GPU队列，并关联栅栏
        // GPU执行完所有命令后会自动发出栅栏信号
        VK_CHECK(vkQueueSubmit(queue_,                    // GPU队列
                               1,                         // 提交信息数量
                               submitInfo,                // 提交信息（包含命令缓冲区）
                               fences_[fenceCurrentIndex_])); // 关联的栅栏
        
        // === 第3步：更新状态 ===
        // 标记当前栅栏对应的命令已经提交
        isSubmitted_[fenceCurrentIndex_] = true;
    }

    /**
     * @brief 切换到下一个命令缓冲区的实现
     * 
     * 实现循环缓冲区系统，允许CPU和GPU并行工作。
     * 当GPU执行当前命令时，CPU可以准备下一个命令。
     */
    void CommandQueueManager::goToNextCmdBuffer() {
        // === 切换命令缓冲区索引 ===
        // 使用模运算实现循环：0 -> 1 -> 2 -> 0 -> 1 -> ...
        commandBufferCurrentIndex_ =
                (commandBufferCurrentIndex_ + 1) % static_cast<uint32_t>(commandBuffers_.size());
        
        // === 切换栅栏索引 ===
        // 栅栏索引与命令缓冲区索引同步切换
        fenceCurrentIndex_ = (fenceCurrentIndex_ + 1) % commandsInFlight_;
        
        // 注意：这里没有检查新的索引是否可用
        // 调用者需要在使用旰命令缓冲区之前检查同步状态
    }

    /**
     * @brief 等待当前提交完成的实现
     * 
     * 阻塞CPU线程，直到GPU完成当前索引对应的命令执行。
     * 这是CPU-GPU同步的关键机制。
     */
    void CommandQueueManager::waitUntilSubmitIsComplete() {
        // === 检查是否需要等待 ===
        // 如果当前栅栏对应的命令没有被提交，则无需等待
        if (!isSubmitted_[fenceCurrentIndex_]) {
            return;
        }

        // === 等待GPU完成 ===
        // vkWaitForFences会阻塞CPU线程，直到GPU发出栅栏信号
        const auto result = vkWaitForFences(
                device_,                        // Vulkan设备
                1,                              // 等待的栅栏数量
                &fences_[fenceCurrentIndex_],   // 要等待的栅栏
                true,                           // 是否等待所有栅栏（这里只有1个）
                UINT32_MAX);                    // 超时时间（无限等待）
        
        // === 处理超时情况 ===
        // 如果等待超时（理论上不会发生，因为UINT32_MAX）
        if (result == VK_TIMEOUT) {
            std::cerr << "Timeout!" << std::endl;
            // 强制等待整个设备空闲，确保所有操作完成
            vkDeviceWaitIdle(device_);
        }

        // === 更新状态和清理资源 ===
        isSubmitted_[fenceCurrentIndex_] = false;           // 标记为未提交状态
        bufferToDispose_[fenceCurrentIndex_].clear();       // 清空待释放Buffer列表
        deallocateResources();                              // 执行所有清理函数
    }

    /**
     * @brief 等待所有提交完成的实现
     * 
     * 这是一个“全局同步”函数，等待所有“飞行中”的命令完成。
     * 通常在程序结束或重大状态改变时使用。
     */
    void CommandQueueManager::waitUntilAllSubmitsAreComplete() {
        // === 遍历所有栅栏 ===
        // 手动管理索引以确保C++标准兼容性
        size_t index = 0;
        for (auto& fence : fences_) {
            // 等待当前栅栏完成
            VK_CHECK(vkWaitForFences(device_, 1, &fence, true, UINT32_MAX));
            
            // 重置栅栏为“未信号”状态，为下次使用做准备
            VK_CHECK(vkResetFences(device_, 1, &fence));
            
            // 标记对应的命令为未提交状态
            isSubmitted_[index++] = false;
        }
        
        // === 清理所有待释放资源 ===
        bufferToDispose_.clear();  // 清空所有待释放Buffer
        deallocateResources();     // 执行所有清理函数
    }

    /**
     * @brief 注册Buffer延迟释放的实现
     * 
     * 将Buffer添加到当前栅栏对应的延迟释放列表中。
     * 当GPU完成对应的命令时，Buffer会被自动释放。
     */
    void CommandQueueManager::disposeWhenSubmitCompletes(std::shared_ptr<Buffer> buffer) {
        // 将Buffer添加到当前栅栏索引对应的延迟释放列表中
        // std::move避免不必要的引用计数增加
        bufferToDispose_[fenceCurrentIndex_].push_back(std::move(buffer));
        
        // 注意：这里使用的是fenceCurrentIndex_，意味着：
        // - Buffer与当前即将提交的命令关联
        // - 当这个命令完成时，Buffer才会被释放
        // - 这确保了Buffer在GPU使用期间不会被意外释放
    }

    /**
     * @brief 注册自定义清理函数的实现
     * 
     * 允许用户注册复杂的资源清理逻辑，在GPU完成后执行。
     * 适用于需要特殊处理的资源清理场景。
     */
    void CommandQueueManager::disposeWhenSubmitCompletes(
            std::function<void()>&& deallocator) {
        // 将清理函数添加到当前栅栏对应的函数列表中
        // std::move避免函数对象的不必要复制
        deallocators_[fenceCurrentIndex_].push_back(std::move(deallocator));
        
        // 使用场景示例：
        // - 释放非管理的资源（如原始指针）
        // - 更新统计信息
        // - 发送完成通知
        // - 执行清理后的状态更新
    }

    /**
     * @brief 获取并开始命令缓冲区录制的实现
     * 
     * 这是一个便利函数，自动处理了命令缓冲区的同步、重置和开始录制。
     * 返回的命令缓冲区可以立即用于录制命令。
     */
    VkCommandBuffer CommandQueueManager::getCmdBufferToBegin() {
        // === 第1步：等待同步 ===
        // 等待当前栅栏对应的GPU操作完成
        // 这确保了对应的命令缓冲区不再被GPU使用，可以安全重用
        VK_CHECK(vkWaitForFences(device_, 1, &fences_[fenceCurrentIndex_], true, UINT32_MAX));
        
        // === 第2步：重置命令缓冲区 ===
        // 清空命令缓冲区中的所有旧命令，释放关联的资源
        VK_CHECK(vkResetCommandBuffer(
                commandBuffers_[commandBufferCurrentIndex_],    // 要重置的命令缓冲区
                VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));// 释放资源标志

        // === 第3步：开始录制 ===
        // 设置命令缓冲区开始信息
        const VkCommandBufferBeginInfo info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // 结构体类型
                // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: 表示这个命令缓冲区
                // 只会被提交一次，之后就会被重置或释放
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        
        // 调用Vulkan API开始命令缓冲区录制
        VK_CHECK(vkBeginCommandBuffer(commandBuffers_[commandBufferCurrentIndex_], &info));

        // === 返回可用的命令缓冲区 ===
        return commandBuffers_[commandBufferCurrentIndex_];
    }

    /**
     * @brief 获取新的命令缓冲区的实现
     * 
     * 从命令池分配一个新的命令缓冲区，但不自动开始录制。
     * 适用于需要手动控制录制生命周期的特殊场景。
     */
    VkCommandBuffer CommandQueueManager::getCmdBuffer() {
        // === 设置分配信息 ===
        const VkCommandBufferAllocateInfo commandBufferInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // 结构体类型
                .commandPool = commandPool_,                             // 从哪个命令池分配
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // 主命令缓冲区级别
                .commandBufferCount = 1,                                 // 分配数量
        };
        
        // === 分配命令缓冲区 ===
        VkCommandBuffer cmdBuffer{VK_NULL_HANDLE};
        VK_CHECK(vkAllocateCommandBuffers(device_, &commandBufferInfo, &cmdBuffer));

        // 注意：这个函数返回的命令缓冲区需要手动：
        // 1. 调用vkBeginCommandBuffer开始录制
        // 2. 录制命令
        // 3. 调用vkEndCommandBuffer结束录制
        // 4. 提交执行
        // 5. 手动释放（如果需要）
        
        return cmdBuffer;
    }

    /**
     * @brief 结束命令缓冲区录制的实现
     * 
     * 调用Vulkan API结束命令缓冲区的录制阶段，
     * 使其进入可执行状态。
     */
    void CommandQueueManager::endCmdBuffer(VkCommandBuffer cmdBuffer) {
        // 调用Vulkan API结束命令缓冲区录制
        // 此后命令缓冲区就可以被提交到队列执行了
        VK_CHECK(vkEndCommandBuffer(cmdBuffer));
        
        // 命令缓冲区状态变化：
        // Recording（录制中） -> Executable（可执行）
        // 
        // 接下来的步骤通常是：
        // 1. 创建VkSubmitInfo结构体
        // 2. 调用submit()函数提交到GPU
        // 3. 等待GPU完成执行
    }

    /**
     * @brief 执行资源清理的实现
     * 
     * 遍历所有注册的清理函数并执行它们。
     * 这是资源延迟释放系统的核心机制。
     */
    void CommandQueueManager::deallocateResources() {
        // === 嵌套循环执行所有清理函数 ===
        // 外层循环：遍历每个栅栏对应的清理函数列表
        for (auto& deallocators : deallocators_) {
            // 内层循环：执行当前列表中的所有清理函数
            for (auto& deallocator : deallocators) {
                // 调用清理函数
                // 这些函数可能包含：
                // - 释放非管理的内存
                // - 关闭文件句柄
                // - 更新状态机
                // - 发送完成信号等
                deallocator();
            }
        }
        
        // 注意：这个函数不会清空清理函数列表
        // 清空操作由调用者负责，通常在waitUntil*函数中完成
    }
} // VkCore

/*
 * === Vulkan 命令队列管理器使用指南（面向初学者）===
 * 
 * 1. 基本概念理解：
 *    - 命令缓冲区 = 记录GPU操作的"任务清单"
 *    - 命令池 = 管理命令缓冲区内存的"分配器"
 *    - 队列 = GPU的"执行管道"，接收并执行命令
 *    - 栅栏 = CPU-GPU同步的"完成通知"机制
 * 
 * 2. 多缓冲并行概念：
 *    - CPU可以在GPU执行命令的同时准备下一批命令
 *    - 通过循环使用多个命令缓冲区实现并行
 *    - 栅栏确保命令缓冲区在重用前已完成执行
 * 
 * 3. 典型使用流程：
 * 
 *    // 步骤1：创建命令队列管理器
 *    auto cmdManager = std::make_unique<CommandQueueManager>(
 *        context, device, 
 *        3,           // 3个命令缓冲区（支持更好的并行）
 *        2,           // 2个并发命令（双缓冲）
 *        0,           // 图形队列族索引
 *        graphicsQueue, 
 *        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
 *        "MainRenderer"
 *    );
 * 
 *    // 步骤2：录制命令（每帧执行）
 *    VkCommandBuffer cmd = cmdManager->getCmdBufferToBegin();
 *    
 *    // 开始渲染通道
 *    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
 *    
 *    // 绑定管线和资源
 *    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
 *    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
 *    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
 *    
 *    // 绑定描述符集（纹理、uniform等）
 *    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
 *                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
 *    
 *    // 执行绘制命令
 *    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
 *    
 *    // 结束渲染通道
 *    vkCmdEndRenderPass(cmd);
 *    
 *    // 结束命令录制
 *    cmdManager->endCmdBuffer(cmd);
 * 
 *    // 步骤3：提交命令到GPU
 *    VkSubmitInfo submitInfo = {
 *        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
 *        .commandBufferCount = 1,
 *        .pCommandBuffers = &cmd
 *    };
 *    cmdManager->submit(&submitInfo);
 * 
 *    // 步骤4：资源管理（可选）
 *    // 如果有临时资源需要在GPU完成后释放
 *    cmdManager->disposeWhenSubmitCompletes(tempBuffer);
 *    
 *    // 或者注册自定义清理逻辑
 *    cmdManager->disposeWhenSubmitCompletes([=]() {
 *        // 清理临时资源
 *        cleanupTempResources();
 *    });
 * 
 *    // 步骤5：切换到下一个缓冲区（为下一帧做准备）
 *    cmdManager->goToNextCmdBuffer();
 * 
 * 4. 同步和等待：
 * 
 *    // 等待当前命令完成（谨慎使用，会影响并行性能）
 *    cmdManager->waitUntilSubmitIsComplete();
 * 
 *    // 程序结束前等待所有命令完成
 *    cmdManager->waitUntilAllSubmitsAreComplete();
 * 
 * 5. 性能优化建议：
 *    - 避免频繁的waitUntil调用，让CPU和GPU并行工作
 *    - 合理设置commandsInFlight数量（通常2-3个）
 *    - 使用延迟释放机制管理临时资源
 *    - 批量提交命令，减少submit调用次数
 * 
 * 6. 常见错误和解决方案：
 *    - 错误：在GPU还在使用时修改命令缓冲区
 *      解决：确保等待栅栏完成后再重用
 *    
 *    - 错误：资源在GPU使用时被释放
 *      解决：使用disposeWhenSubmitCompletes延迟释放
 *    
 *    - 错误：命令缓冲区状态不正确
 *      解决：确保begin/end配对，按正确顺序调用API
 * 
 *    - 错误：死锁或无限等待
 *      解决：检查栅栏状态，确保命令被正确提交
 */