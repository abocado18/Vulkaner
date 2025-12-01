#pragma once

#include "platform/render/pipeline.h"
#include "platform/render/render_object.h"
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

constexpr uint32_t FRAME_OVERLAP = 2;


class IRenderer {


  virtual void draw(std::vector<RenderObject> &render_objects);



};


class Renderer : IRenderer {
public:
  Renderer(uint32_t width, uint32_t height);
  ~Renderer();

  inline bool shouldUpdate() const {
    return !glfwWindowShouldClose(_window_handle);
  }

  void draw(std::vector<RenderObject> &render_objects);

  // Move to Resources later
  Buffer createBuffer(size_t alloc_size, VkBufferUsageFlags usage,
                      VmaMemoryUsage memory_usage);

  void destroyBuffer(const Buffer &buffer);

 

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

  Image _draw_image;

  VkDescriptorSet _draw_image_descriptors;
  VkDescriptorSetLayout _draw_image_descriptor_layout;

  VkPipelineLayout _gradient_pipeline_layout;
  VkPipeline _gradient_pipeline;

  VkDescriptorPool _imm_pool;

  std::array<FrameData, FRAME_OVERLAP> _frames;

  ResourceManager *_resource_manager;

  size_t _frame_number = 0;

  inline FrameData &getCurrentFrame() {

    return _frames[_frame_number % FRAME_OVERLAP];
  };

  bool initVulkan();
  void initSwapchain();

  void createSwapchain(uint32_t width, uint32_t height,
                       VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
  void destroySwapchain();

  void resizeSwapchain();

  void initDrawImage();

  void initCommands();

  void initSyncStructures();

  void initDescriptors();

  void initPipelines();
  void initBackgroundPipelines();

  void initImgui();

  void drawBackground(VkCommandBuffer cmd);
};