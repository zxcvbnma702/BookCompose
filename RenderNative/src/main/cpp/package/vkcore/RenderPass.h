//
// Created by nio on 2025/7/27.
//

#ifndef BOOKCOMPOSE_RENDERPASS_H
#define BOOKCOMPOSE_RENDERPASS_H

#include <memory>
#include <string>
#include <vector>

#include "Common.h"
#include "Utils.h"

namespace VkCore{

    class Context;
    class Texture;

    /**
     * <h3>RenderPass Creation Overview</h3>
     *
     * <p>This function is responsible for creating a Vulkan RenderPass,
     * which is a fundamental part of the rendering pipeline.
     * A RenderPass consists of one or more subpasses, each describing
     * how attachments (images) are used during rendering.</p>
     *
     * <h4>Key Components of RenderPass:</h4>
     *
     * <ul>
     *   <li><b>VkAttachmentDescription[]</b><br>
     *       Describes the format, sample count, load/store operations,
     *       and layout transitions for each attachment (color, depth, stencil, etc.).
     *       Each structure includes:
     *       <ul>
     *         <li><code>format</code>: Image format, e.g., VK_FORMAT_B8G8R8A8_UNORM</li>
     *         <li><code>samples</code>: MSAA sample count, e.g., VK_SAMPLE_COUNT_1_BIT</li>
     *         <li><code>loadOp / storeOp</code>: How the attachment is handled before/after the render pass</li>
     *         <li><code>initialLayout / finalLayout</code>: Memory layout of the image at the start/end of the pass</li>
     *       </ul>
     *   </li>
     *
     *   <li><b>VkAttachmentReference[]</b><br>
     *       Used to reference attachments from within a subpass. Each reference specifies:
     *       <ul>
     *         <li><code>attachment</code>: Index into the attachment description array</li>
     *         <li><code>layout</code>: Layout the attachment will be in during the subpass</li>
     *       </ul>
     *   </li>
     *
     *   <li><b>VkSubpassDescription[]</b><br>
     *       Each subpass describes which attachments it uses for color, depth, input, or resolve.
     *       Typically includes:
     *       <ul>
     *         <li><code>colorAttachments[]</code>: References to color outputs</li>
     *         <li><code>depthStencilAttachment</code>: Reference to depth/stencil buffer (optional)</li>
     *         <li><code>resolveAttachments[]</code>: For MSAA resolve (optional)</li>
     *       </ul>
     *   </li>
     *
     *   <li><b>VkSubpassDependency[]</b><br>
     *       Specifies memory and execution dependencies between subpasses or between
     *       a subpass and external stages. Each dependency includes:
     *       <ul>
     *         <li><code>srcSubpass / dstSubpass</code>: Source and destination subpass indices</li>
     *         <li><code>srcStageMask / dstStageMask</code>: Pipeline stages involved</li>
     *         <li><code>srcAccessMask / dstAccessMask</code>: Resource access types involved</li>
     *       </ul>
     *   </li>
     * </ul>
     *
     * <h4>Recommended Creation Workflow:</h4>
     * <ol>
     *   <li>Create <code>VkAttachmentDescription[]</code> to define attachment properties</li>
     *   <li>Create <code>VkAttachmentReference[]</code> for use in subpasses</li>
     *   <li>Set up <code>VkSubpassDescription[]</code> to define subpass behavior</li>
     *   <li>Define <code>VkSubpassDependency[]</code> to manage synchronization</li>
     *   <li>Populate <code>VkRenderPassCreateInfo</code> and call <code>vkCreateRenderPass()</code></li>
     * </ol>
     *
     * <h4>Important Notes:</h4>
     * <ul>
     *   <li>The order of attachments in the framebuffer must match the attachment descriptions</li>
     *   <li>Initial and final layouts should align with pipeline expectations</li>
     *   <li>All attachment indices in references must be valid</li>
     * </ul>
     *
     * <h4>References:</h4>
     * <ul>
     *   <li><a href="https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass">Vulkan Specification: Render Pass</a></li>
     *   <li><code>VkRenderPass</code>, <code>VkAttachmentDescription</code>, <code>VkSubpassDescription</code></li>
     * </ul>
     */
    class RenderPass final{
    public:
        MOVABLE_ONLY(RenderPass);

        RenderPass(const Context& context,
                   std::vector<std::shared_ptr<Texture>> attachments,
                   std::vector<std::shared_ptr<Texture>> resolveAttachments,
                   const std::vector<VkAttachmentLoadOp>& loadOp,
                   const std::vector<VkAttachmentStoreOp>& storeOp,
                   const std::vector<VkImageLayout>& layout, VkPipelineBindPoint bindPoint,
                   const std::string& name = "");

        RenderPass(const Context& context, const std::vector<VkFormat>& formats,
                   const std::vector<VkImageLayout>& initialLayouts,
                   const std::vector<VkImageLayout>& finalLayouts,
                   const std::vector<VkAttachmentLoadOp>& loadOp,
                   const std::vector<VkAttachmentStoreOp>& storeOp,
                   VkPipelineBindPoint bindPoint,
                   std::vector<uint32_t> resolveAttachmentsIndices,
                   uint32_t depthAttachmentIndex, uint32_t stencilAttachmentIndex = UINT32_MAX,
                   VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                   VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                   bool multiview = false, const std::string& name = "");

        // RenderPass2 - Fragment Density Map support
        RenderPass(const Context& context, const std::vector<VkFormat>& formats,
                   const std::vector<VkImageLayout>& initialLayouts,
                   const std::vector<VkImageLayout>& finalLayouts,
                   const std::vector<VkAttachmentLoadOp>& loadOp,
                   const std::vector<VkAttachmentStoreOp>& storeOp,
                   VkPipelineBindPoint bindPoint,
                   std::vector<uint32_t> resolveAttachmentsIndices,
                   uint32_t depthAttachmentIndex, uint32_t fragmentDensityMapIndex,
                   uint32_t stencilAttachmentIndex = UINT32_MAX,
                   VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                   VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                   bool multiview = false, const std::string& name = "");

        ~RenderPass();

        [[nodiscard]] VkRenderPass vkRenderPass() const { return renderPass_; }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkRenderPass renderPass_ = VK_NULL_HANDLE;
    };
}

#endif //BOOKCOMPOSE_RENDERPASS_H
