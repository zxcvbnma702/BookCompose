//
// Created by nio on 2025/7/23.
//

#include "CommandQueueManager.h"

namespace VkCore {
    CommandQueueManager::CommandQueueManager(const Context &context, VkDevice device,
                                             uint32_t count, uint32_t concurrentNumCommands,
                                             uint32_t queueFamilyIndex, VkQueue queue,
                                             VkCommandPoolCreateFlags flags,
                                             const std::string &name) : device_(device),
                                             queue_(queue) , queueFamilyIndex_(queueFamilyIndex),
                                             commandsInFlight_(concurrentNumCommands){

        fences_.reserve(commandsInFlight_);
        isSubmitted_.reserve(commandsInFlight_);

        /**
         * Create a command pool for the queue
         */
        const VkCommandPoolCreateInfo commandPoolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = flags,
                .queueFamilyIndex = queueFamilyIndex,
        };

        VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool_))

        /**
         * Create a command buffer for the queue
         */
        const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = commandPool_,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
        };

        for(size_t i = 0; i< count; i++){
            VkCommandBuffer cmdBuffer;
            VK_CHECK(vkAllocateCommandBuffers(device_, &commandBufferAllocateInfo, &cmdBuffer))
            commandBuffers_.push_back(cmdBuffer);
        }

        for(size_t i = 0; i< commandsInFlight_; i++){
            VkFence fence;
            const VkFenceCreateInfo fenceCreateInfo{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT
            };
            VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence))
            fences_.push_back(fence);
            isSubmitted_.push_back(false);
        }
    }

    void CommandQueueManager::submit(const VkSubmitInfo *submitInfo) {
        VK_CHECK(vkResetFences(device_, 1, &fences_[fenceCurrentIndex_]))
        VK_CHECK(vkQueueSubmit(queue_, 1, submitInfo, fences_[fenceCurrentIndex_]))
        isSubmitted_[fenceCurrentIndex_] = true;
    }

    void CommandQueueManager::goToNextCmdBuffer() {
        commandBufferCurrentIndex_ = (commandBufferCurrentIndex_ + 1) % static_cast<uint32_t>(commandBuffers_.size());
        fenceCurrentIndex_ = (fenceCurrentIndex_ + 1) % static_cast<uint32_t>(fences_.size());
    }

    void CommandQueueManager::waitUntilSubmitIsComplete() {
        if(!isSubmitted_[fenceCurrentIndex_]){
            return;
        }

        const VkResult result = vkWaitForFences(device_, 1, &fences_[fenceCurrentIndex_], VK_TRUE, UINT32_MAX);
        if(result == VK_TIMEOUT){
            std::cerr << "vkWaitForFences timeout" << std::endl;
            vkDeviceWaitIdle(device_);
        }

        // reset fence status
        isSubmitted_[fenceCurrentIndex_] = false;
    }

    void CommandQueueManager::waitUntilAllSubmitsAreComplete(){
        size_t index = 0;
        for(auto& fence: fences_){
            VK_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT32_MAX))
            VK_CHECK(vkResetFences(device_, 1, &fence))
            isSubmitted_[index++] = false;
        }
    };

    VkCommandBuffer CommandQueueManager::getCmdBufferToBegin() {
        VK_CHECK(vkWaitForFences(device_, 1, &fences_[fenceCurrentIndex_], VK_TRUE, UINT32_MAX))
        VK_CHECK(vkResetCommandBuffer(commandBuffers_[commandBufferCurrentIndex_], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT))

        const VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_CHECK(vkBeginCommandBuffer(commandBuffers_[commandBufferCurrentIndex_], &beginInfo))
        return commandBuffers_[commandBufferCurrentIndex_];
    }

    VkCommandBuffer CommandQueueManager::getCmdBuffer(){
        const VkCommandBufferAllocateInfo commandBufferInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = commandPool_,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
        };
        VkCommandBuffer cmdBuffer{VK_NULL_HANDLE};
        VK_CHECK(vkAllocateCommandBuffers(device_, &commandBufferInfo, &cmdBuffer));

        return cmdBuffer;
    }

    void CommandQueueManager::endCmdBuffer(VkCommandBuffer cmdBuffer){
        VK_CHECK(vkEndCommandBuffer(cmdBuffer))
    }

    CommandQueueManager::~CommandQueueManager() {
        for(auto & fence : fences_){
            vkDestroyFence(device_, fence, nullptr);
        }

        for (auto & commandBuffer : commandBuffers_) {
            vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        }

        vkDestroyCommandPool(device_, commandPool_, nullptr);
    }
} // VkCore