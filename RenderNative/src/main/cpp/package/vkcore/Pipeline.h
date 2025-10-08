//
// Created by nio on 2025/10/8.
//

#pragma once

#include <list>             // std::list 容器
#include <memory>           // 智能指针 std::shared_ptr, std::weak_ptr
#include <mutex>            // 线程同步 std::mutex
#include <unordered_map>    // std::unordered_map 哈希表
#include <vector>           // std::vector 动态数组

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    // 前向声明
    class Context;      // Vulkan上下文
    class Buffer;       // 缓冲区对象
    class RenderPass;   // 渲染通道
    class Sampler;      // 纹理采样器
    class ShaderModule; // 着色器模块
    class Texture;      // 纹理对象

    // 无绑定描述符的最大数量（用于现代图形API的动态资源绑定）
    constexpr uint32_t MAX_DESC_BINDLESS = 1000;

    /**
     * Vulkan 渲染管线(Pipeline)封装类
     * 
     * 什么是渲染管线？
     * 渲染管线是现代GPU处理图形数据的标准化流程，就像工厂的流水线一样。
     * 想象一下汽车制造：原材料经过多个工站，最终组装成完整的汽车。
     * GPU渲染也是类似的过程：
     * 
     * 3D模型数据 → [顶点着色器] → 图元装配 → 光栅化 → [片段着色器] → 最终像素
     * 
     * 渲染管线的主要阶段：
     * 1. 顶点处理：将3D坐标转换为屏幕坐标
     * 2. 图元装配：将顶点组合成三角形、线段等
     * 3. 光栅化：确定每个像素是否在图元内部
     * 4. 片段处理：计算每个像素的颜色
     * 5. 输出合并：处理深度测试、混合等
     * 
     * 为什么需要管线？
     * - 性能优化：GPU并行处理大量数据
     * - 灵活性：可编程着色器允许自定义渲染效果
     * - 标准化：统一的接口处理不同类型的渲染任务
     * 
     * 这个类支持三种类型的管线：
     * 1. 图形管线(Graphics Pipeline)：传统的3D渲染
     * 2. 计算管线(Compute Pipeline)：通用GPU计算
     * 3. 光线追踪管线(Ray Tracing Pipeline)：实时光线追踪渲染
     */
    class Pipeline final {
    public:
        /**
         * 描述符集合描述结构
         * 
         * 什么是描述符？
         * 描述符就像"地址簿"，告诉着色器如何访问各种资源（纹理、缓冲区等）。
         * 想象你在餐厅点菜：菜单（描述符）告诉你有哪些菜（资源）以及如何获取它们。
         * 
         * 描述符集合的概念：
         * - Set 0: 全局资源（相机矩阵、光照信息等）
         * - Set 1: 材质资源（纹理、材质参数等）
         * - Set 2: 对象资源（模型矩阵、对象特定数据等）
         * 
         * 这种分层设计的优势：
         * - 性能优化：相同类型的资源可以批量更新
         * - 内存效率：避免重复绑定相同的资源
         * - 灵活性：不同渲染通道可以重用部分资源
         */
        struct SetDescriptor {
            uint32_t set_{};                                       // 描述符集合的索引号
            std::vector<VkDescriptorSetLayoutBinding> bindings_{}; // 该集合中的绑定信息列表
        };

        /**
         * 视口(Viewport)封装结构
         * 
         * 什么是视口？
         * 视口定义了3D场景在屏幕上显示的区域，就像相机的取景框。
         * 它决定了：
         * - 渲染区域的位置和大小
         * - 深度值的映射范围
         * - 坐标系的转换
         * 
         * 视口变换的作用：
         * 1. 将标准化设备坐标[-1,1]转换为屏幕坐标
         * 2. 控制渲染输出的位置和缩放
         * 3. 支持多视口渲染（如分屏显示）
         * 
         * 常见应用场景：
         * - 全屏渲染：视口 = 整个屏幕
         * - 窗口渲染：视口 = 窗口区域
         * - 小地图：视口 = 屏幕一角的小区域
         */
        struct ViewPort {
            /**
             * 从屏幕尺寸构造视口
             * @param extents 屏幕或窗口的宽度和高度
             */
            explicit ViewPort(const VkExtent2D& extents) { viewport_ = fromExtents(extents); }
            
            ViewPort() = default;                              // 默认构造函数
            ViewPort(const ViewPort&) = default;               // 拷贝构造函数
            ViewPort& operator=(const ViewPort&) = default;    // 拷贝赋值操作符

            /**
             * 从Vulkan视口结构构造
             */
            ViewPort(const VkViewport& viewport) : viewport_(viewport) {}

            /**
             * 赋值操作符重载
             */
            ViewPort& operator=(const VkViewport& viewport) {
                viewport_ = viewport;
                return *this;
            }

            ViewPort& operator=(const VkExtent2D& extents) {
                viewport_ = fromExtents(extents);
                return *this;
            }

            /**
             * 转换为Vulkan扩展尺寸格式
             */
            VkExtent2D toVkExtents() {
                return VkExtent2D{static_cast<uint32_t>(std::abs(viewport_.width)),
                                  static_cast<uint32_t>(std::abs(viewport_.height))};
            }
            
            /**
             * 获取底层Vulkan视口对象
             */
            VkViewport toVkViewPort() { return viewport_; }

        private:
            /**
             * 从屏幕尺寸创建标准视口
             * 
             * 默认设置：
             * - 起始位置：(0,0) - 从屏幕左上角开始
             * - 深度范围：[0.0, 1.0] - 标准深度缓冲区范围
             *   * 0.0 = 最近的物体（屏幕前方）
             *   * 1.0 = 最远的物体（屏幕后方）
             */
            VkViewport fromExtents(const ::VkExtent2D& extents) {
                VkViewport v;
                v.x = 0;                    // 视口左上角X坐标
                v.y = 0;                    // 视口左上角Y坐标  
                v.width = extents.width;    // 视口宽度
                v.height = extents.height;  // 视口高度
                v.minDepth = 0.0;           // 最小深度值（最近）
                v.maxDepth = 1.0;           // 最大深度值（最远）
                return v;
            }

            VkViewport viewport_ = {};  // 存储的Vulkan视口对象
        };

        /**
         * 图形渲柕管线描述结构
         * 
         * 这个结构包含了创建一个图形渲柕管线所需的所有信息。
         * 图形管线是最常用的管线类型，用于渲柕3D模型、UI界面等。
         * 
         * 主要配置分类：
         * 1. 着色器配置：顶点着色器、片段着色器
         * 2. 输入配置：顶点数据格式、原语类型
         * 3. 光栅化配置：裁剪、正面判断等
         * 4. 输出配置：颜色混合、深度测试等
         */
        struct GraphicsPipelineDescriptor {
            // ========================================
            // 资源绑定配置
            // ========================================
            std::vector<SetDescriptor> sets_;               // 描述符集合列表（资源绑定）
            
            // ========================================
            // 着色器配置
            // ========================================
            std::weak_ptr<ShaderModule> vertexShader_;      // 顶点着色器（处理顶点变换）
            std::weak_ptr<ShaderModule> fragmentShader_;    // 片段着色器（计算像素颜色）
            
            // ========================================
            // 数据传递配置
            // ========================================
            std::vector<VkPushConstantRange> pushConstants_; // 推送常量范围（快速数据传递）
            
            // ========================================
            // 动态状态配置
            // ========================================
            std::vector<VkDynamicState> dynamicStates_;     // 可在运行时改变的状态
            
            // ========================================
            // 渲柕目标配置
            // ========================================
            bool useDynamicRendering_ = false;               // 是否使用动态渲柕（Vulkan 1.3新特性）
            std::vector<VkFormat> colorTextureFormats;       // 颜色附件格式列表
            VkFormat depthTextureFormat = VK_FORMAT_UNDEFINED;   // 深度附件格式
            VkFormat stencilTextureFormat = VK_FORMAT_UNDEFINED; // 模板附件格式

            // ========================================
            // 几何处理配置
            // ========================================
            // 原语类型：如何组合顶点数据
            VkPrimitiveTopology primitiveTopology =
                    VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 默认：三角形列表
            
            VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;      // 采样数（抗锯齿）
            VkCullModeFlagBits cullMode = VK_CULL_MODE_BACK_BIT;            // 裁剪模式（隐藏背面）
            VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;        // 正面定义（逆时针）
            ViewPort viewport;                                              // 视口设置
            
            // ========================================
            // 颜色混合配置
            // ========================================
            bool blendEnable = false;                       // 是否启用颜色混合
            uint32_t numberBlendAttachments = 0u;           // 混合附件数量
            
            // ========================================
            // 深度测试配置
            // ========================================
            bool depthTestEnable = true;                            // 启用深度测试（防止遮挡错误）
            bool depthWriteEnable = true;                           // 允许写入深度缓冲区
            VkCompareOp depthCompareOperation = VK_COMPARE_OP_LESS; // 深度比较操作（较近的通过）

            // ========================================
            // 顶点输入配置
            // ========================================
            VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                    .vertexBindingDescriptionCount = 0,     // 顶点绑定描述数量
                    .vertexAttributeDescriptionCount = 0,   // 顶点属性描述数量
            };
            
            // ========================================
            // 着色器特化常量配置
            // ========================================
            std::vector<VkSpecializationMapEntry> vertexSpecConstants_;     // 顶点着色器特化常量
            std::vector<VkSpecializationMapEntry> fragmentSpecConstants_;   // 片段着色器特化常量
            void* vertexSpecializationData = nullptr;                      // 顶点特化数据
            void* fragmentSpecializationData = nullptr;                    // 片段特化数据

            // ========================================
            // 颜色混合状态配置
            // ========================================
            std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates_; // 每个颜色附件的混合状态
        };

        /**
         * 计算管线描述结构
         * 
         * 计算管线用于执行通用的GPU计算任务，不涉及图形渲柕。
         * 常见应用场景：
         * - 图像处理：模糊、锐化、颜色调整等
         * - 物理模拟：粒子系统、流体计算等
         * - 数据处理：数组排序、矩阵运算等
         * - 后处理效果：色调映射、普通渲柕等
         * 
         * 与图形管线的区别：
         * - 更简单：只需要一个计算着色器
         * - 更灵活：可以处理任意数据结构
         * - 更高效：避免了图形管线的复杂步骤
         */
        struct ComputePipelineDescriptor {
            std::vector<SetDescriptor> sets_;                               // 描述符集合列表
            std::weak_ptr<ShaderModule> computeShader_;                     // 计算着色器
            std::vector<VkPushConstantRange> pushConstants_;                // 推送常量范围
            std::vector<VkSpecializationMapEntry> specializationConsts_;    // 特化常量映射
            void* specializationData_ = nullptr;                           // 特化数据指针
        };

        /**
         * 光线追踪管线描述结构
         * 
         * 光线追踪管线是最新的GPU渲柕技术，用于实现高质量的光照和反射效果。
         * 需要RTX系列或支持光追的GPU。
         * 
         * 光线追踪的基本原理：
         * 1. 光线生成：从相机发射光线
         * 2. 光线遍历：光线在场景中传播
         * 3. 命中检测：光线与物体相交
         * 4. 着色计算：计算交点的颜色
         * 5. 递归追踪：处理反射、折射等
         * 
         * 与传统光栅化的优势：
         * - 物理准确的光照模型
         * - 实时全局光照和反射
         * - 更简单的阴影和环境遮蔽
         * - 支持复杂的光学现象
         */
        struct RayTracingPipelineDescriptor {
            std::vector<SetDescriptor> sets_;                                      // 描述符集合
            
            // 光线追踪着色器类型
            std::weak_ptr<ShaderModule> rayGenShader_;                             // 光线生成着色器（启动点）
            std::vector<std::weak_ptr<ShaderModule>> rayMissShaders_;              // 未命中着色器列表
            std::vector<std::weak_ptr<ShaderModule>> rayClosestHitShaders_;        // 最近命中着色器列表
            
            std::vector<VkPushConstantRange> pushConstants_;                       // 推送常量范围

            // TODO: 添加特化常量支持，但需要为每个着色器模块单独配置？
        };

        /**
         * 图形管线构造函数
         * 
         * 创建一个用于3D渲柕的图形管线。图形管线是最复杂也是最常用的管线类型。
         * 
         * @param context Vulkan上下文
         * @param desc 图形管线的详细配置
         * @param renderPass 渲柕通道（定义输出格式和操作）
         * @param name 调试名称
         */
        explicit Pipeline(const Context* context, const GraphicsPipelineDescriptor& desc,
                          VkRenderPass renderPass, const std::string& name = "");

        /**
         * 计算管线构造函数
         * 
         * 创建一个用于通用GPU计算的计算管线。比图形管线简单，但功能强大。
         * 
         * @param context Vulkan上下文
         * @param desc 计算管线的配置
         * @param name 调试名称
         */
        explicit Pipeline(const Context* context, const ComputePipelineDescriptor& desc,
                          const std::string& name = "");

        /**
         * 光线追踪管线构造函数
         * 
         * 创建一个用于实时光线追踪的管线。需要支持光追的GPU和驱动。
         * 
         * @param context Vulkan上下文 
         * @param desc 光线追踪管线的配置
         * @param name 调试名称
         */
        explicit Pipeline(const Context* context, const RayTracingPipelineDescriptor& desc,
                          const std::string& name = "");

        /**
         * 析构函数 - 清理管线资源
         * 
         * 自动清理所有与管线相关的Vulkan资源，包括：
         * - 管线对象本身
         * - 管线布局
         * - 描述符池和布局
         */
        ~Pipeline();

        /**
         * 检查管线是否有效
         * 
         * @return true 如果管线无效（注意：这里的逻辑似乎有误）
         * 
         * TODO: 这个函数的逻辑似乎有问题，应该是 vkPipeline_ != VK_NULL_HANDLE
         */
        bool valid() const { return vkPipeline_ == VK_NULL_HANDLE; }

        /**
         * 获取底层Vulkan管线对象
         * @return Vulkan管线句柄
         */
        VkPipeline vkPipeline() const;

        /**
         * 获取管线布局对象
         * @return 管线布局句柄，用于描述资源绑定和推送常量
         */
        VkPipelineLayout vkPipelineLayout() const;

        /**
         * 更新推送常量数据
         * 
         * 推送常量是一种高效的小量数据传递机制，直接嵌入在GPU命令中。
         * 适用于传递变换矩阵、时间参数、材质ID等小数据。
         * 
         * 优点：
         * - 极低延迟：无需创建描述符或缓冲区
         * - 高效率：直接嵌入GPU命令流
         * - 简单易用：不需要复杂的资源管理
         * 
         * 限制：
         * - 数据大小限制：通常只有128字节
         * - 只能在录制命令时更新
         * 
         * @param commandBuffer 命令缓冲区
         * @param flags 目标着色器阶段标志（如 VK_SHADER_STAGE_VERTEX_BIT）
         * @param size 数据大小（字节）
         * @param data 要传递的数据指针
         */
        void updatePushConstant(VkCommandBuffer commandBuffer, VkShaderStageFlags flags,
                                uint32_t size, const void* data);

        /**
         * 绑定管线到命令缓冲区
         * 
         * 这个操作告诉GPU使用哪个管线来处理后续的渲柕或计算命令。
         * 必须在执行具体的绘制或计算命令之前调用。
         * 
         * 这个方法会：
         * 1. 将管线绑定到指定的命令缓冲区
         * 2. 更新所有待处理的描述符集
         * 
         * @param commandBuffer 目标命令缓冲区
         */
        void bind(VkCommandBuffer commandBuffer);

        /**
         * 绑定顶点缓冲区
         * 
         * 顶点缓冲区包含3D模型的顶点数据（位置、法线、纹理坐标等）。
         * 只有图形管线需要顶点数据，计算和光追管线不需要。
         * 
         * @param commandBuffer 命令缓冲区
         * @param vertexBuffer 包含顶点数据的Vulkan缓冲区
         */
        void bindVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer);

        /**
         * 绑定索引缓冲区
         * 
         * 索引缓冲区存储顶点的引用索引，用于避免重复存储相同的顶点数据。
         * 例如矩形可以用4个顶点和6个索引来表示，而不是6个顶点。
         * 
         * @param commandBuffer 命令缓冲区
         * @param indexBuffer 包含顶点索引的Vulkan缓冲区（uint32类型）
         */
        void bindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer indexBuffer);

        /**
         * 描述符集合分配信息
         */
        struct SetAndCount {
            uint32_t set_;           // 描述符集合索引
            uint32_t count_;         // 需要分配的数量
            std::string name_;       // 调试名称
        };
        
        /**
         * 分配描述符集合
         * 
         * 描述符集合必须在使用之前先分配。这个函数从描述符池中
         * 分配指定数量的描述符集合实例。
         * 
         * 为什么需要多个集合？
         * - 双缓冲/三缓冲：每个帧需要一个集合
         * - 批量渲柕：不同对象需要不同的资源
         * - 并行处理：避免等待和冲突
         * 
         * @param setAndCount 包含集合索引和所需数量的列表
         */
        void allocateDescriptors(const std::vector<SetAndCount>& setAndCount);

        /**
         * 描述符集合绑定信息
         */
        struct SetAndBindingIndex {
            uint32_t set;        // 描述符集合索引
            uint32_t bindIdx;    // 该集合中的实例索引
        };
        
        /**
         * 绑定描述符集合到命令缓冲区
         * 
         * 告诉GPU使用哪些描述符集合来访问资源。必须在执行绘制命令之前调用。
         * 
         * @param commandBuffer 命令缓冲区
         * @param sets 要绑定的集合列表（集合索引 + 实例索引）
         */
        void bindDescriptorSets(VkCommandBuffer commandBuffer,
                                const std::vector<SetAndBindingIndex>& sets);

        /**
         * 资源绑定描述结构
         * 
         * 用于描述将哪些资源绑定到描述符的哪个位置。
         * 支持绑定纹理、采样器、缓冲区等不同类型的资源。
         */
        struct SetBindings {
            uint32_t set_ = 0;                                     // 目标描述符集合索引
            uint32_t binding_ = 0;                                 // 集合内的绑定点索引
            const std::vector<std::shared_ptr<Texture>>* textures_ = nullptr; // 纹理数组（用于纹理采样）
            const std::vector<std::shared_ptr<Sampler>>* samplers_ = nullptr; // 采样器数组（用于纹理过滤）
            std::shared_ptr<Buffer> buffer;                // 缓冲区对象（用于数据存储）
            uint32_t index_ = 0;                                  // 描述符集合实例索引
            uint32_t offset_ = 0;                                 // 缓冲区内的字节偏移
            VkDeviceSize bufferBytes = 0;                          // 缓冲区数据大小
        };

        // void updateDescriptorSets(uint32_t set, uint32_t index,
        //                           const std::vector<SetBindings>& bindings);  // 已废弃的接口
        
        /**
         * 更新采样器描述符集合
         * 
         * 将采样器资源绑定到指定的描述符集合位置。
         * 采样器控制着如何从纹理中采样像素（过滤、寻址）。
         */
        void updateSamplersDescriptorSets(uint32_t set, uint32_t index,
                                          const std::vector<SetBindings>& bindings);
        
        /**
         * 更新纹理描述符集合
         * 
         * 将纹理资源绑定到指定的描述符集合位置。
         * 纹理包含像素数据，用于材质、贴图等效果。
         */
        void updateTexturesDescriptorSets(uint32_t set, uint32_t index,
                                          const std::vector<SetBindings>& bindings);
        
        /**
         * 更新缓冲区描述符集合
         * 
         * 将缓冲区资源绑定到指定的描述符集合位置。
         * 缓冲区可以存储各种类型的数据（矩阵、参数、数组等）。
         */
        void updateBuffersDescriptorSets(uint32_t set, uint32_t index, VkDescriptorType type,
                                         const std::vector<SetBindings>& bindings);
        
        /**
         * 提交所有待处理的描述符更新
         * 
         * 将所有通过bindResource函数添加的资源绑定一次性提交给GPU。
         * 这种批量处理方式可以提高性能。
         */
        void updateDescriptorSets();

        /**
         * 资源绑定方法组
         * 
         * 这一组重载函数用于将各种类型的GPU资源绑定到管线的描述符位置。
         * 这些操作会被缓存，在调用bind()时一次性提交给GPU。
         * 
         * 支持的资源类型：
         * - 缓冲区：存储结构化数据（矩阵、参数等）
         * - 纹理：存储像素数据（贴图、渲柕目标等）
         * - 采样器：控制纹理采样方式
         * - 加速结构：光线追踪的几何数据
         */
        
        /**
         * 绑定缓冲区资源
         * 
         * 将缓冲区资源分配给指定的描述符位置。缓冲区可以存储各种类型的数据。
         * 
         * @param set 描述符集合索引
         * @param binding 集合内的绑定位置
         * @param index 描述符集合实例索引
         * @param buffer 要绑定的缓冲区对象
         * @param offset 缓冲区内的起始偏移（字节）
         * @param size 要使用的数据大小（字节）
         * @param type 描述符类型（如 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER）
         * @param format 缓冲区视图格式（可选，用于纹理缓冲区）
         */
        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          std::shared_ptr<Buffer> buffer, uint32_t offset, uint32_t size,
                          VkDescriptorType type, VkFormat format = VK_FORMAT_UNDEFINED);
        
        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          const std::vector<std::shared_ptr<Texture>>& textures,
                          std::shared_ptr<Sampler> sampler = nullptr,
                          uint32_t dstArrayElement = 0);
        
        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          const std::vector<std::shared_ptr<Sampler>>& samplers);

        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          const std::vector<std::shared_ptr<VkImageView>>& imageViews,
                          VkDescriptorType type);

        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          std::vector<std::shared_ptr<Buffer>> buffers, VkDescriptorType type);

        /**
         * 绑定单个纹理资源
         */
        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          std::shared_ptr<Texture> texture, VkDescriptorType type);

        /**
         * 绑定纹理+采样器组合资源
         * 
         * 这是最常用的纹理绑定方式，将纹理和采样器组合为一个描述符。
         */
        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          std::shared_ptr<Texture> texture, std::shared_ptr<Sampler> sampler,
                          VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        /**
         * 绑定光线追踪加速结构
         * 
         * 用于光线追踪管线，绑定包含几何信息的加速结构。
         */
        void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                          VkAccelerationStructureKHR* accelStructHandle);

    private:
        /**
         * 私有成员函数 - 管线创建和初始化
         */
        
        /**
         * 创建图形渲柕管线
         * 
         * 这是最复杂的管线类型，需要配置所有图形渲柕阶段。
         */
        void createGraphicsPipeline();

        /**
         * 创建计算管线
         * 
         * 相对简单，只需要计算着色器和资源绑定。
         */
        void createComputePipeline();

        /**
         * 创建光线追踪管线
         * 
         * 需要支持光追的GPU和驱动，配置多种光线着色器。
         */
        void createRayTracingPipeline();

        /**
         * 创建管线布局
         * 
         * 管线布局定义了着色器可以访问的资源结构，包括：
         * - 描述符集合布局：定义资源的类型和位置
         * - 推送常量范围：定义快速数据传递的空间
         * 
         * @return 创建的管线布局句柄
         */
        [[nodiscard]] VkPipelineLayout createPipelineLayout(
                const std::vector<VkDescriptorSetLayout>& descLayouts,
                const std::vector<VkPushConstantRange>& pushConsts) const;

        /**
         * 初始化描述符池
         * 
         * 描述符池用于分配描述符集合实例。
         */
        void initDescriptorPool();
        
        /**
         * 初始化描述符布局
         * 
         * 根据管线描述创建描述符集合的布局。
         */
        void initDescriptorLayout();

    private:
        /**
         * 私有成员变量
         */
        
        // ========================================
        // 基础配置
        // ========================================
        const Context* context_ = nullptr;                      // Vulkan上下文指针
        std::string name_;                                      // 管线的调试名称
        
        // ========================================
        // 管线描述（根据类型使用一种）
        // ========================================
        GraphicsPipelineDescriptor graphicsPipelineDesc_;       // 图形管线配置
        ComputePipelineDescriptor computePipelineDesc_;         // 计算管线配置
        RayTracingPipelineDescriptor rayTracingPipelineDesc_;   // 光线追踪管线配置
        
        // ========================================
        // Vulkan对象句柄
        // ========================================
        VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;  // 管线绑定点类型
        VkPipeline vkPipeline_ = VK_NULL_HANDLE;                           // Vulkan管线对象
        VkPipelineLayout vkPipelineLayout_ = VK_NULL_HANDLE;               // 管线布局对象
        VkRenderPass vkRenderPass_ = VK_NULL_HANDLE;                       // 渲柕通道（仅图形管线使用）

        /**
         * 描述符集合管理结构
         */
        struct DescriptorSet {
            std::vector<VkDescriptorSet> vkSets_;       // 分配的描述符集合实例列表
            VkDescriptorSetLayout vkLayout_ = VK_NULL_HANDLE;  // 描述符集合布局
        };
        
        // ========================================
        // 描述符管理
        // ========================================
        std::unordered_map<uint32_t, DescriptorSet> descriptorSets_;  // 描述符集合映射（集合索引 → 集合数据）
        VkDescriptorPool vkDescriptorPool_ = VK_NULL_HANDLE;           // 描述符池（用于分配描述符集合）
        std::vector<VkPushConstantRange> pushConsts_;                  // 推送常量范围列表

        // ========================================
        // 资源绑定缓存（用于批量更新）
        // ========================================
        std::list<std::vector<VkDescriptorBufferInfo>> bufferInfo_;                      // 缓冲区信息列表
        std::list<VkBufferView> bufferViewInfo_;                                        // 缓冲区视图信息列表
        std::list<std::vector<VkDescriptorImageInfo>> imageInfo_;                        // 图像信息列表
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructInfo_;  // 加速结构信息列表
        std::vector<VkWriteDescriptorSet> writeDescSets_;                                // 待处理的描述符更新列表
        std::mutex mutex_;                                                              // 线程同步锁（多线程安全）
    };

}  // namespace VkCore  // 注意：这里应该是VkCore，不是VulkanCore
