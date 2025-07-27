//
// Created by nio on 2025/7/28.
//

#ifndef BOOKCOMPOSE_FRAMEBUFFER_H
#define BOOKCOMPOSE_FRAMEBUFFER_H

#include <memory>
#include <vector>

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    class Context;
    class Texture;

    class Framebuffer final {
    public:
        MOVABLE_ONLY(Framebuffer);

        explicit Framebuffer(const Context &context, VkDevice device, VkRenderPass renderPass,
                             const std::vector<std::shared_ptr<Texture>> &attachments,
                             const std::shared_ptr<Texture> depthAttachment,
                             const std::shared_ptr<Texture> stencilAttachment,
                             const std::string &name = "");

        ~Framebuffer();

        VkFramebuffer vkFramebuffer() const;

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    };

} // VkCore

#endif //BOOKCOMPOSE_FRAMEBUFFER_H
