#pragma once

#include "volk.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

struct FrameData {
  VkCommandPool _graphics_command_pool;
  VkCommandBuffer _graphics_command_buffer;

  VkCommandPool _compute_command_pool;
  VkCommandBuffer _compute_command_buffer;

  VkCommandPool _transfer_command_pool;
  VkCommandBuffer _transfer_command_buffer;
};

constexpr uint32_t FRAME_OVERLAP = 2;

class Renderer {
public:
  Renderer(uint32_t width, uint32_t height);
  ~Renderer();

private:
  VkInstance _instance;
  VkPhysicalDevice _chosen_gpu;
  VkDevice _device;
  VkSurfaceKHR _surface;

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

  GLFWwindow *_window_handle;

  std::array<FrameData, FRAME_OVERLAP> _frames;

  inline FrameData &get_current_frame() {

    static size_t _frame_number = 0;

    return _frames[_frame_number++ % FRAME_OVERLAP];
  };

  bool initVulkan();
  void initSwapchain();

  void createSwapchain(uint32_t width, uint32_t height,
                       VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
  void destroySwapchain();

  void initCommands();
  void initSyncStructures();
};