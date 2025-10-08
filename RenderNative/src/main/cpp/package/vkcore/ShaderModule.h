//
// Created by nio on 2025/10/8.
//

#pragma once

#ifdef _WIN32
#include <glslang/Public/ShaderLang.h>
#endif

#include <string>
#include <vector>

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    class Context;

    class ShaderModule final {
    public:
        explicit ShaderModule(const Context* context, const std::string& filePath,
                              const std::string& entryPoint,
                              VkShaderStageFlagBits stages, const std::string& name);
        explicit ShaderModule(const Context* context, const std::vector<char>& data,
                              const std::string& entryPoint,
                              VkShaderStageFlagBits stages, const std::string& name);
        explicit ShaderModule(const Context* context, const std::string& filePath,
                              VkShaderStageFlagBits stages, const std::string& name);

        ~ShaderModule();

        VkShaderModule vkShaderModule() const;

        VkShaderStageFlagBits vkShaderStageFlags() const;

        const std::string& entryPoint() const;

    private:
#ifdef _WIN32
        EShLanguage shaderStageFromFileName(const char* fileName);

  std::vector<char> glslToSpirv(const std::vector<char>& data,
                                EShLanguage shaderStage,
                                const std::string& shaderDir,
                                const char* entryPoint);
#endif

        void printShader(const std::vector<char>& data);

        void createShader(const std::string& filePath, const std::string& entryPoint,
                          const std::string& name);

        void createShader(const std::vector<char>& spirv,
                          const std::string& entryPoint, const std::string& name);

    private:
        const Context* context_ = nullptr;
        VkShaderModule vkShaderModule_ = VK_NULL_HANDLE;
        VkShaderStageFlagBits vkStageFlags_;
        std::string entryPoint_;
    };

}  // namespace VulkanCore
