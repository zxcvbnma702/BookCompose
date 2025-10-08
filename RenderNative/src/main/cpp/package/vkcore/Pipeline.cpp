//
// Created by nio on 2025/10/8.
//

#include "Pipeline.h"

#include "Buffer.h"        // GPU缓冲区管理
#include "Context.h"       // Vulkan上下文
#include "RenderPass.h"    // 渲柕通道管理
#include "Sampler.h"       // 纹理采样器
#include "ShaderModule.h"  // 着色器模块管理
#include "Texture.h"       // 纹理资源管理

namespace VkCore {

    // 描述符集合的最大数量限制
    // 这个值决定了可以同时存在的描述符集合数量
    // 4096 * 3 = 12288，支持大规模的渲柕场景
    static constexpr int MAX_DESCRIPTOR_SETS = 4096 * 3;

    // ========================================
    // 管线构造函数实现
    // ========================================
    
    /**
     * 图形管线构造函数实现
     * 
     * 创建一个用于3D渲柕的完整图形管线。这是最复杂的管线类型，
     * 需要配置所有从顶点处理到像素输出的阶段。
     * 
     * 处理流程：
     * 1. 保存传入的配置参数
     * 2. 设置管线类型为图形管线
     * 3. 调用内部创建函数完成复杂的配置
     */
    Pipeline::Pipeline(const Context* context, const GraphicsPipelineDescriptor& desc,
                       VkRenderPass renderPass, const std::string& name)
            : context_(context),                                    // 保存Vulkan上下文
              graphicsPipelineDesc_(desc),                         // 保存图形管线配置
              bindPoint_(VK_PIPELINE_BIND_POINT_GRAPHICS),         // 设置为图形管线类型
              vkRenderPass_(renderPass),                           // 保存渲柕通道
              name_{name} {                                        // 保存调试名称
        // 启动图形管线的创建流程
        createGraphicsPipeline();
    }

    /**
     * 计算管线构造函数实现
     * 
     * 创建一个用于GPU通用计算的管线。比图形管线简单很多，
     * 但功能强大，可以处理各种通用计算任务。
     * 
     * 常见应用：
     * - 图像处理（模糊、锐化、颜色校正）
     * - 数据处理（排序、搜索、统计）
     * - 物理模拟（粒子、流体、布料）
     * - 后处理效果（HDR色调映射、SSAO）
     */
    Pipeline::Pipeline(const Context* context, const ComputePipelineDescriptor& desc,
                       const std::string& name /*= ""*/)
            : context_(context),                            // 保存Vulkan上下文
              computePipelineDesc_(desc),                  // 保存计算管线配置
              bindPoint_(VK_PIPELINE_BIND_POINT_COMPUTE),  // 设置为计算管线类型
              name_{name} {                                // 保存调试名称
        // 启动计算管线的创建流程
        createComputePipeline();
    }

    /**
     * 光线追踪管线构造函数实现
     * 
     * 创建一个用于实时光线追踪渲柕的管线。需要RTX系列或
     * 其他支持光追的高端 GPU。提供了最高质量的光照效果。
     * 
     * 优势：
     * - 物理准确的光照模型
     * - 实时全局光照和反射
     * - 精确的阴影和环境遮蔽
     * - 支持复杂光学现象（折射、散射等）
     */
    Pipeline::Pipeline(const Context* context, const RayTracingPipelineDescriptor& desc,
                       const std::string& name /*= ""*/)
            : context_(context),                                       // 保存Vulkan上下文
              rayTracingPipelineDesc_(desc),                          // 保存光线追踪管线配置
              bindPoint_(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR),     // 设置为光线追踪管线类型
              name_{name} {                                           // 保存调试名称
        // 启动光线追踪管线的创建流程
        createRayTracingPipeline();
    }

    // ========================================
    // 管线析构函数
    // ========================================
    
    /**
     * 管线析构函数实现
     * 
     * 按照RAII原则清理所有管线相关的Vulkan资源。需要按照与Vulkan对象
     * 创建相反的顺序进行销毁，以避免依赖冲突。
     * 
     * 清理顺序说明：
     * 1. 首先销毁管线对象（依赖其他资源）
     * 2. 然后销毁管线布局（依赖描述符布局）
     * 3. 再销毁描述符池（依赖描述符布局）
     * 4. 最后销毁描述符布局（最基础的资源）
     */
    Pipeline::~Pipeline() {
        const auto device = context_->device();  // 获取Vulkan设备句柄

        // 步骤 1: 销毁管线对象（依赖所有其他资源）
        vkDestroyPipeline(device, vkPipeline_, nullptr);
        
        // 步骤 2: 销毁管线布局（依赖描述符布局）
        vkDestroyPipelineLayout(device, vkPipelineLayout_, nullptr);
        
        // 步骤 3: 销毁描述符池（自动释放所有分配的描述符集合）
        vkDestroyDescriptorPool(device, vkDescriptorPool_, nullptr);

        // 步骤 4: 逐个销毁所有描述符集合布局
        for (const auto& set : descriptorSets_) {
            vkDestroyDescriptorSetLayout(device, set.second.vkLayout_, nullptr);
        }
    }

    // ========================================
    // 基础访问方法
    // ========================================
    
    /**
     * 获取底层Vulkan管线对象
     * @return Vulkan管线句柄，用于直接调用Vulkan API
     */
    VkPipeline Pipeline::vkPipeline() const { 
        return vkPipeline_; 
    }

    /**
     * 获取管线布局对象
     * @return 管线布局句柄，定义了资源的组织结构
     */
    VkPipelineLayout Pipeline::vkPipelineLayout() const { 
        return vkPipelineLayout_; 
    }

    // ========================================
    // 管线使用方法
    // ========================================
    
    /**
     * 更新推送常量实现
     * 
     * 推送常量是一种高效的小数据传递机制，直接嵌入GPU命令流。
     * 相比于描述符和缓冲区，推送常量有最低的延迟和最高的更新频率。
     * 
     * 实际应用中的使用场景：
     * - MVP矩阵（Model-View-Projection）
     * - 时间参数和动画数据
     * - 材质ID和小量材质参数
     * - 光照参数和环境设置
     * 
     * @param commandBuffer 目标命令缓冲区
     * @param flags 目标着色器阶段标志
     * @param size 数据大小（字节）
     * @param data 数据指针
     */
    void Pipeline::updatePushConstant(VkCommandBuffer commandBuffer, VkShaderStageFlags flags,
                                      uint32_t size, const void* data) {
        // 直接将数据推送到GPU命令流中
        // 参数说明：命令缓冲区、管线布局、目标阶段、偏移（0）、大小、数据
        vkCmdPushConstants(commandBuffer, vkPipelineLayout_, flags, 0, size, data);
    }

    /**
     * 绑定管线实现
     * 
     * 这是渲柕循环中的关键步骤，告诉GPU使用哪个管线来处理后续的渲柕命令。
     * 一旦绑定，所有的绘制或计算命令都会使用这个管线的设置。
     * 
     * 执行步骤：
     * 1. 将管线绑定到命令缓冲区
     * 2. 提交所有待处理的资源绑定
     * 
     * @param commandBuffer 目标命令缓冲区
     */
    void Pipeline::bind(VkCommandBuffer commandBuffer) {
        // 将管线绑定到指定的命令缓冲区
        // bindPoint_可以是：GRAPHICS、COMPUTE或RAY_TRACING_KHR
        vkCmdBindPipeline(commandBuffer, bindPoint_, vkPipeline_);

        // 提交所有通过bindResource系列函数缓存的资源绑定
        // 这种批量处理方式可以减少Vulkan API调用次数，提高性能
        updateDescriptorSets();
    }

    // ========================================
    // 描述符管理方法
    // ========================================
    
    /**
     * 分配描述符集合实现
     * 
     * 描述符集合必须在使用之前从描述符池中分配。这个函数批量分配
     * 指定数量的描述符集合实例，支持多缓冲和批量渲柕。
     * 
     * 为什么需要多个实例？
     * - 双缓冲/三缓冲：避免在GPU使用时修改资源
     * - 批量渲柕：不同对象使用不同的资源集合
     * - 并行处理：多线程同时录制命令
     * 
     * @param setAndCount 包含集合索引和所需数量的列表
     */
    void Pipeline::allocateDescriptors(const std::vector<SetAndCount>& setAndCount) {
        // 如果描述符池还未创建，先创建一个
        if (vkDescriptorPool_ == VK_NULL_HANDLE) {
            initDescriptorPool();
        }

        // 遍历每个需要分配的集合
        for (auto set : setAndCount) {
            // 验证集合索引是否存在于管线定义中
            // 使用 C++17 兼容的写法替代 C++20 的 contains() 方法
            ASSERT(descriptorSets_.find(set.set_) != descriptorSets_.end(),
                   "This pipeline doesn't have a set with index " + std::to_string(set.set_));

            // 创建描述符集合分配信息
            /**
             * VkDescriptorSetAllocateInfo 结构体详解
             * 
             * 这个结构体用于从描述符池(Descriptor Pool)中分配描述符集合(Descriptor Set)。
             * 描述符集合就像是GPU资源的"地址簿"，告诉着色器如何访问各种资源。
             * 
             * 字段说明：
             * - sType: 结构体类型标识符，用于Vulkan内部识别
             * - pNext: 扩展结构体指针，通常为nullptr
             * - descriptorPool: 描述符池句柄，提供内存分配的来源
             * - descriptorSetCount: 要分配的描述符集合数量
             * - pSetLayouts: 指向描述符集合布局数组的指针，定义集合的结构
             * 
             * 分配过程类比：
             * 想象描述符池是一个"资源管理办公室"，描述符集合是"资源使用许可证"。
             * 这个结构体就是"申请表"，告诉办公室你需要什么类型的许可证。
             */
            const VkDescriptorSetAllocateInfo allocInfo = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // 结构体类型标识
                    .descriptorPool = vkDescriptorPool_,                     // 从哪个池分配
                    .descriptorSetCount = 1,                                // 每次分配一个集合
                    .pSetLayouts = &descriptorSets_[set.set_].vkLayout_,    // 使用的布局模板
            };

            // 按照所需数量循环分配
            for (size_t i = 0; i < set.count_; ++i) {
                VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
                
                // 从描述符池中分配一个新的描述符集合
                VK_CHECK(vkAllocateDescriptorSets(context_->device(), &allocInfo, &descriptorSet));
                
                // 将分配的集合添加到列表中
                descriptorSets_[set.set_].vkSets_.push_back(descriptorSet);

                // 为描述符集合设置调试名称，便于调试和性能分析
                context_->setVkObjectname(descriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                          "Descriptor set: " + set.name_ + " " + std::to_string(i));
            }
        }
    }

    /**
     * 绑定描述符集合实现
     * 
     * 将已分配的描述符集合绑定到命令缓冲区。这告诉GPU在执行后续
     * 的绘制或计算命令时使用哪些资源。
     * 
     * 注意：
     * - 必须在执行绘制/计算命令之前调用
     * - 只绑定需要更改的集合，其他集合保持不变
     * - 支持部分更新，无需每次都绑定所有集合
     * 
     * @param commandBuffer 命令缓冲区
     * @param sets 要绑定的集合列表（集合索引 + 实例索引）
     */
    void Pipeline::bindDescriptorSets(VkCommandBuffer commandBuffer,
                                      const std::vector<SetAndBindingIndex>& sets) {
        // 遍历每个需要绑定的集合
        for (const auto& set : sets) {
            // 绑定单个描述符集合到指定的集合索引位置
            // 参数：命令缓冲区、管线类型、管线布局、集合索引、集合数量(1)、集合指针、动态偏移数量(0)、动态偏移数组(无)
            vkCmdBindDescriptorSets(commandBuffer, bindPoint_, vkPipelineLayout_, set.set, 1u,
                                    &descriptorSets_[set.set].vkSets_[set.bindIdx], 0, nullptr);
        }
    }

    void Pipeline::updateSamplersDescriptorSets(uint32_t set, uint32_t index,
                                                const std::vector<SetBindings>& bindings) {
        ASSERT(!bindings.empty(), "bindings are empty");
        std::vector<std::vector<VkDescriptorImageInfo>> samplerInfo(bindings.size());

        std::vector<VkWriteDescriptorSet> writeDescSets;
        writeDescSets.reserve(bindings.size());

        // 使用 C++17 兼容的 for 循环写法
        size_t idx = 0;
        for (auto& binding : bindings) {
            // 检查采样器指针是否有效
            if (binding.samplers_ && !binding.samplers_->empty()) {
                samplerInfo[idx].reserve(binding.samplers_->size());
                for (const auto& sampler : *binding.samplers_) {
                    /**
                     * VkDescriptorImageInfo 结构体详解
                     * 
                     * 这个结构体描述了图像相关描述符的详细信息，包括采样器、图像视图和布局。
                     * GPU使用这些信息来正确访问和采样纹理。
                     * 
                     * 字段说明：
                     * - sampler: 采样器句柄，定义纹理采样的方式（过滤、寻址模式等）
                     * - imageView: 图像视图句柄，定义如何解释图像数据
                     * - imageLayout: 图像布局，描述图像在内存中的组织方式
                     * 
                     * 常见的使用场景：
                     * - 纹理采样：在片段着色器中访问2D纹理
                     * - 立方体贴图：环境映射和反射效果
                     * - 深度缓冲区：阴影映射和深度测试
                     */
                    samplerInfo[idx].emplace_back(VkDescriptorImageInfo{
                            .sampler = sampler->vkSampler(),    // 采样器句柄（控制纹理采样方式）
                            .imageView = VK_NULL_HANDLE,        // 图像视图（在这里不需要）
                            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED, // 图像布局（纯采样器不需要）
                    });
                }
            }
            /**
             * VkWriteDescriptorSet 结构体详解
             * 
             * 这是描述符系统的核心结构体，用于向描述符集合写入资源信息。
             * 它告诉Vulkan驱动在描述符集合的哪个位置，绑定哪些资源。
             * 
             * 字段说明：
             * - sType: 结构体类型标识符
             * - pNext: 扩展结构体指针（如加速结构信息）
             * - dstSet: 目标描述符集合句柄
             * - dstBinding: 目标绑定点索引（在着色器中的binding编号）
             * - dstArrayElement: 目标数组元素索引（用于数组类型的描述符）
             * - descriptorCount: 要更新的描述符数量
             * - descriptorType: 描述符类型（采样器、纹理、缓冲区等）
             * - pImageInfo: 图像类型描述符的详细信息
             * - pBufferInfo: 缓冲区类型描述符的详细信息
             * - pTexelBufferView: 缓冲区视图类型描述符的信息
             * 
             * 使用原理：
             * 想象描述符集合是一个"地址簿"，这个结构体就是"更新指令"，
             * 告诉系统在地址簿的第X页第Y行写上新的地址信息。
             */
            const VkWriteDescriptorSet writeDescSet = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,        // 结构体类型标识
                    .dstSet = descriptorSets_[set].vkSets_[index],          // 目标描述符集合
                    .dstBinding = binding.binding_,                         // 目标绑定点
                    .dstArrayElement = 0,                                   // 数组起始索引
                    .descriptorCount = static_cast<uint32_t>(samplerInfo[idx].size()), // 描述符数量
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,           // 描述符类型：采样器
                    .pImageInfo = samplerInfo[idx].data(),                  // 图像信息数组
                    .pBufferInfo = nullptr,                                 // 缓冲区信息（不需要）
            };

            writeDescSets.emplace_back(std::move(writeDescSet));
            ++idx;
        }

        vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(),
                               0, nullptr);
    }

    void Pipeline::updateTexturesDescriptorSets(uint32_t set, uint32_t index,
                                                const std::vector<SetBindings>& bindings) {
        ASSERT(!bindings.empty(), "bindings are empty");
        std::vector<std::vector<VkDescriptorImageInfo>> imageInfo(bindings.size());

        std::vector<VkWriteDescriptorSet> writeDescSets;
        writeDescSets.reserve(bindings.size());

        // 使用 C++17 兼容的 for 循环写法
        size_t idx = 0;
        for (auto& binding : bindings) {
            // 检查纹理指针是否有效
            if (binding.textures_ && !binding.textures_->empty()) {
                imageInfo[idx].reserve(binding.textures_->size());
                for (const auto& texture : *binding.textures_) {
                    imageInfo[idx].emplace_back(VkDescriptorImageInfo{
                            .imageView = texture->vkImageView(),
                            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    });
                }
            }
            const VkWriteDescriptorSet writeDescSet = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSets_[set].vkSets_[index],
                    .dstBinding = binding.binding_,
                    .dstArrayElement = 0,
                    .descriptorCount = static_cast<uint32_t>(imageInfo[idx].size()),
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .pImageInfo = imageInfo[idx].data(),
                    .pBufferInfo = nullptr,
            };

            writeDescSets.emplace_back(std::move(writeDescSet));
            ++idx;
        }

        vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(),
                               0, nullptr);
    }

    void Pipeline::updateBuffersDescriptorSets(uint32_t set, uint32_t index,
                                               VkDescriptorType type,
                                               const std::vector<SetBindings>& bindings) {
        ASSERT(!bindings.empty(), "bindings are empty");
        std::vector<VkDescriptorBufferInfo> bufferInfo;
        bufferInfo.reserve(bindings.size());
        std::vector<VkWriteDescriptorSet> writeDescSets;
        writeDescSets.reserve(bindings.size());

        for (auto& binding : bindings) {
            /**
             * VkDescriptorBufferInfo 结构体详解
             * 
             * 这个结构体描述了缓冲区描述符的详细信息，用于告诉GPU如何访问缓冲区中的数据。
             * 缓冲区描述符是最常用的描述符类型之一，用于传递各种类型的数据。
             * 
             * 字段说明：
             * - buffer: Vulkan缓冲区句柄，指向存储数据的内存区域
             * - offset: 缓冲区内的字节偏移，指定数据的起始位置
             * - range: 要访问的数据范围大小（字节）
             * 
             * 常见的使用场景：
             * - Uniform Buffer: 存储着色器参数（如MVP矩阵、光照参数）
             * - Storage Buffer: 存储大量结构化数据（如顶点数组、粒子数据）
             * - Index Buffer: 存储顶点索引，优化网格绘制
             * - Vertex Buffer: 存储顶点属性（位置、法线、纹理坐标）
             * 
             * 数据访问模式：
             * - 只读：着色器只能读取数据（如Uniform Buffer）
             * - 读写：着色器可以修改数据（如Storage Buffer）
             */
            bufferInfo.emplace_back(VkDescriptorBufferInfo{
                    .buffer = binding.buffer->vkBuffer(),  // Vulkan缓冲区句柄
                    .offset = 0,                           // 起始偏移量（从缓冲区开头）
                    .range = binding.bufferBytes           // 数据范围大小（字节）
            });

            const VkWriteDescriptorSet writeDescSet = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSets_[set].vkSets_[index],
                    .dstBinding = binding.binding_,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = type,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &bufferInfo.back(),
            };

            writeDescSets.emplace_back(writeDescSet);
        }

        vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(),
                               0, nullptr);
    }

    // ========================================
    // 资源更新方法
    // ========================================
    
    /**
     * 提交所有待处理的描述符更新实现
     * 
     * 这是一个批量处理函数，将所有通过bindResource函数缓存的资源绑定
     * 一次性提交给GPU。这种设计可以：
     * - 减少API调用次数：一次调用更新所有资源
     * - 提高并发性：支持多线程并行绑定资源
     * - 保证原子性：所有更新同时生效
     * 
     * 调用时机：
     * - 在bind()方法中自动调用
     * - 可以手动调用以在不重新绑定管线的情况下更新资源
     */
    void Pipeline::updateDescriptorSets() {
        // 检查是否有待处理的更新
        if (!writeDescSets_.empty()) {
            // 使用互斥锁保证线程安全（支持多线程并行调用）
            std::unique_lock<std::mutex> mlock(mutex_);
            
            // 批量提交所有的描述符更新操作
            // 参数：Vulkan设备、更新操作数量、更新操作数组、复制操作数量(0)、复制操作数组(无)
            vkUpdateDescriptorSets(context_->device(), writeDescSets_.size(),
                                   writeDescSets_.data(), 0, nullptr);
            
            // 清理所有缓存的数据，为下一次更新做准备
            writeDescSets_.clear();            // 清理更新操作列表
            bufferInfo_.clear();               // 清理缓冲区信息缓存
            bufferViewInfo_.clear();           // 清理缓冲区视图信息缓存
            imageInfo_.clear();                // 清理图像信息缓存
            accelerationStructInfo_.clear();   // 清理加速结构信息缓存
        }
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                std::shared_ptr<Buffer> buffer, uint32_t offset,
                                uint32_t size, VkDescriptorType type, VkFormat format) {
        bufferInfo_.emplace_back(std::vector<VkDescriptorBufferInfo>{VkDescriptorBufferInfo{
                .buffer = buffer->vkBuffer(), .offset = offset, .range = size}});

        if (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
            type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
            ASSERT(format != VK_FORMAT_UNDEFINED, "format must be specified");
            bufferViewInfo_.emplace_back(buffer->requestBufferView(format));
        }

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = type,
                .pImageInfo = nullptr,
                .pBufferInfo = (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                                type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                               ? VK_NULL_HANDLE
                               : bufferInfo_.back().data(),
                .pTexelBufferView = (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                                     type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                                    ? &bufferViewInfo_.back()
                                    : VK_NULL_HANDLE,
        };

        writeDescSets_.emplace_back(std::move(writeDescSet));
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                const std::vector<std::shared_ptr<Texture>>& textures,
                                std::shared_ptr<Sampler> sampler, uint32_t dstArrayElement) {
        if (textures.size() == 0) {
            return;
        }

        std::unique_lock<std::mutex> mlock(mutex_);

        imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
        imageInfo_.back().reserve(textures.size());
        for (const auto& texture : textures) {
            if (texture) {
                imageInfo_.back().emplace_back(VkDescriptorImageInfo{
                        .sampler = sampler ? sampler->vkSampler() : VK_NULL_HANDLE,
                        .imageView = texture->vkImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                });
            }
        }

        if (imageInfo_.back().size() == 0) {
            return;
        }

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = dstArrayElement,
                .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
                .descriptorType = sampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                          : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo = imageInfo_.back().data(),
                .pBufferInfo = nullptr,
        };

        writeDescSets_.emplace_back(std::move(writeDescSet));
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                const std::vector<std::shared_ptr<Sampler>>& samplers) {
        imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
        imageInfo_.back().reserve(samplers.size());
        for (const auto& sampler : samplers) {
            imageInfo_.back().emplace_back(VkDescriptorImageInfo{
                    .sampler = sampler->vkSampler(),
            });
        }

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo = imageInfo_.back().data(),
                .pBufferInfo = nullptr,
        };

        writeDescSets_.emplace_back(writeDescSet);
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                std::vector<std::shared_ptr<Buffer>> buffers,
                                VkDescriptorType type) {
        std::vector<VkDescriptorBufferInfo> bufferInfos;

        for (auto& buffer : buffers) {
            bufferInfos.emplace_back(VkDescriptorBufferInfo{
                    .buffer = buffer->vkBuffer(),
                    .offset = 0,
                    .range = buffer->size(),
            });
        }

        bufferInfo_.emplace_back(bufferInfos);

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = uint32_t(bufferInfos.size()),
                .descriptorType = type,
                .pImageInfo = nullptr,
                .pBufferInfo = bufferInfo_.back().data(),
        };

        writeDescSets_.emplace_back(std::move(writeDescSet));
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                std::shared_ptr<Texture> texture, VkDescriptorType type) {
        imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
        imageInfo_.back().push_back(VkDescriptorImageInfo{
                .imageView = texture->vkImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        });

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
                .descriptorType = type,
                .pImageInfo = imageInfo_.back().data(),
                .pBufferInfo = nullptr,
        };

        writeDescSets_.emplace_back(writeDescSet);
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                const std::vector<std::shared_ptr<VkImageView>>& imageViews,
                                VkDescriptorType type) {
        imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
        imageInfo_.back().reserve(imageViews.size());
        for (const auto& imview : imageViews) {
            imageInfo_.back().emplace_back(VkDescriptorImageInfo{
                    .imageView = *imview,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            });
        }

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
                .descriptorType = type,
                .pImageInfo = imageInfo_.back().data(),
                .pBufferInfo = nullptr,
        };

        writeDescSets_.emplace_back(writeDescSet);
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                std::shared_ptr<Texture> texture,
                                std::shared_ptr<Sampler> sampler, VkDescriptorType type) {
        imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
        imageInfo_.back().push_back(VkDescriptorImageInfo{
                .sampler = sampler->vkSampler(),
                .imageView = texture->vkImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        });

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
                .descriptorType = type,
                .pImageInfo = imageInfo_.back().data(),
                .pBufferInfo = nullptr,
        };

        writeDescSets_.emplace_back(writeDescSet);
    }

    void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                                VkAccelerationStructureKHR* accelStructHandle) {
        /**
         * VkWriteDescriptorSetAccelerationStructureKHR 结构体详解
         * 
         * 这个结构体是光线追踪扩展的一部分，用于将加速结构绑定到描述符集合。
         * 加速结构是光线追踪的核心数据结构，存储了场景的几何信息。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符（KHR扩展）
         * - pNext: 扩展结构体指针（通常为null）
         * - accelerationStructureCount: 加速结构数量
         * - pAccelerationStructures: 指向加速结构句柄数组的指针
         * 
         * 光线追踪中的加速结构类型：
         * 1. BLAS (Bottom Level Acceleration Structure)：
         *    - 存储单个3D模型的几何信息
         *    - 包含三角形数据、顶点信息
         *    - 用于光线-三角形交点测试
         * 
         * 2. TLAS (Top Level Acceleration Structure)：
         *    - 存储场景中所有对象的层次结构
         *    - 包含对象实例的变换矩阵和材质信息
         *    - 用于全局光线遍历和场景管理
         * 
         * 常见用途：
         * - 实时全局光照：计算间接光照效果
         * - 反射和折射：高质量的镜面和玻璃效果
         * - 阴影映射：精确的软阴影和环境遮蔽
         * - 环境遮蔽：空间音频和全局光照效果
         * 
         * 注意：
         * - 需要RTX 20系列或更高级的GPU
         * - 需要启用VK_KHR_ray_tracing_pipeline扩展
         * - 性能开销较大，适合高端游戏和专业应用
         */
        accelerationStructInfo_.push_back(VkWriteDescriptorSetAccelerationStructureKHR{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, // KHR扩展结构体类型
                .accelerationStructureCount = 1,                                          // 加速结构数量（1个）
                .pAccelerationStructures = accelStructHandle,                             // 加速结构句柄数组
        });

        ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
               "Did you allocate the descriptor set before binding to it?");

        const VkWriteDescriptorSet writeDescSet = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = &accelerationStructInfo_.back(),
                .dstSet = descriptorSets_[set].vkSets_[index],
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = 1u,
                .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        };

        writeDescSets_.emplace_back(writeDescSet);
    }

    // ========================================
    // 顶点数据绑定方法（仅图形管线使用）
    // ========================================
    
    /**
     * 绑定顶点缓冲区实现
     * 
     * 将包含3D模型顶点数据的缓冲区绑定到图形管线。顶点数据包括：
     * - 位置坐标（x, y, z）
     * - 法线向量（用于光照计算）
     * - 纹理坐标（用于纹理映射）
     * - 颜色信息（如果需要）
     * 
     * @param commandBuffer 命令缓冲区
     * @param vertexBuffer 包含顶点数据的Vulkan缓冲区
     */
    void Pipeline::bindVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer) {
        VkBuffer vertexBuffers[1] = {vertexBuffer};   // 顶点缓冲区数组（支持多个缓冲区）
        VkDeviceSize vertexOffset[1] = {0};           // 每个缓冲区的起始偏移（从0开始）
        
        // 绑定顶点缓冲区到管线
        // 参数：命令缓冲区、第一个绑定点(0)、缓冲区数量(1)、缓冲区数组、偏移数组
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexOffset);
    }

    /**
     * 绑定索引缓冲区实现
     * 
     * 索引缓冲区存储顶点的引用索引，用于优化网格存储。
     * 例如一个立方体有8个顶点但需要12个三角形，使用索引可以避免重复存储。
     * 
     * 优点：
     * - 节省内存：避免重复存储相同的顶点
     * - 提高性能：减少顶点着色器的调用次数
     * - 缓存友好：顶点数据重用可以提高缓存命中率
     * 
     * @param commandBuffer 命令缓冲区
     * @param indexBuffer 包含顶点索引的Vulkan缓冲区
     */
    void Pipeline::bindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer indexBuffer) {
        // 绑定索引缓冲区到管线
        // 参数：命令缓冲区、索引缓冲区、起始偏移(0)、索引类型(uint32)
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void Pipeline::createGraphicsPipeline() {
        /**
         * VkSpecializationInfo 结构体详解
         * 
         * 这个结构体用于定义着色器的特化常量(Specialization Constants)。
         * 特化常量是一种高级特性，允许在管线创建时向着色器传递常量值。
         * 
         * 字段说明：
         * - mapEntryCount: 特化常量映射条目数量
         * - pMapEntries: 指向VkSpecializationMapEntry数组的指针
         * - dataSize: 特化数据的总大小（字节）
         * - pData: 指向实际特化数据的指针
         * 
         * 使用优势：
         * 1. 性能优化：编译器可以根据常量值优化代码
         * 2. 代码复用：同一个着色器可以用于不同的配置
         * 3. 动态配置：运行时决定着色器的行为
         * 
         * 常见应用场景：
         * - 光照模型切换：通过常量启用/禁用不同的光照算法
         * - 纹理单元数量：根据硬件能力调整纹理采样次数
         * - 材质类型：不同材质使用不同的着色算法
         * - 调试模式：开发时启用额外的调试信息输出
         */
        const VkSpecializationInfo vertexSpecializationInfo{
                .mapEntryCount =
                static_cast<uint32_t>(graphicsPipelineDesc_.vertexSpecConstants_.size()), // 顶点着色器特化常量数量
                .pMapEntries = graphicsPipelineDesc_.vertexSpecConstants_.data(),         // 特化常量映射表
                .dataSize = !graphicsPipelineDesc_.vertexSpecConstants_.empty()           // 特化数据的总大小
                            ? graphicsPipelineDesc_.vertexSpecConstants_.back().offset +
                              graphicsPipelineDesc_.vertexSpecConstants_.back().size
                            : 0,
                .pData = graphicsPipelineDesc_.vertexSpecializationData,
        };

        const VkSpecializationInfo fragmentSpecializationInfo{
                .mapEntryCount =
                static_cast<uint32_t>(graphicsPipelineDesc_.fragmentSpecConstants_.size()),
                .pMapEntries = graphicsPipelineDesc_.fragmentSpecConstants_.data(),
                .dataSize = !graphicsPipelineDesc_.fragmentSpecConstants_.empty()
                            ? graphicsPipelineDesc_.fragmentSpecConstants_.back().offset +
                              graphicsPipelineDesc_.fragmentSpecConstants_.back().size
                            : 0,
                .pData = graphicsPipelineDesc_.fragmentSpecializationData,
        };

        const auto vertShader = graphicsPipelineDesc_.vertexShader_.lock();
        ASSERT(vertShader,
               "Vertex's ShaderModule has been destroyed before being used to create "
               "a pipeline");
        const auto fragShader = graphicsPipelineDesc_.fragmentShader_.lock();
        ASSERT(fragShader,
               "Vertex's ShaderModule has been destroyed before being used to create "
               "a pipeline");
        /**
         * VkPipelineShaderStageCreateInfo 结构体详解
         * 
         * 这个结构体用于定义渲柕管线中的一个着色器阶段。
         * 渲柕管线可以包含多个着色器阶段，每个阶段负责不同的处理任务。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - flags: 创建标志（通常为0）
         * - stage: 着色器阶段类型（顶点、片段、计算等）
         * - module: 着色器模块句柄，包含SPIR-V字节码
         * - pName: 着色器入口函数名（通常是"main"）
         * - pSpecializationInfo: 特化常量信息（可选）
         * 
         * 不同着色器阶段的作用：
         * 1. 顶点着色器 (Vertex Shader)：
         *    - 处理每个顶点的数据（位置、法线、纹理坐标）
         *    - 执行坐标变换（模型空间 → 世界空间 → 观察空间 → 裁剪空间）
         *    - 计算顶点光照（如果使用Gouraud着色）
         * 
         * 2. 片段着色器 (Fragment Shader)：
         *    - 处理每个像素的颜色计算
         *    - 执行纹理采样和过滤
         *    - 应用光照模型（如Phong、PBR）
         *    - 处理透明度和特效
         * 
         * 3. 计算着色器 (Compute Shader)：
         *    - 执行通用的并行计算任务
         *    - 不属于图形渲柕管线，使用独立的计算管线
         */
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
                // 顶点着色器阶段配置
                VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  // 结构体类型
                        .stage = vertShader->vkShaderStageFlags(),                    // 顶点着色器阶段
                        .module = vertShader->vkShaderModule(),                      // SPIR-V着色器模块
                        .pName = vertShader->entryPoint().c_str(),                   // 入口函数名
                        .pSpecializationInfo = !graphicsPipelineDesc_.vertexSpecConstants_.empty()
                                               ? &vertexSpecializationInfo           // 特化常量信息
                                               : nullptr,                            // 无特化常量
                },
                // 片段着色器阶段配置  
                VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  // 结构体类型
                        .stage = fragShader->vkShaderStageFlags(),                    // 片段着色器阶段
                        .module = fragShader->vkShaderModule(),                      // SPIR-V着色器模块
                        .pName = fragShader->entryPoint().c_str(),                   // 入口函数名
                        .pSpecializationInfo = !graphicsPipelineDesc_.fragmentSpecConstants_.empty()
                                               ? &fragmentSpecializationInfo         // 特化常量信息
                                               : nullptr,                            // 无特化常量
                },
        };

        /**
         * VkPipelineVertexInputStateCreateInfo 结构体详解
         * 
         * 这个结构体定义了顶点输入的格式和组织方式。
         * 它告诉GPU如何解释从顶点缓冲区中读取的数据。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - vertexBindingDescriptionCount: 顶点绑定描述数量
         * - pVertexBindingDescriptions: 顶点绑定描述数组
         * - vertexAttributeDescriptionCount: 顶点属性描述数量
         * - pVertexAttributeDescriptions: 顶点属性描述数组
         * 
         * 顶点数据组织方式：
         * 1. 交错存储 (Interleaved)：所有属性在一个缓冲区中按顺序排列
         *    示例：[位置1, 法线1, UV1, 位置2, 法线2, UV2, ...]
         * 2. 分离存储 (Separate)：不同属性在不同缓冲区中
         *    示例：缓冲区1[位置1, 位置2, ...], 缓冲区2[法线1, 法线2, ...]
         */
        const VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,  // 结构体类型
                .vertexBindingDescriptionCount = 0,                                 // 顶点绑定描述数量（0 = 无顶点数据）
                .vertexAttributeDescriptionCount = 0,                               // 顶点属性描述数量（0 = 无属性）
        };

        /**
         * VkPipelineInputAssemblyStateCreateInfo 结构体详解
         * 
         * 这个结构体定义了如何将顶点数据组装成图元（点、线、三角形）。
         * 这是图形渲柕管线中的图元装配阶段。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - topology: 原语拓扑类型，定义顶点如何组成图元
         * - primitiveRestartEnable: 是否启用原语重启
         * 
         * 常见的拓扑类型：
         * - VK_PRIMITIVE_TOPOLOGY_POINT_LIST: 点列表，每个顶点是一个点
         * - VK_PRIMITIVE_TOPOLOGY_LINE_LIST: 线段列表，每两个顶点构成一条线
         * - VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: 三角形列表，每三个顶点构成一个三角形
         * - VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: 三角形带，共享顶点的三角形
         * 
         * 应用场景：
         * - 点：粒子系统、星空渲柕
         * - 线：线框模式、调试可视化
         * - 三角形：大多数3D模型渲柕
         */
        const VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,  // 结构体类型
                .topology = graphicsPipelineDesc_.primitiveTopology,                  // 原语拓扑类型
                .primitiveRestartEnable = VK_FALSE,                                   // 禁用原语重启
        };

        // viewport & scissor stuff
        const VkViewport viewport = graphicsPipelineDesc_.viewport.toVkViewPort();

        const VkRect2D scissor = {
                .offset =
                        {
                                .x = 0,
                                .y = 0,
                        },
                .extent = graphicsPipelineDesc_.viewport.toVkExtents(),
        };

        /**
         * VkPipelineViewportStateCreateInfo 结构体详解
         * 
         * 这个结构体定义了视口和裁剪矩形的配置。
         * 视口定义了3D场景在屏幕上显示的区域，裁剪矩形定义了有效的渲柕区域。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - viewportCount: 视口数量（支持多视口渲柕）
         * - pViewports: 指向VkViewport结构体数组的指针
         * - scissorCount: 裁剪矩形数量
         * - pScissors: 指向VkRect2D结构体数组的指针
         * 
         * 视口和裁剪的区别：
         * - 视口：将3D坐标转换为屏幕坐标，包括深度范围
         * - 裁剪：丸纯的像素级别裁剪，超出范围的像素被丢弃
         * 
         * 常见应用：
         * - 全屏渲柕：视口 = 整个屏幕，裁剪 = 整个屏幕
         * - 分屏渲柕：多个视口对应不同的屏幕区域
         * - UI窗口：裁剪用于限制绘制在指定窗口内
         */
        const VkPipelineViewportStateCreateInfo viewportState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,  // 结构体类型
                .viewportCount = 1,                                             // 使用一个视口
                .pViewports = &viewport,                                        // 视口配置
                .scissorCount = 1,                                              // 使用一个裁剪矩形
                .pScissors = &scissor,                                          // 裁剪矩形配置
        };

        /**
         * VkPipelineRasterizationStateCreateInfo 结构体详解
         * 
         * 这个结构体控制光栅化阶段的行为，即将矢量图元转换为像素的过程。
         * 光栅化是图形渲柕管线中的固定功能阶段之一。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - depthClampEnable: 是否启用深度限制（不丢弃超出范围的片段）
         * - rasterizerDiscardEnable: 是否禁用光栅化（输出空白）
         * - polygonMode: 多边形填充模式（填充、线框、点）
         * - cullMode: 裁剪模式（前面、背面、不裁剪）
         * - frontFace: 正面定义（顺时针或逆时针）
         * - depthBiasEnable: 是否启用深度偏移
         * - lineWidth: 线段宽度（必须为1.0，除非启用相关特性）
         * 
         * 裁剪模式详解：
         * - VK_CULL_MODE_NONE: 不裁剪，渲柕所有面（性能较低）
         * - VK_CULL_MODE_FRONT_BIT: 裁剪正面，只渲柕背面
         * - VK_CULL_MODE_BACK_BIT: 裁剪背面，只渲柕正面（最常用）
         * 
         * 常见应用：
         * - 填充模式：正常的3D模型渲柕
         * - 线框模式：网格显示、调试模式
         * - 背面裁剪：提高性能，面片数减半
         */
        const VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,  // 结构体类型
                .depthClampEnable = VK_FALSE,                                        // 禁用深度限制
                .rasterizerDiscardEnable = VK_FALSE,                                 // 启用光栅化输出
                .polygonMode = VK_POLYGON_MODE_FILL,                                 // 多边形填充模式
                .cullMode = VkCullModeFlags(graphicsPipelineDesc_.cullMode),         // 裁剪模式（通常是背面裁剪）
                .frontFace = graphicsPipelineDesc_.frontFace,                        // 正面定义（通常是逆时针）
                .depthBiasEnable = VK_FALSE,                                         // 禁用深度偏移
                .depthBiasConstantFactor = 0.0f,                                     // 深度偏移常数（未使用）
                .depthBiasClamp = 0.0f,                                              // 深度偏移限制（未使用）
                .depthBiasSlopeFactor = 0.0f,                                        // 深度偏移斜率（未使用）
                .lineWidth = 1.0f,                                                   // 线段宽度（标准宽度）
        };

        /**
         * VkPipelineMultisampleStateCreateInfo 结构体详解
         * 
         * 这个结构体控制多采样抗锯齿(Multisampling Anti-Aliasing, MSAA)的设置。
         * 多采样是一种抗锯齿技术，通过在每个像素位置采样多个点来消除锢齿。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - rasterizationSamples: 每个像素的采样数量（1x, 2x, 4x, 8x, 16x）
         * - sampleShadingEnable: 是否启用采样着色（per-sample shading）
         * - minSampleShading: 最小采样着色比例（0.0-1.0）
         * - pSampleMask: 采样掩码，用于控制哪些采样点有效
         * - alphaToCoverageEnable: Alpha-to-Coverage功能（用于透明抗锯齿）
         * - alphaToOneEnable: 强制Alpha值为1.0
         * 
         * MSAA级别对比：
         * - 1x MSAA: 无抗锯齿，最快但有锢齿
         * - 2x MSAA: 轻度抗锯齿，性能影响较小
         * - 4x MSAA: 中等抗锯齿，性能和质量的平衡
         * - 8x MSAA: 高质量抗锯齿，性能影响显著
         * - 16x MSAA: 极高质量，性能开销很大
         * 
         * 适用场景：
         * - 移动设备：通常使用2x-4x MSAA
         * - 桌面游戏：4x-8x MSAA
         * - 专业应用：8x-16x MSAA
         */
        const VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,  // 结构体类型
                .rasterizationSamples = graphicsPipelineDesc_.sampleCount,          // 采样数量（来自管线配置）
                .sampleShadingEnable = VK_FALSE,                                    // 禁用per-sample着色（较慢）
                .minSampleShading = 1.0f,                                           // 最小采样着色比例（未使用）
                .pSampleMask = nullptr,                                             // 采样掩码（未使用）
                .alphaToCoverageEnable = VK_FALSE,                                  // 禁用Alpha-to-Coverage
                .alphaToOneEnable = VK_FALSE,                                       // 禁用Alpha-to-One
        };

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;

        if (graphicsPipelineDesc_.blendAttachmentStates_.size() > 0) {
            ASSERT(graphicsPipelineDesc_.blendAttachmentStates_.size() ==
                   graphicsPipelineDesc_.colorTextureFormats.size(),
                   "Blend states need to be provided for all color textures");
            colorBlendAttachments = graphicsPipelineDesc_.blendAttachmentStates_;
        } else {
            colorBlendAttachments = std::vector<VkPipelineColorBlendAttachmentState>(
                    graphicsPipelineDesc_.colorTextureFormats.size(),
                    {
                            .blendEnable = graphicsPipelineDesc_.blendEnable,
                            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // Optional
                            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // Optional
                            .colorBlendOp = VK_BLEND_OP_ADD,                             // Optional
                            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // Optional
                            .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,            // Optional
                            .alphaBlendOp = VK_BLEND_OP_ADD,                             // Optional
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                    });
        }

        /**
         * VkPipelineColorBlendStateCreateInfo 结构体详解
         * 
         * 这个结构体控制颜色混合（Color Blending）的设置。
         * 颜色混合是图形渲柕管线的最后一个阶段，用于处理透明度和多层渲柕。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - logicOpEnable: 是否启用逻辑操作模式（与混合模式互斥）
         * - logicOp: 逻辑操作类型（如与、或、非等）
         * - attachmentCount: 颜色附件数量（对应多目标渲柕）
         * - pAttachments: 指向混合附件状态数组的指针
         * - blendConstants: 混合常数值（用于某些混合公式）
         * 
         * 混合公式原理：
         * 最终颜色 = (src颜色 * src因子) 运算符 (dst颜色 * dst因子)
         * 
         * 常见混合模式：
         * 1. Alpha混合（透明度）：
         *    - srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA
         *    - dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
         *    - 用于透明物体、UI元素等
         * 
         * 2. 加法混合（发光效果）：
         *    - srcColorBlendFactor = VK_BLEND_FACTOR_ONE
         *    - dstColorBlendFactor = VK_BLEND_FACTOR_ONE
         *    - 用于粒子系统、光晕效果
         * 
         * 3. 乘法混合（阴影效果）：
         *    - srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR
         *    - dstColorBlendFactor = VK_BLEND_FACTOR_ZERO
         *    - 用于阴影映射、颜色调制
         */
        const VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,  // 结构体类型
                .logicOpEnable = VK_FALSE,                                          // 禁用逻辑操作（使用混合模式）
                .logicOp = VK_LOGIC_OP_COPY,                                        // 逻辑操作类型（未使用）
                .attachmentCount = uint32_t(colorBlendAttachments.size()),          // 颜色附件数量
                .pAttachments = colorBlendAttachments.data(),                       // 混合附件状态数组
                .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},                         // 混合常数（未使用）
        };

        // Descriptor Set
        initDescriptorLayout();

        // End of descriptor set layout
        // TODO: does order matter in descSetLayouts? YES!
        std::vector<VkDescriptorSetLayout> descSetLayouts(descriptorSets_.size());
        for (const auto& set : descriptorSets_) {
            descSetLayouts[set.first] = set.second.vkLayout_;
        }

        vkPipelineLayout_ =
                createPipelineLayout(descSetLayouts, graphicsPipelineDesc_.pushConstants_);

        /**
         * VkPipelineDepthStencilStateCreateInfo 结构体详解
         * 
         * 这个结构体控制深度测试和模板测试的设置。
         * 深度测试是3D渲柕中至关重要的功能，用于正确处理物体之间的遭挡关系。
         * 
         * 字段说明：
         * - sType: 结构体类型标识符
         * - depthTestEnable: 是否启用深度测试
         * - depthWriteEnable: 是否允许写入深度缓冲区
         * - depthCompareOp: 深度比较操作（小于、大于、等于等）
         * - depthBoundsTestEnable: 是否启用深度范围测试
         * - stencilTestEnable: 是否启用模板测试
         * - front/back: 正面和背面的模板操作设置
         * - minDepthBounds/maxDepthBounds: 深度范围测试的范围
         * 
         * 深度测试原理：
         * 1. 对于每个像素，比较新片段的深度值与深度缓冲区中的值
         * 2. 按照depthCompareOp进行比较（如VK_COMPARE_OP_LESS）
         * 3. 如果测试通过，更新颜色缓冲区和深度缓冲区
         * 4. 如果测试失败，丢弃该片段
         * 
         * 常见深度比较操作：
         * - VK_COMPARE_OP_LESS: 新片段深度 < 缓冲区深度（最常用，近物体覆盖远物体）
         * - VK_COMPARE_OP_LESS_OR_EQUAL: 新片段深度 <= 缓冲区深度
         * - VK_COMPARE_OP_GREATER: 新片段深度 > 缓冲区深度（反向z）
         * - VK_COMPARE_OP_ALWAYS: 总是通过（禁用深度测试）
         * - VK_COMPARE_OP_NEVER: 从不通过（全部丢弃）
         * 
         * 模板测试应用：
         * - 阴影体渲柕：标记阴影区域
         * - 反射效果：标记反射表面
         * - 轮廓渲柕：先绘制轮廓，再填充内部
         * - 遮罩效果：复杂的几何遮罩
         */
        const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,  // 结构体类型
                .depthTestEnable = graphicsPipelineDesc_.depthTestEnable,             // 深度测试启用状态
                .depthWriteEnable = graphicsPipelineDesc_.depthWriteEnable,           // 深度写入启用状态
                .depthCompareOp = graphicsPipelineDesc_.depthCompareOperation,        // 深度比较操作（通常是LESS）
                .depthBoundsTestEnable = VK_FALSE,                                   // 禁用深度范围测试
                .stencilTestEnable = VK_FALSE,                                       // 禁用模板测试
                .front = {},                                                         // 正面模板操作（空）
                .back = {},                                                          // 背面模板操作（空）
                .minDepthBounds = 0.0f,                                              // 最小深度范围（未使用）
                .maxDepthBounds = 1.0f,                                              // 最大深度范围（未使用）
        };

        /**
         * VkPipelineDynamicStateCreateInfo 结构体详解
         * 
         * 这个结构体定义了哪些管线状态可以在运行时动态更改。
         * 动态状态允许在不重新创建管线的情况下更改某些设置。
         * 
         * 常见动态状态：
         * - VK_DYNAMIC_STATE_VIEWPORT: 视口设置
         * - VK_DYNAMIC_STATE_SCISSOR: 裁剪矩形
         * - VK_DYNAMIC_STATE_LINE_WIDTH: 线段宽度
         * - VK_DYNAMIC_STATE_DEPTH_BIAS: 深度偏移
         * - VK_DYNAMIC_STATE_BLEND_CONSTANTS: 混合常数
         */
        const VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,      // 结构体类型
                .dynamicStateCount =
                static_cast<uint32_t>(graphicsPipelineDesc_.dynamicStates_.size()), // 动态状态数量
                .pDynamicStates = graphicsPipelineDesc_.dynamicStates_.data(),       // 动态状态数组
        };

        /**
         * VkPipelineRenderingCreateInfo 结构体详解
         * 
         * 这个结构体用于动态渲柕(Dynamic Rendering)功能。
         * 动态渲柕是Vulkan 1.3的新特性，允许在不使用VkRenderPass的情况下进行渲柕。
         * 这简化了API使用，提高了灵活性。
         * 
         * 传统渲柕 vs 动态渲柕：
         * - 传统：需要预先创建VkRenderPass和VkFramebuffer
         * - 动态：直接指定附件格式，运行时绑定图像
         */
        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,                       // 结构体类型
                .colorAttachmentCount = uint32_t(graphicsPipelineDesc_.colorTextureFormats.size()), // 颜色附件数量
                .pColorAttachmentFormats = graphicsPipelineDesc_.colorTextureFormats.data(),     // 颜色附件格式数组
                .depthAttachmentFormat = graphicsPipelineDesc_.depthTextureFormat,               // 深度附件格式
                .stencilAttachmentFormat = graphicsPipelineDesc_.stencilTextureFormat,           // 模板附件格式
        };

        /**
         * VkGraphicsPipelineCreateInfo 结构体详解
         * 
         * 这是图形渲柕管线的主结构体，整合了所有前面定义的子结构体。
         * 它定义了从顶点数据到最终像素的整个渲柕流程。
         * 
         * 图形渲柕管线的完整阶段：
         * 1. 顶点输入 (Vertex Input): 定义顶点数据格式
         * 2. 顶点着色器 (Vertex Shader): 变换顶点坐标
         * 3. 图元装配 (Input Assembly): 组装图元（点、线、三角形）
         * 4. 视口变换 (Viewport): 将3D坐标映射到屏幕坐标
         * 5. 光栅化 (Rasterization): 将矢量图元转换为像素
         * 6. 片段着色器 (Fragment Shader): 计算像素颜色
         * 7. 深度测试 (Depth Test): 处理物体遭挡
         * 8. 颜色混合 (Color Blending): 处理透明度和混合
         * 9. 输出合并 (Output Merger): 写入最终像素
         * 
         * 性能优化要点：
         * - 管线状态对象(PSO)是预编译的，创建后不能修改
         * - GPU可以对整个管线进行深度优化
         * - 动态状态可以在不重新创建管线的情况下修改
         */
        const VkGraphicsPipelineCreateInfo pipelineInfo = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,           // 结构体类型 
                .pNext = graphicsPipelineDesc_.useDynamicRendering_                 // 扩展结构体链
                         ? &pipelineRenderingCreateInfo                         // 使用动态渲柕
                         : nullptr,                                             // 使用传统渲柕通道
                .stageCount = uint32_t(shaderStages.size()),
                .pStages = shaderStages.data(),
                .pVertexInputState = &graphicsPipelineDesc_.vertexInputCreateInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depthStencilState,  // Optional
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = vkPipelineLayout_,
                .renderPass = vkRenderPass_,
                .basePipelineHandle = VK_NULL_HANDLE,  // Optional
                .basePipelineIndex = -1,               // Optional
        };

        VK_CHECK(vkCreateGraphicsPipelines(context_->device(), VK_NULL_HANDLE, 1, &pipelineInfo,
                                           nullptr, &vkPipeline_));

        context_->setVkObjectname(vkPipeline_, VK_OBJECT_TYPE_PIPELINE,
                                  "Graphics pipeline: " + name_);
    }  // namespace VulkanCore

    void Pipeline::createComputePipeline() {
        const auto computeShader = computePipelineDesc_.computeShader_.lock();
        ASSERT(computeShader,
               "Compute's ShaderModule has been destroyed before being used to create "
               "a pipeline");

        const VkSpecializationInfo specializationInfo{
                .mapEntryCount =
                static_cast<uint32_t>(computePipelineDesc_.specializationConsts_.size()),
                .pMapEntries = computePipelineDesc_.specializationConsts_.data(),
                .dataSize = !computePipelineDesc_.specializationConsts_.empty()
                            ? computePipelineDesc_.specializationConsts_.back().offset +
                              computePipelineDesc_.specializationConsts_.back().size
                            : 0,
                .pData = computePipelineDesc_.specializationData_,
        };

        initDescriptorLayout();

        std::vector<VkDescriptorSetLayout> descSetLayouts;
        for (const auto& set : descriptorSets_) {
            descSetLayouts.push_back(set.second.vkLayout_);
        }

        vkPipelineLayout_ =
                createPipelineLayout(descSetLayouts, computePipelineDesc_.pushConstants_);

        VkPipelineShaderStageCreateInfo shaderStage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = computeShader->vkShaderStageFlags(),
                .module = computeShader->vkShaderModule(),
                .pName = computeShader->entryPoint().c_str(),
        };

        VkComputePipelineCreateInfo computePipelineCreateInfo{
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .flags = 0,
                .stage = shaderStage,
                .layout = vkPipelineLayout_,
        };
        VK_CHECK(vkCreateComputePipelines(context_->device(), VK_NULL_HANDLE, 1,
                                          &computePipelineCreateInfo, VK_NULL_HANDLE,
                                          &vkPipeline_));
        context_->setVkObjectname(vkPipeline_, VK_OBJECT_TYPE_PIPELINE,
                                  "Compute pipeline: " + name_);
    }

    void Pipeline::createRayTracingPipeline() {
        initDescriptorLayout();

        std::vector<VkDescriptorSetLayout> descSetLayouts;
        for (const auto& set : descriptorSets_) {
            descSetLayouts.push_back(set.second.vkLayout_);
        }

        vkPipelineLayout_ =
                createPipelineLayout(descSetLayouts, rayTracingPipelineDesc_.pushConstants_);

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

        const auto rayGenShader = rayTracingPipelineDesc_.rayGenShader_.lock();

        VkPipelineShaderStageCreateInfo rayGenShaderInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = rayGenShader->vkShaderStageFlags(),
                .module = rayGenShader->vkShaderModule(),
                .pName = rayGenShader->entryPoint().c_str(),
        };

        shaderStages.push_back(rayGenShaderInfo);

        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = static_cast<uint32_t>(shaderStages.size()) - 1,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
        };

        shaderGroups.push_back(shaderGroup);

        for (auto& rayMissShader : rayTracingPipelineDesc_.rayMissShaders_) {
            const auto rayMissShaderPtr = rayMissShader.lock();

            VkPipelineShaderStageCreateInfo rayMissShaderInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = rayMissShaderPtr->vkShaderStageFlags(),
                    .module = rayMissShaderPtr->vkShaderModule(),
                    .pName = rayMissShaderPtr->entryPoint().c_str(),
            };

            shaderStages.push_back(rayMissShaderInfo);

            VkRayTracingShaderGroupCreateInfoKHR shaderGroup{
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                    .generalShader = static_cast<uint32_t>(shaderStages.size()) - 1,
                    .closestHitShader = VK_SHADER_UNUSED_KHR,
                    .anyHitShader = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR,
            };

            shaderGroups.push_back(shaderGroup);
        }

        for (auto& rayClosestHitShader : rayTracingPipelineDesc_.rayClosestHitShaders_) {
            const auto rayClosestHitShaderPtr = rayClosestHitShader.lock();

            VkPipelineShaderStageCreateInfo rayClosestHitShaderInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = rayClosestHitShaderPtr->vkShaderStageFlags(),
                    .module = rayClosestHitShaderPtr->vkShaderModule(),
                    .pName = rayClosestHitShaderPtr->entryPoint().c_str(),
            };

            shaderStages.push_back(rayClosestHitShaderInfo);

            VkRayTracingShaderGroupCreateInfoKHR shaderGroup{
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                    .generalShader = VK_SHADER_UNUSED_KHR,
                    .closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1,
                    .anyHitShader = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR,
            };

            shaderGroups.push_back(shaderGroup);
        }

        VkRayTracingPipelineCreateInfoKHR rayTracingPipelineInfo{
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                .stageCount = static_cast<uint32_t>(shaderStages.size()),
                .pStages = shaderStages.data(),
                .groupCount = static_cast<uint32_t>(shaderGroups.size()),
                .pGroups = shaderGroups.data(),
                .maxPipelineRayRecursionDepth = 10,
                .layout = vkPipelineLayout_,
        };
        VK_CHECK(vkCreateRayTracingPipelinesKHR(context_->device(), VK_NULL_HANDLE,
                                                VK_NULL_HANDLE, 1, &rayTracingPipelineInfo,
                                                nullptr, &vkPipeline_));

        context_->setVkObjectname(vkPipeline_, VK_OBJECT_TYPE_PIPELINE,
                                  "RayTracing pipeline: " + name_);
    }

    VkPipelineLayout Pipeline::createPipelineLayout(
            const std::vector<VkDescriptorSetLayout>& descLayouts,
            const std::vector<VkPushConstantRange>& pushConsts) const {
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = (uint32_t)descLayouts.size(),
                .pSetLayouts = descLayouts.data(),
                .pushConstantRangeCount =
                !pushConsts.empty() ? static_cast<uint32_t>(pushConsts.size()) : 0,
                .pPushConstantRanges = !pushConsts.empty() ? pushConsts.data() : nullptr,
        };

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VK_CHECK(vkCreatePipelineLayout(context_->device(), &pipelineLayoutInfo, nullptr,
                                        &pipelineLayout));
        context_->setVkObjectname(pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                  "pipeline layout: " + name_);

        return pipelineLayout;
    }

    void Pipeline::initDescriptorPool() {
        std::vector<SetDescriptor> sets;

        if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
            sets = graphicsPipelineDesc_.sets_;
        } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_COMPUTE) {
            sets = computePipelineDesc_.sets_;
        } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR) {
            sets = rayTracingPipelineDesc_.sets_;
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        // 使用 C++17 兼容的 for 循环写法
        size_t setIndex = 0;
        for (const auto& set : sets) {
            for (const auto& binding : set.bindings_) {
                poolSizes.push_back({binding.descriptorType, MAX_DESCRIPTOR_SETS});
            }
            ++setIndex; // 递增索引（如果需要使用）
        }

        const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                         VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = MAX_DESCRIPTOR_SETS,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        };
        VK_CHECK(vkCreateDescriptorPool(context_->device(), &descriptorPoolInfo, nullptr,
                                        &vkDescriptorPool_));
        context_->setVkObjectname(vkDescriptorPool_, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                                  "Graphics pipeline descriptor pool: " + name_);
    }

    void Pipeline::initDescriptorLayout() {
        std::vector<SetDescriptor> sets;

        if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
            sets = graphicsPipelineDesc_.sets_;
        } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_COMPUTE) {
            sets = computePipelineDesc_.sets_;
        } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR) {
            sets = rayTracingPipelineDesc_.sets_;
        }
        constexpr VkDescriptorBindingFlags flagsToEnable =
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
        /* | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT; */  // This is disabled
        // because the feature
        // is disabled. See
        // Context::createDefaultFeatureChain
        // 使用 C++17 兼容的 for 循环写法
        size_t setIndex = 0;
        for (const auto& set : sets) {
            std::vector<VkDescriptorBindingFlags> bindFlags(set.bindings_.size(), flagsToEnable);
            /* this won't work for android */
            const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                    .pNext = nullptr,
                    .bindingCount = static_cast<uint32_t>(set.bindings_.size()),
                    .pBindingFlags = bindFlags.data(),
            };
            /* end of not working for android */

            const VkDescriptorSetLayoutCreateInfo dslci = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    /* the next two lines won't work for android */
#if defined(_WIN32)
                    .pNext = &extendedInfo,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
#endif
                    /* end of not working for android*/
                    .bindingCount = static_cast<uint32_t>(set.bindings_.size()),
                    .pBindings = !set.bindings_.empty() ? set.bindings_.data() : nullptr,
            };

            VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
            VK_CHECK(vkCreateDescriptorSetLayout(context_->device(), &dslci, nullptr,
                                                 &descriptorSetLayout));
            context_->setVkObjectname(descriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                                      "Graphics pipeline descriptor set " +
                                      std::to_string(setIndex) + " layout: " + name_);
            ++setIndex; // 递增索引
            descriptorSets_[set.set_].vkLayout_ = descriptorSetLayout;
        }
    }

}  // namespace VkCore

/*
 * ========================================
 * Vulkan渲柕管线系统的总结和学习指南
 * ========================================
 * 
 * 一、渲柕管线的核心概念
 * 
 * 1. 什么是渲柕管线？
 *    渲柕管线是GPU处理图形数据的标准化流程，就像工厂的流水线：
 *    - 原料输入：3D模型数据（顶点、索引、纹理等）
 *    - 加工处理：经过多个阶段的处理（变换、光照、着色等）
 *    - 成品输出：屏幕上的像素和颜色
 * 
 * 2. 为什么需要管线？
 *    - 并行处理：GPU有数千个核心，可以同时处理大量数据
 *    - 灵活性：可编程着色器允许自定义效果
 *    - 性能优化：硬件加速的固定功能阶段
 *    - 标准化：统一的接口处理不同类型的任务
 * 
 * 二、三种管线类型详解
 * 
 * 1. 图形管线 (Graphics Pipeline):
 *    用途：传统的3D渲柕、UI界面、后处理效果
 *    特点：最复杂但最常用，支持完整的图形渲柕管线
 *    阶段：顶点处理 → 图元装配 → 光栅化 → 片段处理 → 输出合并
 * 
 * 2. 计算管线 (Compute Pipeline):
 *    用途：通用GPU计算、图像处理、物理模拟、AI计算
 *    特点：简单但功能强大，只需要一个计算着色器
 *    优势：灵活性高，可以处理任意数据结构和算法
 * 
 * 3. 光线追踪管线 (Ray Tracing Pipeline):
 *    用途：实时光线追踪渲柕、高质量光照和反射
 *    特点：需要RTX等支持光追的GPU，效果最好但要求最高
 *    阶段：光线生成 → 光线遍历 → 命中检测 → 着色计算
 * 
 * 三、渲柕管线的详细阶段
 * 
 * 1. 顶点处理阶段 (Vertex Processing):
 *    - 输入：3D模型的顶点数据（位置、法线、纹理坐标等）
 *    - 处理：坐标变换（世界坐标 → 屏幕坐标）、光照计算
 *    - 输出：屏幕空间中的顶点位置和属性
 * 
 * 2. 图元装配阶段 (Primitive Assembly):
 *    - 功能：将顶点组合成图元（点、线段、三角形）
 *    - 裁剪：移除视口外的图元
 *    - 背面剮除：隐藏看不到的三角形背面
 * 
 * 3. 光栅化阶段 (Rasterization):
 *    - 功能：将矢量图元转换为像素网格
 *    - 输出：每个被图元覆盖的像素及其属性（颜色、深度等）
 *    - 采样：处理抗锯齿（多采样）以消除锯齿
 * 
 * 4. 片段处理阶段 (Fragment Processing):
 *    - 输入：光栅化产生的片段（可理解为"像素候选人"）
 *    - 处理：纹理采样、光照计算、特效处理
 *    - 输出：最终的像素颜色和透明度
 * 
 * 5. 输出合并阶段 (Output Merger):
 *    - 深度测试：判断像素是否被遮挡
 *    - 模板测试：处理复杂的模板操作
 *    - 颜色混合：处理透明和半透明效果
 *    - 多目标渲柕：同时输出到多个渲柕目标
 * 
 * 四、资源管理系统
 * 
 * 1. 描述符系统 (Descriptor System):
 *    - 目的：管理GPU资源的访问方式（纹理、缓冲区、采样器）
 *    - 分层设计：集合 0-3，每个集合包含相关资源
 *    - 优势：高效的批量更新、灵活的资源组合
 * 
 * 2. 推送常量 (Push Constants):
 *    - 特点：极低延迟的小数据传递（通常128字节以内）
 *    - 使用场景：MVP矩阵、时间参数、材质ID等
 *    - 优势：直接嵌入命令流，无需额外的资源分配
 * 
 * 3. 顶点数据管理：
 *    - 顶点缓冲区：存储顶点属性（位置、法线、UV等）
 *    - 索引缓冲区：优化顶点复用，节省内存和带宽
 *    - 多流支持：不同属性可以来自不同缓冲区
 * 
 * 五、性能优化策略
 * 
 * 1. 管线状态优化：
 *    - 状态对象：预编译管线状态，减少运行时开销
 *    - 动态状态：只对频繁变化的状态使用动态设置
 *    - 管线缓存：重用相同配置的管线对象
 * 
 * 2. 资源绑定优化：
 *    - 批量更新：使用updateDescriptorSets函数集中处理
 *    - 描述符池管理：合理分配和重用描述符集合
 *    - 内存局部性：相关资源分组管理
 * 
 * 3. 并行化优化：
 *    - 多线程命令录制：不同线程同时录制不同部分
 *    - 管线并行：同时使用多个管线处理不同任务
 *    - 资源独立性：避免不必要的依赖和等待
 * 
 * 六、实际应用场景
 * 
 * 1. 游戏引擎：
 *    - 图形管线：3D场景渲柕、UI界面、特效处理
 *    - 计算管线：粒子系统、后处理效果、物理模拟
 *    - 光追管线：实时全局光照、反射和阴影
 * 
 * 2. 专业软件：
 *    - CAD/3D建模：复杂几何体的高质量渲柕
 *    - 影视制作：高精度的光照和材质表现
 *    - 科学可视化：大规模数据的实时展示
 * 
 * 3. 通用计算：
 *    - 机器学习：GPU加速的神经网络训练和推理
 *    - 图像处理：实时的图像增强和滤镜效果
 *    - 密码学：加密算法的并行计算
 * 
 * 七、调试和性能分析
 * 
 * 1. 调试工具：
 *    - RenderDoc：最流行的Vulkan调试工具
 *    - NSight Graphics：NVIDIA的GPU调试和分析工具
 *    - Vulkan SDK验证层：开发阶段的错误检测
 * 
 * 2. 常见问题和解决方案：
 *    - 验证层错误：仔细阅读错误信息，检查API使用正确性
 *    - 性能问题：使用GPU分析工具定位瓶颈
 *    - 内存泄露：检查RAII模式的正确实现
 *    - 同步问题：正确使用信号量和栅栏
 * 
 * 3. 性能监控指标：
 *    - 帧率 (FPS)：渲柕流畅度的直接体现
 *    - GPU利用率：管线效率的关键指标
 *    - 内存带宽：纹理和缓冲区访问效率
 *    - 着色器复杂度：单个像素的处理时间
 * 
 * 这个Pipeline类封装了Vulkan管线系统的所有复杂性，为开发者提供了
 * 一个统一、高效、易用的接口。通过合理使用这个类，可以实现
 * 从简单的2D界面到复杂的3D场景，从实时渲柕到离线计算的
 * 各种高性能图形应用。
 */