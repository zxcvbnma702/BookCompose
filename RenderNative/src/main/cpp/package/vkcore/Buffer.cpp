//
// Created by Nio on 2025/8/28.
//
// Vulkan Buffer 管理类实现
//
// 这个文件实现了Vulkan缓冲区的完整生命周期管理，包括：
// - 缓冲区创建和内存分配
// - CPU-GPU数据传输
// - 内存映射和数据复制
// - 资源清理和释放
//

#include "Buffer.h"

#include <algorithm>
#include <iostream>

#include "Context.h"
#include "Texture.h"

namespace VkCore {
    /**
     * @brief Staging Buffer构造函数实现
     * 
     * 这个构造函数创建一个staging buffer，用于从CPU向GPU传输数据。
     * Staging buffer是CPU可访问的，而目标buffer是GPU专用的。
     */
    Buffer::Buffer(const Context *context, VmaAllocator vmaAllocator, VkDeviceSize size,
                   VkBufferUsageFlags usage, Buffer *actualBuffer, const std::string &name)
            : context_(context), allocator_(vmaAllocator), size_(size),
              actualBufferIfStaging_(actualBuffer) {

        // === 参数验证 ===
        // 确保actualBuffer不为空，因为staging buffer需要一个目标缓冲区
        ASSERT(actualBufferIfStaging_,
               "Actual Buffer must not be null in case of staging buffer");

        // 确保目标缓冲区支持作为传输目标（可以接收数据）
        ASSERT(actualBufferIfStaging_->usage_ & VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               "Actual buffer must be dst buffer in case of staging buffer");

        // 确保目标缓冲区是GPU专用内存（性能最优）
        ASSERT(actualBufferIfStaging_->allocCreateInfo_.usage == VMA_MEMORY_USAGE_GPU_ONLY,
               "Actual buffer must be GPU only in case of staging buffer, staging "
               "buffer will upload from cpu to this gpu buffer");

        // === 创建Vulkan缓冲区 ===
        // 设置缓冲区创建信息
        VkBufferCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,  // 结构体类型标识
                .pNext = nullptr,                               // 扩展信息指针
                .flags = {},                                    // 创建标志（通常为0）
                .size = size_,                                  // 缓冲区大小
                // 用途：原有用途 + 传输源标志（staging buffer需要能够传输数据）
                .usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,       // 独占模式（只有一个队列族访问）
                .queueFamilyIndexCount = {},                    // 队列族数量（独占模式下为0）
                .pQueueFamilyIndices = {}                       // 队列族索引数组
        };

        // === 设置内存分配信息 ===
        allocCreateInfo_ = {
                // VMA_ALLOCATION_CREATE_MAPPED_BIT: 创建时就映射到CPU地址空间
                // 这样CPU可以直接访问这块内存，无需额外的映射操作
                VMA_ALLOCATION_CREATE_MAPPED_BIT, 
                // VMA_MEMORY_USAGE_CPU_ONLY: 只有CPU可以访问的内存类型
                // 这种内存类型适合staging buffer，CPU写入速度快
                VMA_MEMORY_USAGE_CPU_ONLY
        };

        // === 创建缓冲区和分配内存 ===
        // vmaCreateBuffer是VMA库的函数，一次性完成缓冲区创建和内存分配
        // VK_CHECK宏用于检查Vulkan函数调用是否成功
        VK_CHECK(vmaCreateBuffer(allocator_,        // VMA分配器
                                 &createInfo,       // 缓冲区创建信息
                                 &allocCreateInfo_, // 内存分配信息
                                 &buffer_,          // 输出：创建的缓冲区句柄
                                 &allocation_,      // 输出：内存分配句柄
                                 nullptr));         // 输出：分配信息（这里不需要）

        // === 获取分配信息 ===
        // 获取实际分配的内存信息，包括内存类型、大小、映射指针等
        vmaGetAllocationInfo(allocator_, allocation_, &allocationInfo_);

        // === 设置调试名称 ===
        // 为缓冲区设置一个可读的名称，便于在调试工具中识别
        // 这在使用RenderDoc、NSight等调试工具时非常有用
        context->setVkObjectname(buffer_, VK_OBJECT_TYPE_BUFFER, "Staging Buffer: " + name);
    }

    /**
     * @brief 通用Buffer构造函数实现
     * 
     * 这个构造函数可以创建任何类型的缓冲区，通过传入的参数进行精确控制。
     * 适用于创建GPU专用缓冲区、CPU可见缓冲区等各种类型。
     */
    Buffer::Buffer(const Context *context, VmaAllocator vmaAllocator,
                   const VkBufferCreateInfo &createInfo, const VmaAllocationCreateInfo &allocInfo,
                   const std::string &name) : context_{context},
                                              allocator_(vmaAllocator),
                                              size_(createInfo.size),
                                              usage_(createInfo.usage),
                                              allocCreateInfo_(allocInfo) {
        
        // === 创建缓冲区和分配内存 ===
        // 使用传入的参数直接创建缓冲区，提供最大的灵活性
        VK_CHECK(vmaCreateBuffer(allocator_,    // VMA分配器
                                 &createInfo,   // 用户提供的缓冲区创建信息
                                 &allocInfo,    // 用户提供的内存分配信息
                                 &buffer_,      // 输出：缓冲区句柄
                                 &allocation_,  // 输出：内存分配句柄
                                 nullptr));     // 不需要额外的分配信息
        
        // === 获取实际分配信息 ===
        // 存储实际分配的内存属性，后续操作可能需要用到
        vmaGetAllocationInfo(allocator_, allocation_, &allocationInfo_);

        // === 设置调试名称 ===
        // 为调试和性能分析工具提供可读的缓冲区名称
        context->setVkObjectname(buffer_, VK_OBJECT_TYPE_BUFFER, "Buffer: " + name);
    }

    /**
     * @brief Buffer析构函数实现
     * 
     * 按照Vulkan资源管理的最佳实践，确保所有资源都被正确释放。
     * 释放顺序很重要：先释放依赖资源，再释放主要资源。
     */
    Buffer::~Buffer() {
        // === 解除内存映射 ===
        // 如果内存被映射到CPU地址空间，需要先解除映射
        if (mappedMemory_) {
            vmaUnmapMemory(allocator_, allocation_);
        }

        // === 销毁缓冲区视图 ===
        // 缓冲区视图依赖于缓冲区，必须先销毁
        // 使用结构化绑定（C++17特性）遍历所有视图
        for (auto &[bufferViewFormat, bufferView]: bufferViews_) {
            // vkDestroyBufferView是Vulkan API，用于销毁缓冲区视图
            vkDestroyBufferView(context_->device(), bufferView, nullptr);
        }

        // === 销毁缓冲区和释放内存 ===
        // vmaDestroyBuffer会同时销毁Vulkan缓冲区对象和释放VMA管理的内存
        // 这是VMA库的便利函数，简化了资源清理过程
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
    }

    /**
     * @brief 获取缓冲区大小
     * 
     * 返回创建时指定的缓冲区大小，单位为字节。
     */
    VkDeviceSize Buffer::size() const {
        return size_;
    }

    /**
     * @brief 上传整个缓冲区数据（从指定偏移开始）
     * 
     * 这是upload(offset, size)的便利重载，上传从offset开始的所有数据。
     */
    void Buffer::upload(VkDeviceSize offset) const { 
        upload(offset, size_); 
    }

    /**
     * @brief 上传指定范围的缓冲区数据
     * 
     * 在Vulkan中，CPU对映射内存的修改可能不会立即对GPU可见。
     * 这个函数执行"flush"操作，确保指定范围的数据对GPU可见。
     * 
     * @param offset 开始偏移量（字节）
     * @param size 要刷新的数据大小（字节）
     * 
     * @note 缓存一致性概念：
     * - CPU和GPU可能有各自的缓存系统
     * - CPU写入的数据可能还在CPU缓存中，GPU看不到
     * - flush操作强制将数据从CPU缓存写入到主内存
     * - 这样GPU就能看到最新的数据了
     */
    void Buffer::upload(VkDeviceSize offset, VkDeviceSize size) const {
        // vmaFlushAllocation是VMA库提供的函数，用于刷新指定范围的内存
        // 相当于调用vkFlushMappedMemoryRanges，但更简单
        VK_CHECK(vmaFlushAllocation(allocator_,  // VMA分配器
                                   allocation_,  // 内存分配句柄
                                   offset,       // 刷新起始偏移
                                   size));       // 刷新大小
    }

    /**
     * @brief 将staging buffer的数据复制到GPU缓冲区
     * 
     * 这是staging buffer最重要的功能：将CPU可访问的数据传输到GPU专用内存。
     * 这个操作必须在GPU命令缓冲区中执行，是异步操作。
     * 
     * @param commandBuffer GPU命令缓冲区，用于记录GPU操作命令
     * @param srcOffset 源缓冲区（staging buffer）的偏移量
     * @param dstOffset 目标缓冲区（GPU buffer）的偏移量
     * 
     * @note 命令缓冲区概念：
     * - Vulkan中的GPU操作不是立即执行的，而是记录在命令缓冲区中
     * - 命令缓冲区类似于一个“待办事项”列表
     * - 只有当命令缓冲区被提交到GPU队列时，操作才会真正执行
     * 
     * @warning 这个函数只能在staging buffer上调用
     */
    void Buffer::uploadStagingBufferToGPU(VkCommandBuffer const &commandBuffer, uint64_t srcOffset,
                                          uint64_t dstOffset) const {
        // === 设置复制区域 ===
        // VkBufferCopy结构体定义了要复制的数据范围
        VkBufferCopy region{
            .srcOffset = srcOffset,  // 源缓冲区偏移量
            .dstOffset = dstOffset,  // 目标缓冲区偏移量
            .size = size_            // 复制的数据大小
        };
        
        // === 参数验证 ===
        // 确保这是一个staging buffer，具有有效的目标缓冲区
        ASSERT(actualBufferIfStaging_ != nullptr,
               "actualBufferIfStaging_ can't be null in case of staging");
        
        // === 记录复制命令 ===
        // vkCmdCopyBuffer是Vulkan API，在命令缓冲区中记录一个缓冲区复制命令
        vkCmdCopyBuffer(commandBuffer,                          // 命令缓冲区
                        vkBuffer(),                             // 源缓冲区（staging buffer）
                        actualBufferIfStaging_->vkBuffer(),     // 目标缓冲区（GPU buffer）
                        1,                                      // 复制区域数量
                        &region);                               // 复制区域数组
    }

    /**
     * @brief 将CPU数据复制到缓冲区
     * 
     * 这个函数将CPU内存中的数据直接复制到缓冲区的映射内存中。
     * 适用于CPU可访问的缓冲区（如staging buffer或CPU_TO_GPU类型缓冲区）。
     * 
     * @param data 源数据指针，指向要复制的CPU数据
     * @param size 数据大小（字节）
     * 
     * @note 内存映射过程：
     * 1. 检查是否已经映射到CPU地址空间
     * 2. 如果未映射，调用vmaMapMemory进行映射
     * 3. 使用memcpy将数据复制到映射的内存中
     * 
     * @warning 复制数据后，可能需要调用upload()函数来确保数据对GPU可见
     */
    void Buffer::copyDataToBuffer(const void *data, size_t size) const {
        // === 检查并执行内存映射 ===
        // 如果内存还未映射到CPU地址空间，需要先进行映射
        if (!mappedMemory_) {
            // vmaMapMemory将GPU内存映射到CPU可访问的地址空间
            // 映射后，mappedMemory_将指向可以直接访问的内存地址
            VK_CHECK(vmaMapMemory(allocator_,       // VMA分配器
                                  allocation_,      // 内存分配句柄
                                  &mappedMemory_)); // 输出：映射的内存地址
        }
        
        // === 复制数据 ===
        // 使用标准C库的memcpy函数将数据复制到映射的内存中
        // 这就像普通的内存复制操作一样简单
        memcpy(mappedMemory_,  // 目标地址（映射的GPU内存）
               data,           // 源地址（CPU数据）
               size);          // 复制大小
    }

    /**
     * @brief 获取缓冲区的GPU设备地址
     * 
     * 设备地址是GPU端的内存地址，允许着色器直接访问缓冲区数据。
     * 这是一个相对较新的Vulkan特性，主要用于高级渲染技术。
     * 
     * @return GPU设备地址，如果不支持或不可用则返回0
     * 
     * @note 设备地址的使用场景：
     * - 光线追踪：加速结构需要缓冲区的设备地址
     * - 无绑定资源：着色器可以通过地址直接访问数据
     * - 计算着色器：高效的数据结构遍历和操作
     * 
     * @note 功能支持要求：
     * - 需要VK_KHR_buffer_device_address扩展
     * - 缓冲区创建时需要VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT标志
     * - 目前只在Windows平台上启用
     */
    VkDeviceAddress Buffer::vkDeviceAddress() const {
        // === Staging Buffer特殊处理 ===
        // 如果这是一个staging buffer，返回实际GPU缓冲区的设备地址
        // 因为staging buffer本身不会被着色器直接访问
        if (actualBufferIfStaging_) {
            return actualBufferIfStaging_->vkDeviceAddress();
        }

        // === 条件编译：只在支持的平台上启用 ===
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
        // === 延迟获取设备地址 ===
        // 只有在首次需要时才查询设备地址，提高性能
        if (!bufferDeviceAddress_) {
            // 设置查询参数
            const VkBufferDeviceAddressInfo bdAddressInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // 结构体类型
                .buffer = buffer_,                                     // 要查询的缓冲区
            };
            
            // 调用Vulkan API获取设备地址
            bufferDeviceAddress_ = vkGetBufferDeviceAddress(context_->device(), &bdAddressInfo);
        }
        return bufferDeviceAddress_;
#else
        // === 不支持的平台返回0 ===
        // 在不支持设备地址的平台上，返回0表示无效地址
        return 0;
#endif
    }

    /**
     * @brief 请求指定格式的缓冲区视图
     * 
     * 缓冲区视图允许将原始的字节数据解释为特定格式的结构化数据。
     * 这对于着色器中访问缓冲区数据非常有用。
     * 
     * @param viewFormat 视图格式（如VK_FORMAT_R32G32B32A32_SFLOAT等）
     * @return 缓冲区视图句柄
     * 
     * @note 缓冲区视图的作用：
     * - 类似于将一个int数组视为float数组
     * - 着色器可以通过视图以结构化方式读取数据
     * - 支持各种数据类型：整数、浮点数、向量等
     * 
     * @note 性能优化：
     * - 这个函数会缓存创建的视图
     * - 相同格式的视图只会创建一次，后续请求直接返回缓存的版本
     */
    VkBufferView Buffer::requestBufferView(VkFormat viewFormat) {
        // === 检查缓存 ===
        // 先在缓存中查找是否已经创建了这种格式的视图
        auto itr = bufferViews_.find(viewFormat);
        if (itr != bufferViews_.end()) {
            // 找到了缓存的视图，直接返回
            return itr->second;
        }

        // === 创建新的缓冲区视图 ===
        // 设置视图创建参数
        VkBufferViewCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, // 结构体类型
                .flags = 0,                                         // 创建标志（通常为0）
                .buffer = vkBuffer(),                               // 关联的缓冲区
                .format = viewFormat,                               // 数据格式
                .offset = 0,                                        // 视图开始偏移（从头开始）
                .range = size_,                                     // 视图覆盖范围（整个缓冲区）
        };
        
        // 声明视图句柄变量
        VkBufferView bufferView;
        
        // === 创建视图 ===
        // 调用Vulkan API创建缓冲区视图
        VK_CHECK(vkCreateBufferView(context_->device(), // Vulkan设备
                                    &createInfo,        // 创建信息
                                    nullptr,            // 内存分配回调（通常为nullptr）
                                    &bufferView));      // 输出：创建的视图句柄
        
        // === 缓存新创建的视图 ===
        // 将新创建的视图存入缓存中，以便下次直接使用
        bufferViews_[viewFormat] = bufferView;
        
        return bufferView;
    }


} // VkCore

/*
 * === Vulkan Buffer 使用指南（面向初学者）===
 * 
 * 1. 基本概念理解：
 *    - Buffer = GPU内存中的一块连续空间，存储各种数据
 *    - Staging Buffer = CPU可访问的临时缓冲区，用于数据传输
 *    - GPU Buffer = GPU专用的高性能缓冲区，CPU通常无法直接访问
 * 
 * 2. 典型使用流程：
 *    a) 创建GPU缓冲区（目标）
 *    b) 创建Staging缓冲区（临时）
 *    c) 将数据写入Staging缓冲区
 *    d) 通过GPU命令将数据从Staging传输到GPU缓冲区
 *    e) 在渲染中使用GPU缓冲区
 * 
 * 3. 代码示例：
 * 
 *    // 步骤1：创建GPU缓冲区
 *    VkBufferCreateInfo bufferInfo = {
 *        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
 *        .size = dataSize,
 *        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
 *        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
 *    };
 *    VmaAllocationCreateInfo allocInfo = {
 *        .usage = VMA_MEMORY_USAGE_GPU_ONLY
 *    };
 *    auto gpuBuffer = std::make_unique<Buffer>(context, allocator, bufferInfo, allocInfo, "VertexBuffer");
 * 
 *    // 步骤2：创建Staging缓冲区
 *    auto stagingBuffer = std::make_unique<Buffer>(context, allocator, dataSize, 
 *                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
 *                                                  gpuBuffer.get(), "VertexStaging");
 * 
 *    // 步骤3：将数据写入Staging缓冲区
 *    float vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
 *    stagingBuffer->copyDataToBuffer(vertices, sizeof(vertices));
 *    stagingBuffer->upload(); // 确保数据对GPU可见
 * 
 *    // 步骤4：传输数据到GPU缓冲区
 *    // 这部分需要在命令缓冲区中执行
 *    vkBeginCommandBuffer(commandBuffer, &beginInfo);
 *    stagingBuffer->uploadStagingBufferToGPU(commandBuffer);
 *    vkEndCommandBuffer(commandBuffer);
 *    // 提交命令缓冲区执行...
 * 
 *    // 步骤5：在渲染中使用GPU缓冲区
 *    VkBuffer vertexBuffers[] = {gpuBuffer->vkBuffer()};
 *    VkDeviceSize offsets[] = {0};
 *    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
 * 
 * 4. 重要提醒：
 *    - 总是检查VK_CHECK宏的返回值
 *    - 确保在正确的时机调用upload()函数
 *    - Staging缓冲区在数据传输完成后可以销毁
 *    - 使用RAII管理资源生命周期，避免内存泄漏
 */