//
// Created by nio on 2025/7/23.
//

#ifndef BOOKCOMPOSE_COMMANDQUEUEMANAGER_H
#define BOOKCOMPOSE_COMMANDQUEUEMANAGER_H

#include <functional>
#include <string>
#include <vector>

#include "Common.h"
#include "Utils.h"

namespace VkCore {

    class Context;

    /**
     * CommandBuffers are copntainers for the actual commands that are executed by the GPU.
     * To record command buffers,you allocate a command buffer from a command pool.
     * then use vkCmd* family of functions to record commands into the command buffer.
     * Once the commands have been recorded, you can submit them to the queue.
     */
    class CommandQueueManager {
    public:
        explicit CommandQueueManager(
                const Context& context, VkDevice device, uint32_t count,
                uint32_t concurrentNumCommands, uint32_t queueFamilyIndex, VkQueue queue,
                VkCommandPoolCreateFlags flags =
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,  // default is
                // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
                // since we want to
                // record a command
                // buffer every
                // frame, so we want
                // to be able to
                // reset and record
                // over it
                const std::string& name = "");

        ~CommandQueueManager();

        void submit(const VkSubmitInfo* submitInfo);

        void goToNextCmdBuffer();

        // use to synchronize with the GPU
        void waitUntilSubmitIsComplete();
        void waitUntilAllSubmitsAreComplete();

//        void disposeWhenSubmitCompletes(std::shared_ptr<Buffer> buffer);
//        void disposeWhenSubmitCompletes(std::function<void()>&& deallocator);

        VkCommandBuffer getCmdBufferToBegin();

        VkCommandBuffer getCmdBuffer();

        void endCmdBuffer(VkCommandBuffer cmdBuffer);

        [[nodiscard]] uint32_t queueFamilyIndex() const { return queueFamilyIndex_; }


    private:

    private:
        uint32_t commandsInFlight_ = 2;
        uint32_t queueFamilyIndex_ = 0;
        VkQueue queue_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkCommandPool commandPool_ = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers_;
        std::vector<VkFence> fences_;
        std::vector<bool> isSubmitted_;
        uint32_t fenceCurrentIndex_ = 0;
        uint32_t commandBufferCurrentIndex_ = 0;
    };

} // VkCore

#endif //BOOKCOMPOSE_COMMANDQUEUEMANAGER_H
