#pragma once

#include "volk.h"

#include "allocator/vk_mem_alloc.h"

#include "GLFW/glfw3.h"

#include "resource_handler.h"

#include "pipeline.h"
#include "vertex.h"

#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace render {

struct Swapchain {
  VkSwapchainKHR swapchain;
  std::vector<VkImage> images;
  std::vector<VkImageView> views;
  std::vector<uint64_t> resource_image_indices = {};
  VkExtent2D extent;
};

class RenderContext {
public:
  RenderContext(uint32_t width, uint32_t height,
                const std::string &shader_path);
  ~RenderContext();

  // Public Api Calls

  // Creates Buffer and returns Resource Index
  template <typename T>
  uint64_t createBuffer(uint32_t size,
                        resource_handler::BufferUsages buffer_usage) {

    return resource_handler->createBuffer<T>(size, buffer_usage);
  }

  // Writes to buffer and returns giveon or new offset
  template <typename T>
  uint32_t writeToBuffer(T *data, uint32_t number_of_instances = 1,
                         uint32_t offset = UINT32_MAX) {
    return resource_handler->writeToBuffer(data, number_of_instances, offset);
  }

  resource_handler::ResourceHandle createImage(uint32_t width, uint32_t height,
                                               VkImageUsageFlags usage_flags,
                                               VkFormat image_format) {

    return resource_handler->createImage(width, height, usage_flags,
                                         image_format);
  }

  resource_handler::ResourceHandle loadImage(const std::string &path,
                                               VkImageUsageFlags usage_flags,
                                               VkFormat image_format) {

    return resource_handler->loadImage(path, image_format, usage_flags);
  }

  void writeImage(resource_handler::ResourceHandle handle,
                  const uint8_t *pixels, uint32_t width, uint32_t height) {

    resource_handler->writeImage(handle, pixels, width, height);
  }

  bool windowShouldClose() const;

  void update();

private:
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkSurfaceKHR surface;
  Swapchain swapchain;

  pipeline::PipelineManager *pipeline_manager;
  resource_handler::ResourceHandler *resource_handler;

  VmaAllocator vma_allocator;

  VkQueue graphics_queue;
  uint32_t graphics_queue_index;

  VkQueue transfer_queue;
  uint32_t transfer_queue_index;

  VkCommandBuffer graphics_command_buffer;
  VkCommandPool graphics_command_pool;

  VkCommandBuffer transfer_command_buffer;
  VkCommandPool transfer_command_pool;

  VkSemaphore aquire_image_semaphore;
  VkSemaphore transfer_finished_semaphore;
  std::vector<VkSemaphore> rendering_finished_semaphores;

  VkFence fence;

  GLFWwindow *window;

  void recreateSwapchain(uint32_t width, uint32_t height);

  void render();

  void createSwapchain(bool has_old_swapchain = false,
                       Swapchain old_swapchain = {});
};
} // namespace render