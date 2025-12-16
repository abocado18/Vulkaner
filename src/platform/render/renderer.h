#pragma once

#include "platform/render/pipeline.h"
#include "platform/render/render_object.h"
#include "platform/render/vulkan_macros.h"
#include "volk.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "allocator/vk_mem_alloc.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "deletion_queue.h"

#include "resources.h"

struct FrameData {
  VkCommandPool _graphics_command_pool;
  VkCommandBuffer _graphics_command_buffer;

  VkCommandPool _compute_command_pool;
  VkCommandBuffer _compute_command_buffer;

  VkCommandPool _transfer_command_pool;
  VkCommandBuffer _transfer_command_buffer;

  VkSemaphore _swapchain_semaphore;
  VkSemaphore _transfer_semaphore;
  VkSemaphore _compute_semaphore;
  std::vector<VkSemaphore> _render_semaphores;

  VkFence _render_fence;

  DeletionQueue<> _deletion_queue;
};



// Common Renderer Abstraction Interface, to do: replace vulkan objects with
// general ones once you have a second graphics api
class IRenderer {
public:

  virtual ~IRenderer() = default;

  virtual void draw(RenderFrame _render_frame) = 0;

  virtual ResourceHandle createBuffer(size_t size,
                                      VkBufferUsageFlags usage_flags) = 0;

  virtual BufferHandle
  writeBuffer(ResourceHandle handle, void *data, uint32_t size,
              uint32_t offset = UINT32_MAX,
              VkAccessFlags new_buffer_access_flags = VK_ACCESS_NONE) = 0;

  virtual ResourceHandle
  createImage(std::array<uint32_t, 3> extent, VkImageType image_type,
              VkFormat image_format, VkImageUsageFlags image_usage,
              VkImageViewType view_type, VkImageAspectFlags aspect_mask,
              bool create_mipmaps, uint32_t array_layers) = 0;

  virtual void
  writeImage(ResourceHandle handle, void *data, uint32_t size,
             std::array<uint32_t, 3> offset = {0, 0, 0},
             VkImageLayout new_layout = VK_IMAGE_LAYOUT_GENERAL) = 0;

  virtual bool shouldRun() = 0;
};

class VulkanRenderer : public IRenderer {
public:
  VulkanRenderer(uint32_t width, uint32_t height);
  ~VulkanRenderer() override;

  inline bool shouldRun() override {
    return !glfwWindowShouldClose(_window_handle);
  }

  void draw(RenderFrame _render_frame) override;

  ResourceHandle createBuffer(size_t size,
                              VkBufferUsageFlags usage_flags) override;

  BufferHandle
  writeBuffer(ResourceHandle handle, void *data, uint32_t size,
              uint32_t offset = UINT32_MAX,
              VkAccessFlags new_buffer_access_flags = VK_ACCESS_NONE) override;

  ResourceHandle
  createImage(std::array<uint32_t, 3> extent, VkImageType image_type,
              VkFormat image_format, VkImageUsageFlags image_usage,
              VkImageViewType view_type, VkImageAspectFlags aspect_mask,
              bool create_mipmaps, uint32_t array_layers) override;

  void writeImage(ResourceHandle handle, void *data, uint32_t size,
                  std::array<uint32_t, 3> offset = {0, 0, 0},
                  VkImageLayout new_layout = VK_IMAGE_LAYOUT_GENERAL) override;

private:
  VkInstance _instance;
  VkPhysicalDevice _chosen_gpu;
  VkDevice _device;
  VkSurfaceKHR _surface;

  VmaAllocator _allocator;

  PipelineManager *_pipeline_manager;

  VkDebugUtilsMessengerEXT _debug_messenger;

  VkSwapchainKHR _swapchain;
  VkFormat _swapchain_image_format;
  std::vector<VkImage> _swapchain_images;
  std::vector<VkImageView> _swapchain_images_views;
  VkExtent2D _swapchain_extent;

  VkQueue _graphics_queue;
  uint32_t _graphics_queue_family;

  VkQueue _transfer_queue;
  uint32_t _transfer_queue_family;

  VkQueue _compute_queue;
  uint32_t _compute_queue_family;

  bool _isInitialized;

  bool _dedicated_transfer;
  bool _dedicated_compute;

  bool _resized_requested;

  GLFWwindow *_window_handle;

  DeletionQueue<> _main_deletion_queue;

  DescriptorAllocatorGrowable _global_descriptor_allocator;





  VkDescriptorPool _imm_pool;

  std::array<FrameData, FRAMES_IN_FLIGHT> _frames;

  ResourceManager *_resource_manager;

  size_t _frame_number = 0;

  inline FrameData &getCurrentFrame() {

    return _frames[_frame_number % FRAMES_IN_FLIGHT];
  };

  bool initVulkan();
  void initSwapchain();

  void createSwapchain(uint32_t width, uint32_t height,
                       VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
  void destroySwapchain();

  void resizeSwapchain();



  void initCommands();

  void initSyncStructures();

  void initDescriptors();



  void initImgui();

};