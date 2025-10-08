//
// Created by nio on 2025/10/8.
//

#include "Sampler.h"
#include "Context.h"

namespace VkCore {
    // ========================================
    // 基础采样器构造函数实现
    // ========================================
    Sampler::Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter,
                     VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                     VkSamplerAddressMode addressModeW, float maxLod, const std::string& name)
            : device_{context.device()} {  // 从上下文中获取Vulkan逻辑设备句柄
        // 创建Vulkan采样器描述结构体
        // 这个结构体包含了告诉GPU如何采样纹理的所有信息
        const VkSamplerCreateInfo samplerInfo = {
                // 结构体类型标识，Vulkan用这个来验证结构体的正确性
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                
                // 放大滤波模式：当纹理被放大（一个纹理像素对应多个屏幕像素）时如何处理
                .magFilter = magFilter,
                
                // 缩小滤波模式：当纹理被缩小（多个纹理像素对应一个屏幕像素）时如何处理  
                .minFilter = minFilter,
                
                // Mipmap模式：决定在不同LOD级别之间如何插值
                // 如果maxLod > 0说明使用mipmap，选择LINEAR进行平滑过渡
                // 否则使用NEAREST，因为没有mipmap就不需要在级别间插值
                .mipmapMode = maxLod > 0 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
                
                // 纹理坐标寻址模式：当纹理坐标超出[0,1]范围时如何处理
                .addressModeU = addressModeU,  // U轴（水平）
                .addressModeV = addressModeV,  // V轴（垂直） 
                .addressModeW = addressModeW,  // W轴（深度，用于3D纹理）
                
                // LOD偏移值：可以让纹理看起来更锐利(负值)或更模糊(正值)
                .mipLodBias = 0,
                
                // 各向异性过滤：提高斜视角度下纹理的清晰度，这里禁用以简化
                .anisotropyEnable = VK_FALSE,
                
                // LOD范围：控制使用哪些mipmap级别
                .minLod = 0,        // 最清晰的级别（原始纹理）
                .maxLod = maxLod,   // 最模糊的级别
        };
        // 调用Vulkan API创建采样器对象
        // VK_CHECK是一个宏，用于检查Vulkan函数调用是否成功，失败时会抛出异常
        VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_));
        
        // 为采样器设置调试名称，在GPU调试工具（如RenderDoc）中可以看到这个名称
        // 这对于性能分析和问题调试非常有用
        context.setVkObjectname(sampler_, VK_OBJECT_TYPE_SAMPLER, "Sampler: " + name);
    };

    // ========================================
    // 带比较功能的采样器构造函数实现
    // ========================================
    // 这个构造函数主要用于阴影贴图等需要深度比较的高级渲染技术
    Sampler::Sampler(const Context& context, VkFilter minFilter, VkFilter magFilter,
                     VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                     VkSamplerAddressMode addressModeW, float maxLod, bool compareEnable,
                     VkCompareOp compareOp, const std::string& name /*= ""*/)
            : device_{context.device()} {  // 从上下文中获取Vulkan逻辑设备句柄
        // 创建带比较功能的Vulkan采样器描述结构体
        const VkSamplerCreateInfo samplerInfo = {
                // 结构体类型标识
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                
                // 滤波模式（与基础版本相同）
                .magFilter = magFilter,    // 放大滤波
                .minFilter = minFilter,    // 缩小滤波
                
                // Mipmap模式（与基础版本相同）
                .mipmapMode = maxLod > 0 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
                
                // 地址模式（与基础版本相同）
                .addressModeU = addressModeU,
                .addressModeV = addressModeV, 
                .addressModeW = addressModeW,
                
                // LOD控制（与基础版本相同）
                .mipLodBias = 0,
                .anisotropyEnable = VK_FALSE,
                
                // *** 比较功能相关设置 ***
                // 这是这个构造函数的特殊之处，主要用于阴影贴图技术
                .compareEnable = compareEnable,  // 是否启用深度比较
                .compareOp = compareOp,          // 比较操作（如LESS_OR_EQUAL用于阴影测试）
                
                // LOD范围
                .minLod = 0,
                .maxLod = maxLod,
        };
        // 调用Vulkan API创建带比较功能的采样器对象
        VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_));
        
        // 设置调试名称，便于在调试工具中识别
        context.setVkObjectname(sampler_, VK_OBJECT_TYPE_SAMPLER, "Sampler: " + name);
    }

}  // namespace VkCore

/*
 * 总结：Vulkan采样器的作用和重要性
 * 
 * 1. 为什么需要采样器？
 *    在3D渲染中，纹理贴图很少与屏幕像素完美对应。采样器告诉GPU如何从纹理中
 *    "采样"出合适的颜色值。
 * 
 * 2. 主要应用场景：
 *    - 普通纹理采样：用于给3D模型贴图
 *    - 阴影贴图：使用比较功能实现实时阴影
 *    - 环境贴图：用于反射和环境光照
 *    - 法线贴图：用于增加表面细节
 * 
 * 3. 性能考虑：
 *    - 采样器是GPU上的轻量级对象，创建成本低
 *    - 合理的滤波设置可以在视觉质量和性能间取得平衡
 *    - Mipmap可以显著提高远距离纹理的渲染性能
 * 
 * 4. 常见设置组合：
 *    - 像素艺术：NEAREST + CLAMP_TO_EDGE
 *    - 普通纹理：LINEAR + REPEAT  
 *    - UI元素：LINEAR + CLAMP_TO_EDGE
 *    - 阴影贴图：LINEAR + CLAMP_TO_BORDER + 比较功能
 */