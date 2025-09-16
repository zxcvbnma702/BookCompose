//
// Created by Nio on 2025/8/28.
//
// Vulkan Buffer 管理类
// 
// Vulkan 基础概念说明：
// - Buffer: Vulkan中的缓冲区，类似于一块连续的内存空间，用于存储顶点数据、索引数据、
//   uniform数据等。可以理解为显卡内存中的一个数据容器。
// - VMA (Vulkan Memory Allocator): 一个第三方库，简化了Vulkan中复杂的内存管理。
// - Staging Buffer: 暂存缓冲区，用于从CPU内存向GPU内存传输数据的中转站。
//

#ifndef BOOKCOMPOSE_BUFFER_H
#define BOOKCOMPOSE_BUFFER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.h"
#include "Utils.h"
#include "vk_mem_alloc.h"

namespace VkCore {

    // 前向声明：避免循环包含，这里只声明类名
    class Context;   // Vulkan上下文管理类
    class Texture;   // 纹理管理类

    /**
     * @brief Vulkan缓冲区管理类
     * 
     * 这个类封装了Vulkan Buffer的创建、内存分配、数据传输等操作。
     * 
     * Vulkan Buffer概念解释：
     * - Buffer是Vulkan中存储数据的基本单元，类似于一个内存数组
     * - 可以存储顶点数据、索引数据、uniform数据、存储缓冲区数据等
     * - 需要绑定到特定的内存类型（CPU可见、GPU专用等）
     * 
     * Staging Buffer概念：
     * - 由于GPU内存通常不能直接被CPU访问，需要通过staging buffer作为中转
     * - 数据流向：CPU → Staging Buffer (CPU可访问内存) → GPU Buffer (GPU专用内存)
     * 
     * 使用场景：
     * - 顶点缓冲区：存储3D模型的顶点坐标、法线、纹理坐标等
     * - 索引缓冲区：存储顶点索引，用于优化渲染
     * - Uniform缓冲区：存储着色器中的常量数据（如变换矩阵、光照参数等）
     * - 存储缓冲区：用于计算着色器的数据存储
     */
    class Buffer final {
    public:
        // 只允许移动语义，禁止拷贝（因为Vulkan资源不能被简单拷贝）
        MOVABLE_ONLY(Buffer);

        /**
         * @brief 创建Staging Buffer的构造函数
         * 
         * 这个构造函数专门用于创建staging buffer（暂存缓冲区）。
         * 
         * @param context Vulkan上下文指针，包含设备、队列等信息
         * @param vmaAllocator VMA内存分配器，用于简化内存管理
         * @param size 缓冲区大小（字节数）
         * @param usage 缓冲区用途标志（如顶点缓冲、索引缓冲等）
         * @param actualBuffer 实际的GPU缓冲区指针，staging buffer会向它传输数据
         * @param name 缓冲区名称，用于调试
         * 
         * @note Staging Buffer的作用：
         * - CPU可以直接写入数据到staging buffer
         * - 然后通过GPU命令将数据从staging buffer复制到GPU专用内存
         * - 这是因为GPU专用内存通常CPU无法直接访问，性能更高
         * 
         * @code
         * // 使用示例：
         * // 1. 先创建GPU缓冲区
         * auto gpuBuffer = std::make_unique<Buffer>(context, allocator, createInfo, allocInfo);
         * // 2. 创建对应的staging buffer
         * auto stagingBuffer = std::make_unique<Buffer>(context, allocator, size, usage, gpuBuffer.get());
         * @endcode
         */
        explicit Buffer(const Context *context, VmaAllocator vmaAllocator, VkDeviceSize size,
                        VkBufferUsageFlags usage, Buffer *actualBuffer,
                        const std::string &name = "");

        /**
         * @brief 通用Buffer构造函数
         * 
         * 这个构造函数可以创建任何类型的缓冲区，包括GPU专用缓冲区、
         * CPU可见缓冲区等，通过参数进行精确控制。
         * 
         * @param context Vulkan上下文指针
         * @param vmaAllocator VMA内存分配器
         * @param createInfo Vulkan缓冲区创建信息结构体
         * @param allocInfo VMA分配信息结构体，指定内存类型和属性
         * @param name 缓冲区名称，用于调试
         * 
         * @note createInfo参数说明：
         * - size: 缓冲区大小
         * - usage: 用途标志（VK_BUFFER_USAGE_VERTEX_BUFFER_BIT等）
         * - sharingMode: 共享模式（通常是独占模式）
         * 
         * @note allocInfo参数说明：
         * - usage: 内存使用类型（GPU_ONLY、CPU_ONLY、CPU_TO_GPU等）
         * - flags: 分配标志（如是否需要映射等）
         */
        explicit Buffer(const Context *context, VmaAllocator vmaAllocator,
                        const VkBufferCreateInfo &createInfo,
                        const VmaAllocationCreateInfo &allocInfo,
                        const std::string &name = "");

        /**
         * @brief 析构函数
         * 
         * 自动清理所有Vulkan资源：
         * - 解除内存映射
         * - 销毁缓冲区视图
         * - 销毁缓冲区和释放内存
         */
        ~Buffer();

        /**
         * @brief 获取缓冲区大小
         * @return 缓冲区大小（字节数）
         */
        VkDeviceSize size() const;

        /**
         * @brief 上传缓冲区数据（从指定偏移开始）
         * 
         * 这个函数用于将CPU修改的数据同步到GPU。
         * 在Vulkan中，当你修改了映射到CPU的内存后，需要"flush"操作
         * 来确保数据真正写入到GPU可见的内存中。
         * 
         * @param offset 开始偏移量（字节），默认从0开始
         * 
         * @note 什么时候需要调用这个函数：
         * - 当你通过copyDataToBuffer修改了缓冲区内容后
         * - 需要确保GPU能看到最新的数据时
         */
        void upload(VkDeviceSize offset = 0) const;

        /**
         * @brief 上传缓冲区数据（指定偏移和大小）
         * 
         * @param offset 开始偏移量（字节）
         * @param size 要上传的数据大小（字节）
         * 
         * @note 这个版本允许你只上传部分修改的数据，可以提高性能
         */
        void upload(VkDeviceSize offset, VkDeviceSize size) const;

        /**
         * @brief 将staging buffer的数据复制到GPU缓冲区
         * 
         * 这是staging buffer的核心功能：将暂存在CPU可访问内存中的数据
         * 传输到GPU专用内存中。这个操作需要在GPU命令缓冲区中执行。
         * 
         * @param commandBuffer GPU命令缓冲区，用于记录GPU操作命令
         * @param srcOffset 源缓冲区（staging buffer）的偏移量
         * @param dstOffset 目标缓冲区（GPU buffer）的偏移量
         * 
         * @note 使用流程：
         * 1. 将数据写入staging buffer
         * 2. 开始命令缓冲区记录
         * 3. 调用此函数记录复制命令
         * 4. 结束命令缓冲区记录并提交执行
         * 
         * @warning 这个函数只能在staging buffer上调用
         */
        void uploadStagingBufferToGPU(const VkCommandBuffer &commandBuffer,
                                      uint64_t srcOffset = 0, uint64_t dstOffset = 0) const;

        /**
         * @brief 将CPU数据复制到缓冲区
         * 
         * 这个函数将CPU内存中的数据直接复制到缓冲区的映射内存中。
         * 适用于CPU可访问的缓冲区（如staging buffer）。
         * 
         * @param data 源数据指针
         * @param size 数据大小（字节）
         * 
         * @note 内存映射概念：
         * - Vulkan可以将GPU内存"映射"到CPU地址空间
         * - 映射后，CPU可以像访问普通内存一样访问GPU内存
         * - 但只有特定类型的内存支持映射（如CPU_ONLY、CPU_TO_GPU类型）
         * 
         * @code
         * // 使用示例：
         * float vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
         * buffer->copyDataToBuffer(vertices, sizeof(vertices));
         * buffer->upload(); // 确保数据同步到GPU
         * @endcode
         */
        void copyDataToBuffer(const void *data, size_t size) const;

        /**
         * @brief 获取原始Vulkan缓冲区句柄
         * @return VkBuffer句柄，用于Vulkan API调用
         * 
         * @note 句柄（Handle）概念：
         * - Vulkan使用句柄来引用GPU资源
         * - 句柄本质上是一个指针或ID，指向GPU驱动中的实际资源
         */
        VkBuffer vkBuffer() const { return buffer_; }

        /**
         * @brief 获取缓冲区的设备地址
         * 
         * 设备地址是GPU端的内存地址，主要用于：
         * - 光线追踪中的加速结构
         * - 无绑定资源（bindless resources）
         * - 着色器中直接访问缓冲区数据
         * 
         * @return GPU设备地址，如果不支持则返回0
         * 
         * @note 设备地址功能需要：
         * - 支持VK_KHR_buffer_device_address扩展
         * - 缓冲区创建时指定VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
         */
        VkDeviceAddress vkDeviceAddress() const;

        /**
         * @brief 请求缓冲区视图
         * 
         * 缓冲区视图允许将缓冲区的数据解释为特定格式的数据。
         * 主要用于纹理缓冲区（texture buffer）和存储缓冲区。
         * 
         * @param viewFormat 视图格式（如R32G32B32A32_SFLOAT等）
         * @return 缓冲区视图句柄
         * 
         * @note 缓冲区视图概念：
         * - 类似于将原始字节数据按特定格式解释
         * - 例如：将字节数据解释为浮点数数组、整数数组等
         * - 着色器可以通过视图以结构化方式访问缓冲区数据
         * 
         * @note 这个函数会缓存创建的视图，相同格式只会创建一次
         */
        VkBufferView requestBufferView(VkFormat viewFormat);

    private:
        // === 核心Vulkan对象 ===
        
        /** @brief Vulkan上下文指针，包含设备、队列等全局信息 */
        const Context *context_ = nullptr;
        
        /** @brief VMA内存分配器，简化内存管理 */
        VmaAllocator allocator_;
        
        /** @brief 缓冲区大小（字节数） */
        VkDeviceSize size_;
        
        /** 
         * @brief 缓冲区用途标志位
         * 
         * 常见的用途标志：
         * - VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: 顶点缓冲区
         * - VK_BUFFER_USAGE_INDEX_BUFFER_BIT: 索引缓冲区
         * - VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT: Uniform缓冲区
         * - VK_BUFFER_USAGE_STORAGE_BUFFER_BIT: 存储缓冲区
         * - VK_BUFFER_USAGE_TRANSFER_SRC_BIT: 可作为传输源
         * - VK_BUFFER_USAGE_TRANSFER_DST_BIT: 可作为传输目标
         */
        VkBufferUsageFlags usage_{};
        
        /** @brief VMA分配信息，指定内存类型和分配策略 */
        VmaAllocationCreateInfo allocCreateInfo_{};
        
        /** @brief Vulkan缓冲区句柄，VK_NULL_HANDLE表示无效句柄 */
        VkBuffer buffer_ = VK_NULL_HANDLE;
        
        // === Staging Buffer相关 ===
        
        /** 
         * @brief 实际的GPU缓冲区指针（仅staging buffer使用）
         * 
         * 当这个Buffer是staging buffer时，这个指针指向真正的GPU缓冲区。
         * staging buffer会将数据传输到这个GPU缓冲区中。
         * 如果是普通缓冲区，这个值为nullptr。
         */
        Buffer *actualBufferIfStaging_ = nullptr;
        
        // === 内存管理相关 ===
        
        /** @brief VMA分配句柄，用于内存管理和释放 */
        VmaAllocation allocation_ = nullptr;
        
        /** @brief VMA分配信息，包含实际分配的内存属性和大小 */
        VmaAllocationInfo allocationInfo_ = {};
        
        /** 
         * @brief 缓冲区的GPU设备地址（缓存）
         * 
         * mutable关键字允许在const函数中修改这个值。
         * 设备地址是延迟获取的，第一次调用时才会查询GPU驱动。
         */
        mutable VkDeviceAddress bufferDeviceAddress_ = 0;
        
        /** 
         * @brief 映射到CPU的内存指针
         * 
         * 当缓冲区内存被映射到CPU地址空间时，这个指针指向映射的内存。
         * 通过这个指针，CPU可以直接读写缓冲区数据。
         * mutable允许在const函数中进行延迟映射。
         */
        mutable void *mappedMemory_ = nullptr;
        
        // === 缓冲区视图管理 ===
        
        /** 
         * @brief 缓冲区视图缓存
         * 
         * 键：VkFormat（数据格式）
         * 值：VkBufferView（对应的缓冲区视图句柄）
         * 
         * 缓存的目的是避免重复创建相同格式的视图，提高性能。
         * 每种格式只创建一次视图，后续请求直接返回缓存的视图。
         */
        std::unordered_map<VkFormat, VkBufferView> bufferViews_;
    };

} // VkCore

#endif //BOOKCOMPOSE_BUFFER_H
