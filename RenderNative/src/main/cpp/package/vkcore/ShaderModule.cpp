//
// Created by nio on 2025/10/8.
//

#include "ShaderModule.h"

#ifdef _WIN32
#include <glslang/Public/ResourceLimits.h>  // GLSL资源限制配置
#include <glslang/SPIRV/GlslangToSpv.h>      // GLSL到SPIR-V的转换器
#include <spirv_reflect.h>                   // SPIR-V反射库，用于分析着色器元数据
#endif

#include <cmath>        // 数学函数
#include <filesystem>   // 文件系统操作
#include <fstream>      // 文件流
#include <iostream>     // 输入输出流
#include <sstream>      // 字符串流

#include "Context.h"

#ifdef _WIN32
// ========================================
// Windows下的GLSL编译支持
// ========================================

namespace {
// 无绑定描述符的最大数量，用于现代图形API的动态资源绑定
static constexpr uint32_t MAX_DESC_BINDLESS = 1000;

/**
 * 自定义#include处理器
 * 
 * 在GLSL中，我们经常需要#include其他文件（如共享的常数、函数等）。
 * 这个类负责告诉glslang编译器如何找到和加载这些被包含的文件。
 * 
 * 举例：如果GLSL中有 #include "common.glsl"，
 * 这个类会找到common.glsl文件并读取其内容。
 */
class CustomIncluder final : public glslang::TShader::Includer {
 public:
  /**
   * 构造函数：设置着色器文件所在目录
   * @param shaderDir 着色器源文件所在的目录路径
   */
  explicit CustomIncluder(const std::string& shaderDir) : shaderDirectory(shaderDir) {}
  ~CustomIncluder() = default;

  /**
   * 处理系统包含（如 #include <system_header.h>）
   * 这里暂时不支持系统头文件，返回nullptr
   */
  IncludeResult* includeSystem(const char* headerName, const char* includerName,
                               size_t inclusionDepth) override {
    // 可以在这里实现系统包含路径，如果需要的话
    return nullptr;
  }

  /**
   * 处理本地包含（如 #include "local_header.glsl"）
   * 
   * @param headerName 被包含的文件名
   * @param includerName 包含者文件名
   * @param inclusionDepth 包含深度（防止循环包含）
   * @return 包含结果，包含文件内容和元数据
   */
  IncludeResult* includeLocal(const char* headerName, const char* includerName,
                              size_t inclusionDepth) override {
    // 构建完整的文件路径
    std::string fullPath = shaderDirectory + "/" + headerName;
    
    // 尝试打开文件
    std::ifstream fileStream(fullPath, std::ios::in);
    if (!fileStream.is_open()) {
      std::string errMsg = "Failed to open included file: ";
      errMsg.append(headerName);
      std::cerr << errMsg << std::endl;
      return nullptr;
    }

    // 读取整个文件内容
    std::stringstream fileContent;
    fileContent << fileStream.rdbuf();
    fileStream.close();

    // 分配内存存储文件内容
    // 注意：Includer负责内存管理，当不再需要时会自动删除
    char* content = new char[fileContent.str().length() + 1];
    strncpy(content, fileContent.str().c_str(), fileContent.str().length());
    content[fileContent.str().length()] = '\0';

    // 返回包含结果
    return new IncludeResult(headerName, content, fileContent.str().length(), nullptr);
  }

  /**
   * 释放包含结果的内存
   * 当glslang不再需要某个包含文件时，会调用此函数清理内存
   */
  void releaseInclude(IncludeResult* result) override {
    if (result) {
      delete[] result->headerData;  // 释放文件内容内存
      delete result;                // 释放结果对象
    }
  }

 private:
  std::string shaderDirectory;  // 着色器文件所在目录
};
}  // namespace
#endif

namespace VkCore {

    // ========================================
    // 着色器模块构造函数实现
    // ========================================
    
    /**
     * 从文件创建着色器模块的构造函数（完整版本）
     * 
     * 这个构造函数是最常用的，它从指定的文件路径加载着色器代码。
     * 支持两种文件格式：
     * - .glsl文件：人类可读的GLSL源代码，需要编译
     * - .spv文件：已编译的SPIR-V二进制文件，可直接使用
     */
    ShaderModule::ShaderModule(const Context* context, const std::string& filePath,
                               const std::string& entryPoint, VkShaderStageFlagBits stages,
                               const std::string& name)
            : context_(context),        // 保存Vulkan上下文指针
              entryPoint_(entryPoint),  // 保存入口函数名
              vkStageFlags_(stages) {   // 保存着色器阶段标志
        // 调用内部实现函数创建着色器
        createShader(filePath, entryPoint, name);
    }

    /**
     * 从内存数据创建着色器模块的构造函数
     * 
     * 当着色器代码已经加载到内存中（例如从网络下载、从资源文件中读取）时使用。
     * 这种方式可以避免文件I/O操作，提高加载速度。
     */
    ShaderModule::ShaderModule(const Context* context, const std::vector<char>& data,
                               const std::string& entryPoint, VkShaderStageFlagBits stages,
                               const std::string& name)
            : context_(context),        // 保存Vulkan上下文指针
              entryPoint_(entryPoint),  // 保存入口函数名  
              vkStageFlags_(stages) {   // 保存着色器阶段标志
        // 直接从内存数据创建着色器
        createShader(data, entryPoint, name);
    }

    /**
     * 简化版构造函数：自动使用"main"作为入口点
     * 
     * 大多数着色器都使用"main"作为入口函数，此构造函数提供了一个更简洁的接口。
     * 这种委托构造函数的模式在C++中很常见，可以减少代码重复。
     */
    ShaderModule::ShaderModule(const Context* context, const std::string& filePath,
                               VkShaderStageFlagBits stages, const std::string& name)
            : ShaderModule(context, filePath, "main", stages, name) {
        // 委托给上面的完整构造函数，传入"main"作为入口点
    }

    /**
     * 析构函数：清理Vulkan资源
     * 
     * 在对象销毁时自动调用，确保GPU内存被正确释放。
     * 这是RAII(Resource Acquisition Is Initialization)设计模式的体现，
     * 确保资源的自动管理和释放。
     */
    ShaderModule::~ShaderModule() {
        // 调用Vulkan API销毁着色器模块，释放GPU内存
        vkDestroyShaderModule(context_->device(), vkShaderModule_, nullptr);
    }

    // ========================================
    // 公共接口实现
    // ========================================
    
    /**
     * 返回底层Vulkan着色器模块句柄
     * 用于传递给Vulkan API函数
     */
    VkShaderModule ShaderModule::vkShaderModule() const { 
        return vkShaderModule_; 
    }

    /**
     * 返回着色器阶段标志
     * 用于在创建渲柕管线时指定该着色器的作用阶段
     */
    VkShaderStageFlagBits ShaderModule::vkShaderStageFlags() const { 
        return vkStageFlags_; 
    }

    /**
     * 返回着色器入口函数名
     * Vulkan需要知道从哪个函数开始执行着色器代码
     */
    const std::string& ShaderModule::entryPoint() const { 
        return entryPoint_; 
    }

    // 着色器资源的最大数量限制（纹理、缓冲区等）
    static constexpr uint32_t MAX_RESOURCES_COUNT = 1000;

#ifdef _WIN32
    // ========================================
    // 着色器类型识别和编译相关函数
    // ========================================
    
    /**
     * 根据文件名推断着色器类型
     * 
     * 这个函数通过文件扩展名来自动检测着色器类型，这是一种常见的约定。
     * 
     * 支持的着色器类型和文件扩展名对应：
     * - .vert → 顶点着色器：处理3D模型的顶点变换（位置、旋转、缩放）
     * - .frag → 片段着色器：决定每个像素的最终颜色
     * - .comp → 计算着色器：执行通用计算任务（如图像处理、物理模拟）
     * 
     * 光线追踪着色器类型（RTX等支持光追的GPU）：
     * - .rgen → 光线生成：启动光线追踪过程
     * - .rmiss → 光线未命中：处理光线没有命中任何物体的情况
     * - .rchit → 光线最近命中：处理光线与物体的交点
     * - .rahit → 光线任意命中：处理光线穿过半透明物体的情况
     * 
     * @param fileName 着色器文件名（包含扩展名）
     * @return EShLanguage glslang库用的着色器类型枚举
     */
    EShLanguage ShaderModule::shaderStageFromFileName(const char* fileName) {
        // 顶点着色器：处理每个顶点的位置变换
        if (util::endsWith(fileName, ".vert")) {
            return EShLangVertex;
        } 
        // 片段着色器：决定每个像素的颜色
        else if (util::endsWith(fileName, ".frag")) {
            return EShLangFragment;
        } 
        // 计算着色器：执行通用GPU计算
        else if (util::endsWith(fileName, ".comp")) {
            return EShLangCompute;
        } 
        // 光线生成着色器：启动光线追踪
        else if (util::endsWith(fileName, ".rgen")) {
            return EShLangRayGen;
        } 
        // 光线未命中着色器：处理光线未命中情况
        else if (util::endsWith(fileName, ".rmiss")) {
            return EShLangMiss;
        } 
        // 光线最近命中着色器：处理光线交点
        else if (util::endsWith(fileName, ".rchit")) {
            return EShLangClosestHit;
        } 
        // 光线任意命中着色器：处理半透明物体
        else if (util::endsWith(fileName, ".rahit")) {
            return EShLangAnyHit;
        } 
        else {
            // 不支持的文件类型，开发阶段需要添加支持
            ASSERT(false, "Add if/else for GLSL stage");
        }

        // 默认返回顶点着色器类型（不应该达到这里）
        return EShLangVertex;
    }
#endif

    // ========================================
    // 调试和工具函数
    // ========================================
    
    /**
     * 打印着色器代码（带行号）
     * 
     * 这个函数主要用于调试，当着色器编译失败时可以清晰地看到问题代码。
     * 行号的对齐保证了输出的整洁美观。
     * 
     * 输出格式例子：
     * 1   #version 450
     * 2   layout(location = 0) in vec3 position;
     * 3   void main() {
     * 4       gl_Position = vec4(position, 1.0);
     * 5   }
     * 
     * @param data 着色器源代码数据
     */
    void ShaderModule::printShader(const std::vector<char>& data) {
        // 统计总行数，用于确定行号的宽度
        uint32_t totalLines = std::count(data.begin(), data.end(), '\n');
        
        // 计算行号所需的最大字符数（支持最多9,999行）
        const uint32_t maxNumSpaces = static_cast<uint32_t>(std::log10(totalLines)) + 1;

        uint32_t lineNum = 1;
        std::cout << lineNum;  // 打印第一行的行号
        
        // 添加空格以对齐行号
        const auto numSpaces = maxNumSpaces - static_cast<uint32_t>(std::log10(lineNum)) - 1;
        for (int i = 0; i < numSpaces; ++i) {
            std::cout << ' ';
        }
        std::cout << "  ";  // 行号与代码之间的间隔
        
        // 逐个字符打印，遇到换行符时打印新行号
        for (char c : data) {
            std::cout << c;
            if (c == '\n' && lineNum < totalLines) {  // 换行且不是最后一行
                ++lineNum;
                std::cout << lineNum;

                // 重新计算对齐空格
                const auto numSpaces = maxNumSpaces - static_cast<uint32_t>(std::log10(lineNum)) - 1;
                for (int i = 0; i < numSpaces; ++i) {
                    std::cout << ' ';
                }
                std::cout << "  ";  // 行号与代码之间的间隔
            }
        }
        std::cout << std::endl;  // 结束时换行
    }

    /**
     * 移除GLSL代码中不必要的行
     * 
     * 在GLSL编译过程中，预处理器会添加一些辅助指令，这些指令在调试时会干扰阅读。
     * 此函数移除这些不必要的行，使调试工具（如RenderDoc）能够更好地
     * 展示和单步调试着色器代码。
     * 
     * 移除的内容：
     * - #extension GL_GOOGLE_include_directive : require：Google的包含指令扩展
     * - #line 指令：用于调试信息的行号指示
     * 
     * @param str 预处理后的GLSL代码
     * @return 清理后的GLSL代码
     */
    std::string removeUnnecessaryLines(const std::string& str) {
        std::istringstream iss(str);  // 输入字符串流
        std::ostringstream oss;       // 输出字符串流
        std::string line;

        // 逐行处理输入字符串
        while (std::getline(iss, line)) {
            // 过滤掉Google包含指令和#line指令
            if (line != "#extension GL_GOOGLE_include_directive : require" &&
                line.substr(0, 5) != "#line") {
                oss << line << '\n';  // 保留其他行
            }
        }
        return oss.str();
    }

#ifdef _WIN32
    /**
     * 将GLSL源代码编译为SPIR-V字节码
     * 
     * 这是整个ShaderModule类的核心功能，负责将人类可读的GLSL代码转换为
     * GPU可以执行的SPIR-V二进制格式。整个编译过程包括：
     * 
     * 1. 初始化编译器：设置 glslang 编译器环境
     * 2. 预处理阶段：处理 #include、#define 等预处理指令
     * 3. 解析阶段：检查语法错误和语义错误
     * 4. 链接阶段：解决函数和变量引用
     * 5. 代码生成：输出最终的SPIR-V字节码
     * 
     * 为什么需要编译？
     * - GLSL是高级语言，GPU无法直接执行
     * - SPIR-V是优化过的中间表示，GPU驱动可以高效地将其转换为机器码
     * - 编译过程可以发现并报告编程错误
     * 
     * @param data GLSL源代码
     * @param shaderStage 着色器类型（顶点、片段、计算等）
     * @param shaderDir 着色器文件所在目录，用于解析#include
     * @param entryPoint 着色器入口函数名
     * @return 编译后的SPIR-V字节码
     */
    std::vector<char> ShaderModule::glslToSpirv(const std::vector<char>& data,
                                            EShLanguage shaderStage,
                                            const std::string& shaderDir,
                                            const char* entryPoint) {
        // 管理glslang库的全局初始化（只需要执行一次）
        static bool glslangInitialized = false;

        if (!glslangInitialized) {
            glslang::InitializeProcess();  // 初始化glslang编译器
            glslangInitialized = true;
        }

        // ========================================
        // 第一步：创建和配置预处理着色器对象
        // ========================================
        
        // 创建用于预处理的临时着色器对象
        glslang::TShader tshadertemp(shaderStage);
        const char* glslCStr = data.data();
        tshadertemp.setStrings(&glslCStr, 1);  // 设置GLSL源代码

        // 确定Vulkan和SPIR-V的目标版本
        glslang::EshTargetClientVersion clientVersion = glslang::EShTargetVulkan_1_3;  // Vulkan 1.3
        glslang::EShTargetLanguageVersion langVersion = glslang::EShTargetSpv_1_0;    // SPIR-V 1.0

        // 光线追踪着色器需要更新的SPIR-V版本才能支持
        if (shaderStage == EShLangRayGen || shaderStage == EShLangAnyHit ||
            shaderStage == EShLangClosestHit || shaderStage == EShLangMiss) {
            langVersion = glslang::EShTargetSpv_1_4;  // 升级到SPIR-V 1.4
        }

        // 配置编译环境参数
        tshadertemp.setEnvInput(
            glslang::EShSourceGlsl,      // 输入语言类型：GLSL
            shaderStage,                 // 着色器阶段
            glslang::EShClientVulkan,    // 目标客户端：Vulkan
            460                          // GLSL语言版本：4.60
        );

        tshadertemp.setEnvClient(glslang::EShClientVulkan, clientVersion);  // 设置客户端和版本
        tshadertemp.setEnvTarget(glslang::EShTargetSpv, langVersion);       // 设置目标语言和版本

        tshadertemp.setEntryPoint(entryPoint);       // 设置入口函数
        tshadertemp.setSourceEntryPoint(entryPoint); // 设置源代码入口函数

        // ========================================
        // 第二步：创建用于最终编译的着色器对象
        // ========================================
        
        // 创建用于正式编译的着色器对象
        glslang::TShader tshader(shaderStage);

        // 配置编译环境（与预处理器相同）
        tshader.setEnvInput(glslang::EShSourceGlsl, shaderStage, glslang::EShClientVulkan, 460);
        tshader.setEnvClient(glslang::EShClientVulkan, clientVersion);
        tshader.setEnvTarget(glslang::EShTargetSpv, langVersion);
        tshader.setEntryPoint(entryPoint);
        tshader.setSourceEntryPoint(entryPoint);

        // ========================================
        // 第三步：预处理阶段
        // ========================================
        
        // 获取默认的资源限制（如最大纹理数量、缓冲区数量等）
        const TBuiltInResource* resources = GetDefaultResources();
        
        // 设置编译消息级别
        const EShMessages messages = static_cast<EShMessages>(
            EShMsgDefault |      // 默认消息
            EShMsgSpvRules |     // SPIR-V规则检查
            EShMsgVulkanRules |  // Vulkan规则检查
            EShMsgDebugInfo      // 调试信息
        );
        
        // 创建自定义的#include处理器
        CustomIncluder includer(shaderDir);

        // 执行预处理：展开#include、#define等预处理指令
        std::string preprocessedGLSL;
        if (!tshadertemp.preprocess(
                resources,           // 资源限制
                460,                 // GLSL版本
                ENoProfile,          // 不使用特定配置文件
                false,               // 不强制版本
                false,               // 不转发消息
                messages,            // 消息级别
                &preprocessedGLSL,   // 输出的预处理结果
                includer             // 包含文件处理器
        )) {
            // 预处理失败，打印错误信息
            std::cout << "=== Preprocessing failed for shader ==="uff1a
            printShader(data);  // 打印原始代码
            std::cout << std::endl;
            std::cout << "Error Log: " << tshadertemp.getInfoLog() << std::endl;
            std::cout << "Debug Log: " << tshadertemp.getInfoDebugLog() << std::endl;
            ASSERT(false, "Preprocessing stage failed");
            return std::vector<char>();  // 返回空结果
        }

        // 清理预处理结果：移除不必要的行以便于调试
        // 这对RenderDoc等调试工具很重要，能够正确地单步调试着色器
        preprocessedGLSL = removeUnnecessaryLines(preprocessedGLSL);

        // 将清理后的GLSL代码设置给正式编译器
        const char* preprocessedGLSLStr = preprocessedGLSL.c_str();
        tshader.setStrings(&preprocessedGLSLStr, 1);

        // ========================================
        // 第四步：语法和语义分析
        // ========================================
        
        // 执行详细的语法和语义检查
        if (!tshader.parse(
                resources,  // 资源限制
                460,        // GLSL版本
                false,      // 不使用前向兼容模式
                messages    // 消息级别
        )) {
            // 解析失败，打印详细错误信息
            std::cout << "=== Parsing failed for shader ===" << std::endl;
            printShader(data);  // 打印原始代码帮助定位错误
            std::cout << std::endl;
            std::cout << "Error Log: " << tshader.getInfoLog() << std::endl;
            std::cout << "Debug Log: " << tshader.getInfoDebugLog() << std::endl;
            ASSERT(false, "Syntax/semantic analysis failed");
            return std::vector<char>();
        }

        // ========================================
        // 第五步：配置SPIR-V生成选项
        // ========================================
        
        glslang::SpvOptions options;

#ifdef _DEBUG
        // Debug模式：保留调试信息，不进行优化
        tshader.setDebugInfo(true);          // 生成调试信息
        options.generateDebugInfo = true;    // 在SPIR-V中包含调试信息
        options.disableOptimizer = true;     // 禁用优化器，保持代码可读性
        options.optimizeSize = false;        // 不优化代码大小
        options.stripDebugInfo = false;      // 不移除调试信息
#else
        // Release模式：但仍然保留未使用变量以保持CPU/GPU结构一致性
        // 这是一个重要的设计决定：
        // 如果启用优化，SPIR-V编译器会移除未使用的变量，
        // 这可能导致CPU端和GPU端的结构体布局不一致，
        // 从而引起Debug和Release版本之间的不一致性问题
        options.disableOptimizer = true;     // 保留未使用变量
        options.optimizeSize = true;         // 但仍然优化代码大小
        options.stripDebugInfo = true;       // 移除调试信息减小文件大小
#endif

        // ========================================
        // 第六步：链接阶段
        // ========================================
        
        // 创建程序对象并添加着色器
        glslang::TProgram program;
        program.addShader(&tshader);
        
        // 执行链接：解决着色器中的函数调用和变量引用
        if (!program.link(messages)) {
            std::cout << "=== Linking failed for shader ===" << std::endl;
            std::cout << "Link Error Log: " << program.getInfoLog() << std::endl;
            std::cout << "Link Debug Log: " << program.getInfoDebugLog() << std::endl;
            ASSERT(false, "Shader linking failed");
            return std::vector<char>();
        }

        // ========================================
        // 第七步：生成SPIR-V字节码
        // ========================================
        
        // 创建SPIR-V数据容器和日志记录器
        std::vector<uint32_t> spirvData;
        spv::SpvBuildLogger spvLogger;
        
        // 执行最终的代码生成：将中间表示转换为SPIR-V字节码
        glslang::GlslangToSpv(
            *program.getIntermediate(shaderStage),  // 获取中间表示（IR）
            spirvData,                              // 输出的SPIR-V数据
            &spvLogger,                             // 日志记录器
            &options                                // 生成选项
        );

        // 将uint32_t数组转换为char数组（Vulkan API需要char*类型）
        std::vector<char> byteCode;
        byteCode.resize(spirvData.size() * sizeof(uint32_t));
        std::memcpy(byteCode.data(), spirvData.data(), byteCode.size());

        return byteCode;  // 返回编译完成的SPIR-V字节码
    }
#endif

    // ========================================
    // 着色器创建的内部实现
    // ========================================
    
    /**
     * 从文件创建着色器的内部实现
     * 
     * 这个函数处理了两种类型的文件：
     * 1. .spv文件：已编译的SPIR-V二进制文件，可直接使用
     * 2. .glsl文件：GLSL源代码文件，需要编译为SPIR-V
     * 
     * 优先使用预编译的.spv文件能够：
     * - 提高应用启动速度（无需实时编译）
     * - 减少对编译器的依赖
     * - 确保跨平台一致性
     */
    void ShaderModule::createShader(const std::string& filePath,
                                    const std::string& entryPoint, const std::string& name) {
        std::vector<char> spirv;  // 最终的SPIR-V字节码
        
        // 检查文件类型：.spv是二进制文件，其他是文本文件
        const bool isBinary = util::endsWith(filePath.c_str(), ".spv");
        
        // 根据文件类型选择适当的读取模式
        std::vector<char> fileData = util::readFile(filePath, isBinary);
        std::filesystem::path file(filePath);
        
        if (isBinary) {
            // 对于.spv文件，直接使用读取的数据
            spirv = std::move(fileData);
        }
#ifdef _WIN32
        else {
            // 对于GLSL文件，需要编译为SPIR-V（只在Windows上支持）
            spirv = glslToSpirv(
                fileData,                                    // GLSL源代码
                shaderStageFromFileName(filePath.c_str()),   // 从文件名推断的着色器类型
                file.parent_path().string(),                 // 着色器文件所在目录（用于#include）
                entryPoint.c_str()                           // 入口函数名
            );
        }
#else
        else {
            // 在非Windows平台上，需要预先编译好.spv文件
            ASSERT(false, "GLSL compilation not supported on this platform, use .spv files");
        }
#endif

        // 使用SPIR-V字节码创建着色器模块
        createShader(spirv, entryPoint, name);
    }

    /**
     * 从SPIR-V字节码创建Vulkan着色器模块
     * 
     * 这是着色器创建过程的最终步骤，将SPIR-V字节码转换为Vulkan可以使用的
     * 着色器模块对象。SPIR-V是一种中间表示形式，GPU驱动程序会进一步
     * 将其编译为特定硬件的机器码。
     * 
     * @param spirv SPIR-V字节码数据
     * @param entryPoint 着色器入口函数名
     * @param name 调试名称
     */
    void ShaderModule::createShader(const std::vector<char>& spirv,
                                    const std::string& entryPoint, const std::string& name) {
        // 创建Vulkan着色器模块的描述结构体
        const VkShaderModuleCreateInfo shaderModuleInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,  // 结构体类型
                .codeSize = spirv.size(),                               // SPIR-V字节码大小
                .pCode = (const uint32_t*)spirv.data(),                 // SPIR-V数据指针（必须是32位对齐）
        };
        
        // 调用Vulkan API创建着色器模块
        VK_CHECK(vkCreateShaderModule(context_->device(), &shaderModuleInfo, nullptr,
                                      &vkShaderModule_));
        
        // 为着色器模块设置调试名称，方便在GPU调试工具中识别
        context_->setVkObjectname(vkShaderModule_, VK_OBJECT_TYPE_SHADER_MODULE,
                                  "Shader Module: " + name);
    }

}  // namespace VkCore

/*
 * ========================================
 * Vulkan着色器模块系统的总结和学习指南
 * ========================================
 * 
 * 一、什么是着色器？
 * 着色器是运行在GPU上的小程序，在3D渲染管线的不同阶段执行特定任务。
 * 想象一下，渲染一个3D场景就像一条流水线：
 * 
 * 3D模型 -> [顶点着色器] -> 光栅化 -> [片段着色器] -> 最终像素
 * 
 * 二、主要着色器类型：
 * 
 * 1. 顶点着色器 (Vertex Shader)
 *    - 负责：处理每个3D模型的顶点
 *    - 任务：位置变换、投影、光照计算
 *    - 输入：顶点坐标、法线、纹理坐标
 *    - 输出：屏幕坐标、颜色、纹理坐标
 * 
 * 2. 片段着色器 (Fragment Shader / Pixel Shader)
 *    - 负责：决定每个像素的最终颜色
 *    - 任务：纹理采样、光照计算、材质渲染
 *    - 输入：插值后的顶点属性
 *    - 输出：像素颜色
 * 
 * 3. 计算着色器 (Compute Shader)
 *    - 负责：执行通用的并行计算任务
 *    - 应用：物理模拟、图像处理、AI计算
 * 
 * 4. 光线追踪着色器 (Ray Tracing Shaders)
 *    - 用于实现高质量的光照和反射效果
 *    - 需要RTX等支持硬件加速的GPU
 * 
 * 三、GLSL vs SPIR-V：
 * 
 * GLSL (OpenGL Shading Language):
 * - 人类可读的高级语言，类似C语言
 * - 具有完整的类型系统和数学函数库
 * - 支持模块化编程（#include、函数库）
 * 
 * SPIR-V (Standard Portable Intermediate Representation - Vulkan):
 * - 二进制中间表示形式，类似于JVM字节码
 * - 跨语言、跨平台，可以从多种语言生成
 * - 性能优化，加载速度快
 * - 可移植性强，兼容性好
 * 
 * 四、编译流程详解：
 * 
 * 1. 预处理 (Preprocessing):
 *    - 展开 #include 指令
 *    - 处理 #define 宏定义
 *    - 条件编译 (#ifdef, #endif)
 * 
 * 2. 语法分析 (Parsing):
 *    - 检查语法错误
 *    - 构建抽象语法树 (AST)
 * 
 * 3. 语义分析 (Semantic Analysis):
 *    - 类型检查
 *    - 变量作用域检查
 *    - 函数调用验证
 * 
 * 4. 代码优化 (Optimization):
 *    - 死代码消除
 *    - 常数折叠
 *    - 循环展开
 * 
 * 5. 代码生成 (Code Generation):
 *    - 生成SPIR-V字节码
 *    - 元数据生成
 * 
 * 五、实际应用场景：
 * 
 * 1. 游戏引擎：
 *    - 渲染管线配置
 *    - 材质系统
 *    - 后处理效果
 * 
 * 2. CAD/3D建模软件：
 *    - 技术可视化
 *    - 实时预览
 * 
 * 3. 科学计算：
 *    - 数值模拟
 *    - 数据可视化
 * 
 * 4. 机器学习：
 *    - GPU加速的神经网络计算
 * 
 * 六、最佳实践：
 * 
 * 1. 性能优化：
 *    - 使用.spv预编译文件减少运行时编译
 *    - 合理使用编译选项
 * 
 * 2. 调试和维护：
 *    - 使用有意义的着色器和变量名称
 *    - 保持代码模块化和可重用性
 *    - 在Debug模式下保留调试信息
 * 
 * 3. 跨平台兼容：
 *    - 使用标准GLSL语法
 *    - 避免平台特定的扩展
 * 
 * 这个ShaderModule类封装了所有这些复杂性，为开发者提供了一个简单易用的接口。
 * 通过合理使用这个类，可以大大简化Vulkan应用的着色器管理工作。
 */