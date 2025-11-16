#pragma once

#include "volk.h"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

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

  GLFWwindow *_window_handle;

  bool initVulkan();
  void initSwapchain();

  void createSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
  void destroySwapchain();

  void initCommands();
  void initSyncStructures();
};