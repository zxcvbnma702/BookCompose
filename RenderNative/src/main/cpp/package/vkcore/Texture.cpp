//
// Created by nio on 2025/7/27.
//

#include "Texture.h"

#include <utility>
#include "Context.h"


VkCore::Texture::Texture(const Context &context, VkImageType type, VkFormat format,
                         VkImageCreateFlags flags, VkImageUsageFlags usageFlags, VkExtent3D extents,
                         uint32_t numMipLevels, uint32_t layerCount,
                         VkMemoryPropertyFlags memoryFlags,
                         bool generateMips, VkSampleCountFlagBits msaaSamples, std::string name,
                         bool multiview, VkImageTiling imageTiling)
        : context_{context},
          vmaAllocator_{context.memoryAllocator()},
          usageFlags_{usageFlags},
          flags_{flags},
          type_{type},
          format_{format},
          extents_{extents},
          ownsVkImage_{true},
          mipLevels_(numMipLevels),
          layerCount_(layerCount),
          multiview_(multiview),
          generateMips_(generateMips),
          msaaSamples_(msaaSamples),
          imageTiling_(imageTiling),
          debugName_(std::move(name)) {

    ASSERT(extents.width > 0 && extents.height > 0,
           "Texture cannot have dimensions equal to 0");
    ASSERT(mipLevels_ > 0, "Texture must have at least one mip level");

    if (generateMips_) {
        // Calculate the number of mip levels required
        mipLevels_ = getMipLevelsCount(extents.width, extents.height);
    }

    ASSERT(!(mipLevels_ > 1 && msaaSamples_ != VK_SAMPLE_COUNT_1_BIT),
           "Multisampled images cannot have more than 1 mip level");

    const VkImageCreateInfo imageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .imageType = type,
            .format = format,
            .extent = extents,
            .mipLevels = mipLevels_,
            .arrayLayers = layerCount_,
            .samples = msaaSamples_,
            .tiling = imageTiling_,
            .usage = usageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    // Configure memory allocation settings for the image
    const VmaAllocationCreateInfo allocCreateInfo = {
            // Request dedicated memory for the image (not suballocated)
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,

            // Automatically choose memory type based on whether it's HOST_VISIBLE
            .usage = memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                     ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
                     : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,

            // Set memory priority to highest (1.0 = most important)
            .priority = 1.0f,
    };

    // Create a Vulkan image with VMA-managed memory
    VK_CHECK(vmaCreateImage(vmaAllocator_, &imageCreateInfo, &allocCreateInfo,
                            &image_, &vmaAllocation_, nullptr));

    // If allocation succeeded, retrieve and store the actual memory size
    if (vmaAllocation_ != nullptr) {
        VmaAllocationInfo allocationInfo;
        vmaGetAllocationInfo(vmaAllocator_, vmaAllocation_, &allocationInfo);
        deviceSize_ = allocationInfo.size;
    }

    const VkImageViewType imageViewType =
            VkCore::imageTypeToImageViewType(type, flags, multiview_);

    viewType_ = imageViewType;

    imageView_ =
            createImageView(context, imageViewType, format_, mipLevels_, layerCount, name);
}

VkCore::Texture::Texture(const VkCore::Context &context, VkDevice device, VkImage image,
                         VkFormat format, VkExtent3D extents, uint32_t numlayers, bool multiview,
                         const std::string &name) :
        context_{context},
        image_{image},
        format_{format},
        extents_{extents},
        layerCount_(numlayers),
        multiview_(multiview),
        ownsVkImage_{false},
        debugName_{name} {

    imageView_ = createImageView(
            context, !multiview_ ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY, format,
            1, layerCount_, name);
}

VkImageView VkCore::Texture::vkImageView(uint32_t mipLevel) {
    ASSERT(mipLevel == UINT32_MAX || mipLevel < mipLevels_, "Invalid mip level");

    if (mipLevel == UINT32_MAX) {
        return imageView_;
    }

    if (imageViewFramebuffers_.find(mipLevel) == imageViewFramebuffers_.end()) {
        const VkImageViewType imageViewType =
                VkCore::imageTypeToImageViewType(type_, flags_, multiview_);

        imageViewFramebuffers_[mipLevel] =
                createImageView(context_, imageViewType, format_, 1, VK_REMAINING_ARRAY_LAYERS,
                                "Image View for Framebuffer: " + debugName_);
    }
    return imageViewFramebuffers_[mipLevel];
}

VkCore::Texture::~Texture() {
    for (const auto imageView: imageViewFramebuffers_) {
        vkDestroyImageView(context_.device(), imageView.second, nullptr);
    }

    vkDestroyImageView(context_.device(), imageView_, nullptr);

    if (ownsVkImage_) {
        vmaDestroyImage(vmaAllocator_, image_, vmaAllocation_);
    }
}

bool VkCore::Texture::isDepth() const {
    return (format_ == VK_FORMAT_D16_UNORM || format_ == VK_FORMAT_D16_UNORM_S8_UINT ||
            format_ == VK_FORMAT_D24_UNORM_S8_UINT || format_ == VK_FORMAT_D32_SFLOAT ||
            format_ == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            format_ == VK_FORMAT_X8_D24_UNORM_PACK32);
}

bool VkCore::Texture::isStencil() const {
    return (format_ == VK_FORMAT_S8_UINT || format_ == VK_FORMAT_D16_UNORM_S8_UINT ||
            format_ == VK_FORMAT_D24_UNORM_S8_UINT ||
            format_ == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

uint32_t VkCore::Texture::pixelSizeInBytes() const { return bytesPerPixel(format_); }

uint32_t VkCore::Texture::numMipLevels() const { return mipLevels_; }

uint32_t VkCore::Texture::getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
}

VkImageView
VkCore::Texture::createImageView(const VkCore::Context &context, VkImageViewType viewType,
                                 VkFormat format, uint32_t numMipLevels, uint32_t layers,
                                 const std::string &name) {
    const VkImageAspectFlags aspectMask =
            isDepth() ? VK_IMAGE_ASPECT_DEPTH_BIT
                      : (isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT);
    const VkImageViewCreateInfo imageViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .flags = /*usageFlags_ &
               VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT ?
                   VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT
                   :*/
            VkImageViewCreateFlags(0),
            .image = image_,
            .viewType = viewType,
            .format = format,
            .components =
                    {
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
            .subresourceRange = {
                    .aspectMask = aspectMask,
                    .baseMipLevel = 0,
                    .levelCount = numMipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = multiview_ ? VK_REMAINING_ARRAY_LAYERS : layers,
            }};

    VkImageView imageView{VK_NULL_HANDLE};
    VK_CHECK(vkCreateImageView(context_.device(), &imageViewInfo, nullptr, &imageView));

    return imageView;
}

void VkCore::Texture::addReleaseBarrier(VkCommandBuffer cmdBuffer, uint32_t srcQueueFamilyIndex,
                                        uint32_t dstQueueFamilyIndex) {
    VkImageMemoryBarrier2 releaseBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .srcQueueFamilyIndex = srcQueueFamilyIndex,
            .dstQueueFamilyIndex = dstQueueFamilyIndex,
            .image = image_,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels_, 0, 1},
    };

    VkDependencyInfo dependency_info{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &releaseBarrier,
    };

    vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
}

void VkCore::Texture::addAcquireBarrier(VkCommandBuffer cmdBuffer, uint32_t srcQueueFamilyIndex,
                                        uint32_t dstQueueFamilyIndex) {
    VkImageMemoryBarrier2 acquireBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            .srcQueueFamilyIndex = srcQueueFamilyIndex,
            .dstQueueFamilyIndex = dstQueueFamilyIndex,
            .image = image_,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels_, 0, 1},
    };

    VkDependencyInfo dependency_info{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &acquireBarrier,
    };

    vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
}

void VkCore::Texture::transitionImageLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout) {
    VkAccessFlags srcAccessMask = VK_ACCESS_NONE;
    VkAccessFlags dstAccessMask = VK_ACCESS_NONE;
    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    constexpr VkPipelineStageFlags depthStageMask =
            0 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    constexpr VkPipelineStageFlags sampledStageMask =
            0 | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    /*std::cerr << "[transImgeLayot] Transitioning image " << debugName_ << " from
       "
              << string_VkImageLayout(layout_) << " to " <<
       string_VkImageLayout(newLayout)
              << std::endl;*/

    auto oldLayout = layout_;

    if (oldLayout == newLayout) {
        return;
    }

    switch (oldLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            sourceStage = depthStageMask;
            srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            sourceStage = depthStageMask | sampledStageMask;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            sourceStage = sampledStageMask;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            sourceStage = VK_PIPELINE_STAGE_HOST_BIT;
            srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            break;

        default:
        ASSERT(false, "Unknown image layout.");
            break;
    }

    switch (newLayout) {
        case VK_IMAGE_LAYOUT_GENERAL:
        case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
            destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstAccessMask =
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            destinationStage = depthStageMask;
            dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            destinationStage = depthStageMask | sampledStageMask;
            dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            destinationStage = sampledStageMask;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            // vkQueuePresentKHR performs automatic visibility operations
            break;

        default:
        ASSERT(false, "Unknown image layout.");
            break;
    }

    const VkImageAspectFlags aspectMask =
            isDepth() ? isStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                                    : VK_IMAGE_ASPECT_DEPTH_BIT
                      : VK_IMAGE_ASPECT_COLOR_BIT;
    const VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = srcAccessMask,
            .dstAccessMask = dstAccessMask,
            .oldLayout = layout_,
            .newLayout = newLayout,
            .image = image_,
            .subresourceRange =
                    {
                            .aspectMask = aspectMask,
                            .baseMipLevel = 0,
                            .levelCount = mipLevels_,
                            .baseArrayLayer = 0,
                            .layerCount = multiview_ ? VK_REMAINING_ARRAY_LAYERS : 1,
                    },
    };
    vkCmdPipelineBarrier(cmdBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    layout_ = newLayout;
}

VkSampleCountFlagBits VkCore::Texture::VkSampleCount() const {
    return msaaSamples_;
}
