//
// Created by nio on 2025/10/8.
//

#pragma once

#include <string>

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    class Context;

    /**
     * Vulkan 采样器(Sampler)封装类
     * 
     * 什么是采样器？
     * 采样器是GPU中用于从纹理(texture)中读取像素数据的重要组件。当我们在3D渲染中
     * 需要将纹理贴到3D模型表面时，往往需要对纹理进行缩放、旋转等变换。这时候就需要
     * 采样器来决定如何从原始纹理中"采样"出合适的像素值。
     * 
     * 采样器的主要功能：
     * 1. 滤波(Filtering): 当纹理被放大或缩小时，如何计算新的像素值
     * 2. 地址模式(Address Mode): 当纹理坐标超出[0,1]范围时如何处理
     * 3. LOD控制: 控制多级渐远纹理(mipmapping)的使用
     * 4. 比较功能: 用于阴影贴图等高级技术
     */
    class Sampler final {
    public:
        MOVABLE_ONLY(Sampler); // 只允许移动构造，不允许拷贝（避免资源管理问题）

        /**
         * 基础采样器构造函数
         * 
         * @param context Vulkan上下文，提供设备等信息
         * @param minFilter 纹理缩小时的滤波方式
         *   - VK_FILTER_NEAREST: 最近邻滤波，选择最近的像素，效果锐利但可能有锯齿
         *   - VK_FILTER_LINEAR: 线性滤波，对周围像素进行插值，效果平滑
         * @param magFilter 纹理放大时的滤波方式（选项同minFilter）
         * @param addressModeU 纹理U坐标（水平方向）超出[0,1]范围时的处理方式
         *   - VK_SAMPLER_ADDRESS_MODE_REPEAT: 重复纹理
         *   - VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: 镜像重复
         *   - VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: 夹紧到边缘
         *   - VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: 夹紧到边界色
         * @param addressModeV 纹理V坐标（垂直方向）的处理方式（选项同addressModeU）
         * @param addressModeW 纹理W坐标（深度方向，用于3D纹理）的处理方式
         * @param maxLod 最大LOD级别。LOD(Level of Detail)是多级渐远纹理技术，
         *               距离远的物体使用低分辨率纹理以提高性能。0表示不使用mipmap
         * @param name 采样器的调试名称，便于在调试工具中识别
         */
        explicit Sampler(const Context &context, VkFilter minFilter, VkFilter magFilter,
                         VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                         VkSamplerAddressMode addressModeW, float maxLod,
                         const std::string &name = "");

        /**
         * 带比较功能的采样器构造函数（主要用于阴影贴图等高级技术）
         * 
         * 这个构造函数除了基础功能外，还支持深度比较功能。在阴影贴图技术中，
         * 我们需要比较当前像素的深度与阴影贴图中存储的深度，以判断该像素是否在阴影中。
         * 
         * @param compareEnable 是否启用比较功能
         * @param compareOp 比较操作类型
         *   - VK_COMPARE_OP_NEVER: 从不通过
         *   - VK_COMPARE_OP_LESS: 小于时通过
         *   - VK_COMPARE_OP_EQUAL: 等于时通过  
         *   - VK_COMPARE_OP_LESS_OR_EQUAL: 小于等于时通过
         *   - VK_COMPARE_OP_GREATER: 大于时通过
         *   - VK_COMPARE_OP_NOT_EQUAL: 不等于时通过
         *   - VK_COMPARE_OP_GREATER_OR_EQUAL: 大于等于时通过
         *   - VK_COMPARE_OP_ALWAYS: 总是通过
         * 其他参数含义与基础构造函数相同
         */
        explicit Sampler(const Context &context, VkFilter minFilter, VkFilter magFilter,
                         VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                         VkSamplerAddressMode addressModeW, float maxLod, bool compareEnable,
                         VkCompareOp compareOp, const std::string &name = "");

        /**
         * 析构函数 - 自动清理Vulkan资源
         * 
         * 在对象销毁时自动调用vkDestroySampler释放GPU内存，
         * 这是RAII(Resource Acquisition Is Initialization)设计模式的体现
         */
        ~Sampler() { vkDestroySampler(device_, sampler_, nullptr); };

        /**
         * 获取底层的Vulkan采样器句柄
         * 
         * @return VkSampler Vulkan原生的采样器对象，可以传递给Vulkan API使用
         * [[nodiscard]] 属性提醒调用者不要忽略返回值
         */
        [[nodiscard]] VkSampler vkSampler() const { return sampler_; }

    private:
        /**
         * 私有成员变量
         */
        VkDevice device_ = VK_NULL_HANDLE;    // Vulkan逻辑设备句柄，用于创建和销毁采样器
        VkSampler sampler_ = VK_NULL_HANDLE;  // Vulkan采样器句柄，实际的GPU资源
    };

}  // namespace VkCore  // 注意：这里应该是VkCore，不是VulkanCore