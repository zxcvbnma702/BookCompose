//
// Created by Nio on 2025/5/19.
//

#include "VulkanRender.hpp"
#include <android/asset_manager.h>
#include <android/native_window.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

// Helper to read asset
std::vector<char> readAsset(AAssetManager *assetManager,
                            const std::string &filename) {
  AAsset *asset =
      AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_BUFFER);
  if (!asset) {
    return {};
  }
  size_t size = AAsset_getLength(asset);
  std::vector<char> buffer(size);
  AAsset_read(asset, buffer.data(), size);
  AAsset_close(asset);
  return buffer;
}

VulkanRender::VulkanRender(AAssetManager *assetManager,
                           const char *vertexShader,
                           const char *fragmentShader) {
  this->assetManager = assetManager;
  this->vertexShader = vertexShader;
  this->fragmentShader = fragmentShader;
}

void VulkanRender::run(ANativeWindow *window) {
  this->window = window;
  initVulkan();
}

void VulkanRender::initVulkan() {
  if (initialized_)
    return;

  // 1. Create Context
  std::vector<std::string> instanceExtensions = {"VK_KHR_surface",
                                                 "VK_KHR_android_surface"};
  std::vector<std::string> deviceExtensions = {"VK_KHR_swapchain"};

  context_ = std::make_unique<VkCore::Context>(
      window, std::vector<std::string>{}, // layers
      instanceExtensions, deviceExtensions, VK_QUEUE_GRAPHICS_BIT);

  // 2. Create PhysicalDevice
  // 2. Get PhysicalDevice from Context
  physicalDevice_ =
      const_cast<VkCore::PhysicalDevice *>(&context_->physicalDevice());

  // Actually, Context manages the device creation.
  // The Context constructor I used:
  // Context(void* window, ..., requestedQueueTypes, ...)
  // It creates instance, surface, selects physical device, creates logical
  // device. So context_->device() is valid. context_->physicalDevice() returns
  // a PhysicalDevice object. So I don't need to create PhysicalDevice manually?
  // The Context class has a member `physicalDevice_`.
  // Let's check Context.h again to be sure.

  // Re-reading Context.h from memory (or I should view it if unsure).
  // Context has `const PhysicalDevice& physicalDevice() const`.
  // So I can use that. I don't need to create my own PhysicalDevice unique_ptr
  // if Context owns it. But in my header I defined
  // `std::unique_ptr<VkCore::PhysicalDevice> physicalDevice_;` Maybe I should
  // remove it from header if I use the one from Context. Or maybe I need it for
  // something else? Let's assume Context owns it.

  // 3. Create SwapChain
  createSwapChain();

  // 4. Create RenderPass
  createRenderPass();

  // 5. Create Pipeline
  createPipeline();

  // 6. Create Framebuffers
  createFramebuffers();

  // 7. Create CommandQueueManager
  createCommandBuffers();

  initialized_ = true;

  // 8. Draw frame
  drawFrame();
}

void VulkanRender::createSwapChain() {
  int32_t width = ANativeWindow_getWidth(window);
  int32_t height = ANativeWindow_getHeight(window);

  // Use VK_FORMAT_R8G8B8A8_UNORM (37) which is widely supported on Android.
  // VK_FORMAT_B8G8R8A8_SRGB (50) or (59) is causing "No map for format" errors
  // on this device.
  context_->createSwapchain(
      VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
      VK_PRESENT_MODE_FIFO_KHR,
      VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)});

  swapchain_ = context_->swapchain();
}

void VulkanRender::createRenderPass() {
  // Simple render pass with color attachment
  std::vector<VkFormat> formats = {swapchain_->imageFormat()};
  std::vector<VkImageLayout> initialLayouts = {VK_IMAGE_LAYOUT_UNDEFINED};
  std::vector<VkImageLayout> finalLayouts = {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
  std::vector<VkAttachmentLoadOp> loadOps = {VK_ATTACHMENT_LOAD_OP_CLEAR};
  std::vector<VkAttachmentStoreOp> storeOps = {VK_ATTACHMENT_STORE_OP_STORE};

  renderPass_ = std::make_unique<VkCore::RenderPass>(
      *context_, formats, initialLayouts, finalLayouts, loadOps, storeOps,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      std::vector<uint32_t>{}, // resolveAttachmentsIndices
      UINT32_MAX               // depthAttachmentIndex
  );
}

void VulkanRender::createPipeline() {
  // Load shaders from assets
  auto vertData = readAsset(assetManager, "shaders/vert.spv");
  auto fragData = readAsset(assetManager, "shaders/frag.spv");

  auto vertShader = std::make_shared<VkCore::ShaderModule>(
      context_.get(), vertData, "main",
      VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, "Vertex Shader");

  auto fragShader = std::make_shared<VkCore::ShaderModule>(
      context_.get(), fragData, "main",
      VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, "Fragment Shader");

  VkCore::Pipeline::GraphicsPipelineDescriptor desc;
  desc.vertexShader_ = vertShader;
  desc.fragmentShader_ = fragShader;
  desc.colorTextureFormats = {swapchain_->imageFormat()};
  desc.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
  desc.cullMode = VK_CULL_MODE_NONE;
  desc.frontFace = VK_FRONT_FACE_CLOCKWISE;

  // Viewport and Scissor
  desc.viewport = VkCore::Pipeline::ViewPort(swapchain_->extent());

  pipeline_ = std::make_unique<VkCore::Pipeline>(context_.get(), desc,
                                                 renderPass_->vkRenderPass());
}

void VulkanRender::createFramebuffers() {
  framebuffers_.resize(swapchain_->numberImages());
  for (uint32_t i = 0; i < swapchain_->numberImages(); i++) {
    std::vector<std::shared_ptr<VkCore::Texture>> attachments;
    attachments.push_back(swapchain_->texture(i));

    framebuffers_[i] = std::make_unique<VkCore::Framebuffer>(
        *context_, context_->device(), renderPass_->vkRenderPass(), attachments,
        nullptr, // depth
        nullptr, // stencil
        "Framebuffer " + std::to_string(i));
  }
}

void VulkanRender::createCommandBuffers() {
  // Queue family index 0 is usually graphics on Android?
  // Better to get it from PhysicalDevice.
  uint32_t graphicsQueueFamily =
      context_->physicalDevice().graphicsFamilyIndex().value();

  // Get queue
  VkQueue graphicsQueue;
  vkGetDeviceQueue(context_->device(), graphicsQueueFamily, 0, &graphicsQueue);

  cmdManager_ = std::make_unique<VkCore::CommandQueueManager>(
      *context_, context_->device(), swapchain_->numberImages(),
      2, // frames in flight
      graphicsQueueFamily, graphicsQueue);
}

void VulkanRender::drawFrame() {
  // 1. Acquire image
  auto texture = swapchain_->acquireImage();
  uint32_t imageIndex = swapchain_->currentImageIndex();

  // 2. Get command buffer
  VkCommandBuffer cmd = cmdManager_->getCmdBufferToBegin();

  // 3. Begin Render Pass
  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass_->vkRenderPass();
  renderPassInfo.framebuffer = framebuffers_[imageIndex]->vkFramebuffer();
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapchain_->extent();
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // 4. Bind Pipeline
  pipeline_->bind(cmd);

  // 5. Draw
  vkCmdDraw(cmd, 3, 1, 0, 0);

  // 6. End Render Pass
  vkCmdEndRenderPass(cmd);

  cmdManager_->endCmdBuffer(cmd);

  // 7. Submit
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSubmitInfo submitInfo =
      swapchain_->createSubmitInfo(&cmd, waitStages, true, true);
  cmdManager_->submit(&submitInfo);

  // 8. Present
  swapchain_->present();

  // Wait for completion for this simple example
  cmdManager_->waitUntilSubmitIsComplete();
}
