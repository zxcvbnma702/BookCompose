//
// Created by nio on 2025/7/27.
//

#ifndef BOOKCOMPOSE_TEXTURE_H
#define BOOKCOMPOSE_TEXTURE_H

#include <unordered_map>

#include "Common.h"
#include "Utils.h"
#include "../third_party/vk_mem_alloc.h"

namespace VkCore {

    class Context;

    class Texture final{
    public:
        MOVABLE_ONLY(Texture);

        explicit Texture(const Context& context, VkImageType type, VkFormat format,
                         VkImageCreateFlags flags, VkImageUsageFlags usageFlags,
                         VkExtent3D extents, uint32_t numMipLevels,
                         uint32_t layerCount, VkMemoryPropertyFlags memoryFlags,
                         bool generateMips = false,
                         VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
                         std::string  name = "", bool multiview = false,
                         VkImageTiling = VK_IMAGE_TILING_OPTIMAL);

        // To be used with images that have been created elsewhere (like swapchains,
        // for instance)
        explicit Texture(const Context& context, VkDevice device, VkImage image,
                         VkFormat format, VkExtent3D extents, uint32_t numlayers = 1,
                         bool multiview = false, const std::string& name = "");

        ~Texture();

        [[nodiscard]] VkFormat vkFormat() const { return format_; }

        /**
         * @brief Returns the VkImageView for the specified mip level.
         *
         * If mipLevel == UINT32_MAX, the default full image view is returned.
         * Otherwise, the function returns (or creates) a view for a specific mip level,
         * typically used for rendering into individual mip levels.
         *
         * @param mipLevel The mip level to retrieve the image view for.
         *                 Use UINT32_MAX to access the default view.
         * @return VkImageView The Vulkan image view for the requested mip level.
         */
        VkImageView vkImageView(uint32_t mipLevel = UINT32_MAX);

        [[nodiscard]] VkImage vkImage() const { return image_; }

        [[nodiscard]] VkExtent3D vkExtents() const { return extents_; }

        [[nodiscard]] VkImageLayout vkLayout() const { return layout_; }

        void setImageLayout(VkImageLayout layout) { layout_ = layout; }

        [[nodiscard]] VkDeviceSize vkDeviceSize() const { return deviceSize_; }

        // todo
//        void uploadAndGenMips(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
//                              void* data);
//
//        void uploadOnly(VkCommandBuffer cmdBuffer, const Buffer* stagingBuffer,
//                        void* data, uint32_t layer = 0);

        void addReleaseBarrier(VkCommandBuffer cmdBuffer,
                               uint32_t srcQueueFamilyIndex,
                               uint32_t dstQueueFamilyIndex);

        /**
         * @brief Inserts an acquire barrier to transition ownership of the image between queues.
         *
         * This method is used in multi-queue Vulkan setups to acquire image ownership from a source queue
         * (`srcQueueFamilyIndex`) to a destination queue (`dstQueueFamilyIndex`). It inserts an
         * `VkImageMemoryBarrier2` using `vkCmdPipelineBarrier2`.
         *
         * @param cmdBuffer The command buffer to record the barrier into.
         * @param srcQueueFamilyIndex The source queue family index (previous owner).
         * @param dstQueueFamilyIndex The destination queue family index (new owner).
         */
        void addAcquireBarrier(VkCommandBuffer cmdBuffer,
                               uint32_t srcQueueFamilyIndex,
                               uint32_t dstQueueFamilyIndex);

        /**
         * @brief Transitions the layout of a Vulkan image.
         *
         * This function handles the Vulkan image layout transition using vkCmdPipelineBarrier.
         * It determines the correct source/destination stage masks and access masks based on
         * the old and new image layouts, and emits a memory barrier to synchronize the transition.
         *
         * @param cmdBuffer The command buffer to record the transition barrier into.
         * @param newLayout The desired new layout for the image.
         */
        void transitionImageLayout(VkCommandBuffer cmdBuffer,
                                   VkImageLayout newLayout);

        [[nodiscard]] bool isDepth() const;

        [[nodiscard]] bool isStencil() const;

        [[nodiscard]] uint32_t pixelSizeInBytes() const;

        [[nodiscard]] uint32_t numMipLevels() const;

        void generateMips(VkCommandBuffer cmdBuffer);

        std::vector<std::shared_ptr<VkImageView>> generateViewForEachMips();

        VkSampleCountFlagBits VkSampleCount() const;

    private:
        /**
         * @brief Calculates the number of mipmap levels required for a texture of given size.
         *
         * This uses the formula: floor(log2(max(width, height))) + 1,
         * which gives the total number of mipmap levels from full size down to 1x1.
         *
         * @param texWidth  Width of the texture (in pixels)
         * @param texHeight Height of the texture (in pixels)
         * @return uint32_t Number of mip levels to use when generating mipmaps
         */
        static uint32_t getMipLevelsCount(uint32_t texWidth, uint32_t texHeight) ;

        VkImageView createImageView(const Context& context, VkImageViewType viewType,
                                    VkFormat format, uint32_t numMipLevels,
                                    uint32_t layers, const std::string& name = "");

    private:
        const Context& context_;
        VmaAllocator vmaAllocator_ = nullptr;
        VmaAllocation vmaAllocation_ = nullptr;
        VkDeviceSize deviceSize_ = 0;
        VkImageUsageFlags usageFlags_ = 0;
        VkImageCreateFlags flags_ = 0;
        VkImageType type_ = VK_IMAGE_TYPE_2D;
        VkImage image_ = VK_NULL_HANDLE;
        VkImageView imageView_ = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, VkImageView> imageViewFramebuffers_;
        VkFormat format_ = VK_FORMAT_UNDEFINED;
        VkExtent3D extents_;
        VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        bool ownsVkImage_ = false;
        uint32_t mipLevels_ = 1;
        uint32_t layerCount_ = 1;
        bool multiview_ = false;
        bool generateMips_ = false;
        VkImageViewType viewType_;
        VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;
        VkImageTiling imageTiling_ = VK_IMAGE_TILING_OPTIMAL;
        std::string debugName_;
    };

} // VkCore

#endif //BOOKCOMPOSE_TEXTURE_H
