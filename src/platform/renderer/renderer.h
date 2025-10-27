#pragma once

#include "volk.h"

#include "allocator/vk_mem_alloc.h"

#include "GLFW/glfw3.h"

#include "resource_handler.h"

#include "pipeline.h"
#include "vertex.h"

#include <cstdint>
#include <vector>

namespace render {

struct Swapchain {
  VkSwapchainKHR swapchain;
  std::vector<VkImage> images;
  std::vector<VkImageView> views;
  std::vector<uint64_t> resource_image_indices = {};
  VkExtent2D extent;
};

/// Virtual class that contains all functions used for rendering
class IRenderContext {
public:
  IRenderContext() = default;
  ~IRenderContext() = default;

  virtual bool windowShouldClose() const = 0;

  virtual void update() = 0;
};

class RenderContext : public IRenderContext {
public:
  RenderContext(uint32_t width, uint32_t height,
                const std::string &shader_path);
  ~RenderContext();

  bool windowShouldClose() const override;

  void update() override;

private:
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkSurfaceKHR surface;
  Swapchain swapchain;


  pipeline::PipelineManager *pipeline_manager;
  resource_handler::ResourceHandler * resource_handler;

  VmaAllocator vma_allocator;

  VkQueue graphics_queue;
  uint32_t graphics_queue_index;

  VkCommandBuffer primary_command_buffer;
  VkCommandPool command_pool;

  VkSemaphore aquire_image_semaphore;
  std::vector<VkSemaphore> rendering_finished_semaphores;

  VkFence fence;

  GLFWwindow *window;

  void recreateSwapchain(uint32_t width, uint32_t height);

  void render();

  void createSwapchain(bool has_old_swapchain = false,
                       Swapchain old_swapchain = {});
};
} // namespace render