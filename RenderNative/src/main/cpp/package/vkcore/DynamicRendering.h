#pragma once
#include "Common.h"
#include "Utils.h"
#include "vk_mem_alloc.h"

namespace VkCore {

    class DynamicRendering final {
    public:
        struct AttachmentDescription {
            VkImageView imageView;
            VkImageLayout imageLayout;
            VkResolveModeFlagBits resolveModeFlagBits = VK_RESOLVE_MODE_NONE;
            VkImageView resolveImageView = VK_NULL_HANDLE;
            VkImageLayout resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkAttachmentLoadOp attachmentLoadOp;
            VkAttachmentStoreOp attachmentStoreOp;
            VkClearValue clearValue;
        };

        static std::string instanceExtensions();

        static void beginRenderingCmd(
                VkCommandBuffer commandBuffer, VkImage image, VkRenderingFlags renderingFlags,
                VkRect2D rectRenderSize, uint32_t layerCount, uint32_t viewMask,
                std::vector<AttachmentDescription> colorAttachmentDescList,
                const AttachmentDescription* depthAttachmentDescList,
                const AttachmentDescription* stencilAttachmentDescList,
                VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        static void endRenderingCmd(
                VkCommandBuffer commandBuffer, VkImage image,
                VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VkImageLayout newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    };

}  // namespace VulkanCore