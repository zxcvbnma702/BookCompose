#ifndef BOOKCOMPOSE_VULKANRENDER_HPP
#define BOOKCOMPOSE_VULKANRENDER_HPP

#include "../Application.hpp"
#include "../package/vkcore/CommandQueueManager.h"
#include "../package/vkcore/Context.h"
#include "../package/vkcore/FrameBuffer.h"
#include "../package/vkcore/PhysicalDevice.h"
#include "../package/vkcore/Pipeline.h"
#include "../package/vkcore/RenderPass.h"
#include "../package/vkcore/ShaderModule.h"
#include "../package/vkcore/SwapChain.h"
#include "../package/vkcore/Texture.h"

class VulkanRender : public Application {

public:
  VulkanRender(AAssetManager *assetManager, const char *vertexShader,
               const char *fragmentShader);

  ~VulkanRender() override = default;

  void run(ANativeWindow *window) override;

private:
  void initVulkan();
  void createSwapChain();
  void createRenderPass();
  void createPipeline();
  void createFramebuffers();
  void createCommandBuffers();
  void drawFrame();

private:
  std::unique_ptr<VkCore::Context> context_;
  VkCore::PhysicalDevice *physicalDevice_ = nullptr;
  VkCore::Swapchain *swapchain_ = nullptr;
  std::unique_ptr<VkCore::RenderPass> renderPass_;
  std::unique_ptr<VkCore::Pipeline> pipeline_;
  std::unique_ptr<VkCore::CommandQueueManager> cmdManager_;
  std::vector<std::unique_ptr<VkCore::Framebuffer>> framebuffers_;

  bool initialized_ = false;
};

#endif // BOOKCOMPOSE_VULKANRENDER_HPP
