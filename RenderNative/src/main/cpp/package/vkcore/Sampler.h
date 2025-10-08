//
// Created by nio on 2025/10/8.
//

#pragma once

#include <string>

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    class Context;

    class Sampler final {
    public:
        MOVABLE_ONLY(Sampler);

        explicit Sampler(const Context &context, VkFilter minFilter, VkFilter magFilter,
                         VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                         VkSamplerAddressMode addressModeW, float maxLod,
                         const std::string &name = "");

        explicit Sampler(const Context &context, VkFilter minFilter, VkFilter magFilter,
                         VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                         VkSamplerAddressMode addressModeW, float maxLod, bool compareEnable,
                         VkCompareOp compareOp, const std::string &name = "");

        ~Sampler() { vkDestroySampler(device_, sampler_, nullptr); };

        [[nodiscard]] VkSampler vkSampler() const { return sampler_; }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;
    };

}  // namespace VulkanCore