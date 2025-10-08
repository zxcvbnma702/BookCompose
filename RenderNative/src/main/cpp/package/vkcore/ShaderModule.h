//
// Created by nio on 2025/10/8.
//

#pragma once

#ifdef _WIN32
#include <glslang/Public/ShaderLang.h>  // GLSL 编译器，用于将 GLSL 代码转换为 SPIR-V
#endif

#include <string>
#include <vector>

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    class Context;

    /**
     * Vulkan 着色器模块(ShaderModule)封装类
     * 
     * 什么是着色器？
     * 着色器是运行在GPU上的小程序，用于处理图形渲染管线中的特定阶段。
     * 想象一下，渲染一个3D场景就像一条流水线，每个工人(着色器)负责一个特定任务：
     * - 顶点着色器：处理3D模型的顶点位置变换
     * - 片段着色器：决定每个像素的颜色
     * - 计算着色器：进行通用计算任务
     * - 光线追踪着色器：处理光线与物体的交互
     * 
     * 什么是着色器模块？
     * 着色器模块是Vulkan中加载和管理着色器代码的容器。它将编译后的着色器代码
     * (SPIR-V字节码)包装成Vulkan可以理解和使用的对象。
     * 
     * GLSL vs SPIR-V：
     * - GLSL: 人类可读的着色器源代码语言，类似于C语言
     * - SPIR-V: 编译后的二进制字节码，GPU可以直接执行
     * 
     * 这个类的主要功能：
     * 1. 从文件加载着色器代码(.glsl 或 .spv文件)
     * 2. 将GLSL代码编译为SPIR-V字节码(Windows平台)
     * 3. 创建Vulkan着色器模块对象
     * 4. 管理着色器的生命周期和资源
     */
    class ShaderModule final {
    public:
        /**
         * 从文件路径创建着色器模块（完整版本）
         * 
         * @param context Vulkan上下文，提供设备等信息
         * @param filePath 着色器文件路径
         *   - .glsl文件：需要编译的GLSL源代码
         *   - .spv文件：已编译的SPIR-V二进制文件
         *   - 文件扩展名决定着色器类型：.vert(顶点), .frag(片段), .comp(计算)等
         * @param entryPoint 着色器入口函数名，通常是"main"
         * @param stages 着色器阶段标志
         *   - VK_SHADER_STAGE_VERTEX_BIT: 顶点着色器
         *   - VK_SHADER_STAGE_FRAGMENT_BIT: 片段着色器
         *   - VK_SHADER_STAGE_COMPUTE_BIT: 计算着色器
         *   - VK_SHADER_STAGE_RAYGEN_BIT_KHR: 光线生成着色器
         * @param name 调试名称，便于在调试工具中识别
         */
        explicit ShaderModule(const Context* context, const std::string& filePath,
                              const std::string& entryPoint,
                              VkShaderStageFlagBits stages, const std::string& name);
        
        /**
         * 从内存数据创建着色器模块
         * 
         * 当着色器代码已经加载到内存中时使用此构造函数
         * @param data 着色器数据，可以是GLSL源码或SPIR-V字节码
         * 其他参数含义与上面相同
         */
        explicit ShaderModule(const Context* context, const std::vector<char>& data,
                              const std::string& entryPoint,
                              VkShaderStageFlagBits stages, const std::string& name);
        
        /**
         * 从文件路径创建着色器模块（简化版本）
         * 
         * 自动使用"main"作为入口点，这是最常见的情况
         */
        explicit ShaderModule(const Context* context, const std::string& filePath,
                              VkShaderStageFlagBits stages, const std::string& name);

        /**
         * 析构函数 - 自动清理Vulkan资源
         * 
         * 使用RAII模式确保着色器模块被正确销毁，释放GPU内存
         */
        ~ShaderModule();

        /**
         * 获取底层的Vulkan着色器模块句柄
         * 
         * @return VkShaderModule 可以传递给Vulkan API使用的原生对象
         */
        VkShaderModule vkShaderModule() const;

        /**
         * 获取着色器阶段标志
         * 
         * @return 该着色器所属的渲染管线阶段
         */
        VkShaderStageFlagBits vkShaderStageFlags() const;

        /**
         * 获取着色器入口点函数名
         * 
         * @return 着色器执行的起始函数名，通常是"main"
         */
        const std::string& entryPoint() const;

    private:
        /**
         * 私有辅助函数
         */
        
#ifdef _WIN32
        /**
         * 根据文件名推断着色器类型
         * 
         * @param fileName 着色器文件名
         * @return EShLanguage glslang库使用的着色器语言类型
         * 
         * 支持的文件扩展名：
         * .vert → 顶点着色器
         * .frag → 片段着色器  
         * .comp → 计算着色器
         * .rgen → 光线生成着色器
         * .rmiss → 光线未命中着色器
         * .rchit → 光线最近命中着色器
         * .rahit → 光线任意命中着色器
         */
        EShLanguage shaderStageFromFileName(const char* fileName);

        /**
         * 将GLSL源代码编译为SPIR-V字节码
         * 
         * 这是着色器编译的核心过程：
         * 1. 预处理：处理#include等预处理指令
         * 2. 解析：检查语法和语义错误
         * 3. 链接：解决函数和变量引用
         * 4. 生成：输出SPIR-V字节码
         * 
         * @param data GLSL源代码
         * @param shaderStage 着色器类型
         * @param shaderDir 着色器目录路径，用于解析#include
         * @param entryPoint 入口函数名
         * @return 编译后的SPIR-V字节码
         */
        std::vector<char> glslToSpirv(const std::vector<char>& data,
                                      EShLanguage shaderStage,
                                      const std::string& shaderDir,
                                      const char* entryPoint);
#endif

        /**
         * 打印着色器源代码（带行号）
         * 
         * 用于调试和错误报告，让开发者能够快速定位问题代码行
         */
        void printShader(const std::vector<char>& data);

        /**
         * 从文件创建着色器模块的内部实现
         * 
         * 处理文件加载、格式检测和编译过程
         */
        void createShader(const std::string& filePath, const std::string& entryPoint,
                          const std::string& name);

        /**
         * 从SPIR-V字节码创建着色器模块的内部实现
         * 
         * 调用Vulkan API创建最终的VkShaderModule对象
         */
        void createShader(const std::vector<char>& spirv,
                          const std::string& entryPoint, const std::string& name);

    private:
        /**
         * 私有成员变量
         */
        const Context* context_ = nullptr;              // Vulkan上下文指针
        VkShaderModule vkShaderModule_ = VK_NULL_HANDLE; // Vulkan着色器模块句柄
        VkShaderStageFlagBits vkStageFlags_;            // 着色器阶段标志
        std::string entryPoint_;                        // 入口函数名
    };

}  // namespace VkCore  // 注意：这里应该是VkCore，不是VulkanCore
