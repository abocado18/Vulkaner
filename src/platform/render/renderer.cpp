#include "renderer.h"
#include "GLFW/glfw3.h"
#include "game/plugins/render_plugin.h"
#include "platform/math/math.h"
#include "platform/render/allocator/vk_mem_alloc.h"
#include "platform/render/pipeline.h"
#include "platform/render/render_object.h"
#include "platform/render/resources.h"
#include "platform/render/vk_utils.h"
#include "platform/render/vulkan_macros.h"
#include "vulkan/vulkan_core.h"
#include <X11/Xmd.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sys/types.h>
#include <vector>

#include "VkBootstrap.h"

#ifdef PRODUCTION_BUILD

constexpr bool useValidationLayers = false;

#else

constexpr bool useValidationLayers = true;

#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *userData) {

  std::cerr << "[VULKAN DEBUG] " << callbackData->pMessage << "\n" << std::endl;
  return VK_FALSE;
}

VulkanRenderer::VulkanRenderer(uint32_t width, uint32_t height)
    : _frame_number(0) {

  _isInitialized = false;
  _resized_requested = false;

  if (glfwInit() == GLFW_FALSE)
    return;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window_handle = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

  glfwSetFramebufferSizeCallback(_window_handle, [](GLFWwindow *window,
                                                    int width, int height) {
    VulkanRenderer *r =
        reinterpret_cast<VulkanRenderer *>(glfwGetWindowUserPointer(window));

    r->resizeSwapchain();
  });

  glfwSetWindowUserPointer(_window_handle, this);

  if (!_window_handle)
    return;

  if (!initVulkan())
    return;

  std::cout << "Create Resources\n";

  initSwapchain();

  initCommands();

  initSyncStructures();

  initDescriptors();

  initImgui();

  PipelineBuilder2 builder{};
  builder.makeGraphicsDefault();
  builder.rendering_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
  builder.rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

  std::vector<VkFormat> gbuffer_pipeline_formats = {
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UNORM,

  };

  builder.rendering_info.colorAttachmentCount = gbuffer_pipeline_formats.size();
  builder.rendering_info.pColorAttachmentFormats =
      gbuffer_pipeline_formats.data();

  auto default_col_attachment = builder.color_blend_attachments[0];
  builder.color_blend_attachments.push_back(default_col_attachment);
  builder.color_blend_attachments.push_back(default_col_attachment);

  assert(builder.color_blend_attachments.size() == 3);

  _pipeline_manager->createGraphicsPipeline(
      builder, std::array<std::string, 4>{"gbuffer", "vertexMain", "gbuffer",
                                          "pixelMain"});

  _isInitialized = true;
}

VulkanRenderer::~VulkanRenderer() {

  if (_isInitialized) {

    std::cout << "Destroy Render Resources\n";

    vkDeviceWaitIdle(_device);

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {

      vkDestroyCommandPool(_device, _frames[i]._graphics_command_pool, nullptr);

      if (_dedicated_compute) {
        vkDestroyCommandPool(_device, _frames[i]._compute_command_pool,
                             nullptr);
      }

      if (_dedicated_transfer) {
        vkDestroyCommandPool(_device, _frames[i]._transfer_command_pool,
                             nullptr);
      }

      vkDestroySemaphore(_device, _frames[i]._swapchain_semaphore, nullptr);
      vkDestroySemaphore(_device, _frames[i]._transfer_semaphore, nullptr);
      vkDestroySemaphore(_device, _frames[i]._compute_semaphore, nullptr);

      for (auto &s : _frames[i]._render_semaphores) {
        vkDestroySemaphore(_device, s, nullptr);
      }

      vkDestroyFence(_device, _frames[i]._render_fence, nullptr);
    }

    delete _resource_manager;

    _main_deletion_queue.flush();

    vkDeviceWaitIdle(_device);

    destroySwapchain();

    vkDestroySurfaceKHR(_instance, _surface, nullptr);

    vkDestroyDevice(_device, nullptr);

    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);

    vkDestroyInstance(_instance, nullptr);

    glfwDestroyWindow(_window_handle);

    glfwTerminate();
  } else {
    std::cout << "Renderer was never initialized\n";
  }
}

void VulkanRenderer::initDescriptors() {

  _resource_manager = new ResourceManager(_device, _chosen_gpu, _allocator);
}

bool VulkanRenderer::initVulkan() {

  volkInitialize();

  {

    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(useValidationLayers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    volkLoadInstance(_instance);

    VK_ERROR(
        glfwCreateWindowSurface(_instance, _window_handle, nullptr, &_surface),
        "Could not create Surface\n");

    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceVulkan11Features features11{};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.shaderDrawParameters = VK_TRUE;

    vkb::PhysicalDeviceSelector selector(vkb_inst);
    vkb::PhysicalDevice physical_device =
        selector.set_minimum_version(1, 3)
            .set_required_features_13(features13)
            .set_required_features_11(features11)
            .set_required_features(features)
            .set_surface(_surface)

            .select()
            .value();

    std::cout << physical_device.name << " is used\n";

    vkb::DeviceBuilder device_builder{physical_device};

    vkb::Device vkb_device = device_builder.build().value();

    _device = vkb_device.device;
    _chosen_gpu = physical_device.physical_device;

    _graphics_queue_family =
        vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    _dedicated_compute = physical_device.has_dedicated_compute_queue();

    _dedicated_transfer = physical_device.has_dedicated_transfer_queue();

    _transfer_queue_family = _graphics_queue_family;
    _compute_queue_family = _graphics_queue_family;

    if (_dedicated_compute) {

      std::cout << physical_device.name << " has dedicated compute queue\n";
      _compute_queue_family =
          vkb_device.get_dedicated_queue_index(vkb::QueueType::compute).value();
    }

    if (_dedicated_transfer) {

      std::cout << physical_device.name << " has dedicated transfer queue\n";
      _transfer_queue_family =
          vkb_device.get_dedicated_queue_index(vkb::QueueType::transfer)
              .value();
    }

    _graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();

    _transfer_queue =
        _dedicated_transfer
            ? vkb_device.get_dedicated_queue(vkb::QueueType::transfer).value()
            : _graphics_queue;

    _compute_queue =
        _dedicated_compute
            ? vkb_device.get_dedicated_queue(vkb::QueueType::compute).value()
            : _graphics_queue;

    volkLoadDevice(_device);
  }

  {
    VmaAllocatorCreateInfo allocator_create_info = {};
    allocator_create_info.physicalDevice = _chosen_gpu;
    allocator_create_info.device = _device;
    allocator_create_info.instance = _instance;

    VmaVulkanFunctions vulkan_functions = {};

    VK_ERROR(vmaImportVulkanFunctionsFromVolk(&allocator_create_info,
                                              &vulkan_functions),
             "Load Vulkan Functions");

    allocator_create_info.pVulkanFunctions = &vulkan_functions;

    VK_ERROR(vmaCreateAllocator(&allocator_create_info, &_allocator),
             "Could not create Allocator");

    _main_deletion_queue.pushFunction(
        [&]() { vmaDestroyAllocator(_allocator); });
  }

  {
    _pipeline_manager = new PipelineManager(SHADER_PATH, _device);

    _main_deletion_queue.pushFunction([&]() { delete _pipeline_manager; });
  }

  return true;
}

void VulkanRenderer::initSwapchain() {
  int width, height;
  glfwGetFramebufferSize(_window_handle, &width, &height);

  createSwapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void VulkanRenderer::createSwapchain(uint32_t width, uint32_t height,
                                     VkSwapchainKHR old_swapchain) {

  vkb::SwapchainBuilder builder(_chosen_gpu, _device, _surface);

  _swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

  vkb::Swapchain swapchain =
      builder
          .set_desired_format(VkSurfaceFormatKHR{
              _swapchain_image_format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          .set_desired_extent(width, height)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_old_swapchain(old_swapchain)
          .build()
          .value();

  _swapchain_extent = swapchain.extent;
  _swapchain = swapchain.swapchain;
  _swapchain_images = swapchain.get_images().value();

  if (old_swapchain != VK_NULL_HANDLE) {

    for (auto &v : _swapchain_images_views) {
      vkDestroyImageView(_device, v, nullptr);
    }

    _swapchain_images_views.clear();

    vkDestroySwapchainKHR(_device, old_swapchain, nullptr);
  }

  _swapchain_images_views = swapchain.get_image_views().value();
}

void VulkanRenderer::destroySwapchain() {

  for (VkImageView &v : _swapchain_images_views) {
    vkDestroyImageView(_device, v, nullptr);
  }

  vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void VulkanRenderer::initCommands() {

  VkCommandPoolCreateInfo graphics_pool_create_info = {};
  graphics_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  graphics_pool_create_info.queueFamilyIndex = _graphics_queue_family;

  for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    VK_ERROR(vkCreateCommandPool(_device, &graphics_pool_create_info, nullptr,
                                 &_frames[i]._graphics_command_pool),
             "Could not create Command Pool");

    VkCommandBufferAllocateInfo alloc_info = {};

    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandBufferCount = 1;
    alloc_info.commandPool = _frames[i]._graphics_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_ERROR(vkAllocateCommandBuffers(_device, &alloc_info,
                                      &_frames[i]._graphics_command_buffer),
             "Could not allocate Command Buffer");
  }

  if (_dedicated_compute) {

    VkCommandPoolCreateInfo compute_pool_create_info = {};
    compute_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    compute_pool_create_info.queueFamilyIndex = _compute_queue_family;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
      VK_ERROR(vkCreateCommandPool(_device, &compute_pool_create_info, nullptr,
                                   &_frames[i]._compute_command_pool),
               "Could not create Command Pool");

      VkCommandBufferAllocateInfo alloc_info = {};

      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandBufferCount = 1;
      alloc_info.commandPool = _frames[i]._compute_command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

      VK_ERROR(vkAllocateCommandBuffers(_device, &alloc_info,
                                        &_frames[i]._compute_command_buffer),
               "Could not allocate Command Buffer");
    }
  } else {

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
      _frames[i]._compute_command_pool = _frames[i]._graphics_command_pool;

      VkCommandBufferAllocateInfo alloc_info = {};

      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandBufferCount = 1;
      alloc_info.commandPool = _frames[i]._compute_command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

      VK_ERROR(vkAllocateCommandBuffers(_device, &alloc_info,
                                        &_frames[i]._transfer_command_buffer),
               "Could not allocate Command Buffer");
    }
  }

  if (_dedicated_transfer) {

    VkCommandPoolCreateInfo transfer_pool_create_info = {};
    transfer_pool_create_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transfer_pool_create_info.queueFamilyIndex = _transfer_queue_family;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
      VK_ERROR(vkCreateCommandPool(_device, &transfer_pool_create_info, nullptr,
                                   &_frames[i]._transfer_command_pool),
               "Could not create Command Pool");

      VkCommandBufferAllocateInfo alloc_info = {};

      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandBufferCount = 1;
      alloc_info.commandPool = _frames[i]._transfer_command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

      VK_ERROR(vkAllocateCommandBuffers(_device, &alloc_info,
                                        &_frames[i]._transfer_command_buffer),
               "Could not allocate Command Buffer");
    }
  } else {

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
      _frames[i]._transfer_command_pool = _frames[i]._graphics_command_pool;

      VkCommandBufferAllocateInfo alloc_info = {};

      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandBufferCount = 1;
      alloc_info.commandPool = _frames[i]._transfer_command_pool;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

      VK_ERROR(vkAllocateCommandBuffers(_device, &alloc_info,
                                        &_frames[i]._transfer_command_buffer),
               "Could not allocate Command Buffer");
    }
  }
}

void VulkanRenderer::initSyncStructures() {

  VkFenceCreateInfo fence_create_info = {};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphore_create_info = {};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {

    VK_ERROR(vkCreateFence(_device, &fence_create_info, nullptr,
                           &_frames[i]._render_fence),
             "Could not create fence");

    _frames[i]._render_semaphores.resize(_swapchain_images.size());

    for (auto &render_semaphore : _frames[i]._render_semaphores) {

      VK_ERROR(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                                 &render_semaphore),
               "Could not create Render Semaphore");
    }

    VK_ERROR(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                               &_frames[i]._swapchain_semaphore),
             "Could not create Swapchain Semaphore");

    VK_ERROR(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                               &_frames[i]._transfer_semaphore),
             "Could not create Transfer Semaphore");

    VK_ERROR(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                               &_frames[i]._compute_semaphore),
             "Could not create Transfer Semaphore");
  }
}

void VulkanRenderer::draw(RenderCamera &camera, std::vector<RenderMesh> &meshes,
                          std::vector<RenderLight> &lights) {

  glfwPollEvents();

  {
    if (_resized_requested) {
      resizeSwapchain();
      return;
    }
  }

  VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame()._render_fence, true,
                           UINT64_MAX),
           "Wait for Fence");

  getCurrentFrame()._deletion_queue.flush();
  _resource_manager->runDeletionQueue();

  VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame()._render_fence),
           "Reset Fence");

  uint32_t swapchain_image_index;

  VkResult aquire_res = vkAcquireNextImageKHR(
      _device, _swapchain, UINT64_MAX, getCurrentFrame()._swapchain_semaphore,
      nullptr, &swapchain_image_index);

  if (aquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
    _resized_requested = true;
    return;
  }

  VkCommandBuffer graphics_command_buffer =
      getCurrentFrame()._graphics_command_buffer;
  VkCommandBuffer transfer_command_buffer =
      getCurrentFrame()._transfer_command_buffer;
  VkCommandBuffer compute_command_buffer =
      getCurrentFrame()._compute_command_buffer;

  VK_CHECK(
      vkResetCommandPool(_device, getCurrentFrame()._graphics_command_pool, 0),
      "Graphics Command Buffer");

  if (_dedicated_compute) {
    VK_CHECK(
        vkResetCommandPool(_device, getCurrentFrame()._compute_command_pool, 0),
        "Graphics Command Buffer");
  }

  if (_dedicated_transfer) {
    VK_CHECK(vkResetCommandPool(_device,
                                getCurrentFrame()._transfer_command_pool, 0),
             "Graphics Command Buffer");
  }

  // Commands start here

#pragma region Write Buffer/Image

  {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(transfer_command_buffer, &begin_info),
             "Start Command Buffer");

    for (size_t write_index = 0;
         write_index < _resource_manager->getWrites().size(); write_index++) {

      ResourceWriteInfo &w = _resource_manager->getWrites()[write_index];

      _resource_manager->commitWrite(transfer_command_buffer, w,
                                     _transfer_queue_family,
                                     _graphics_queue_family);

      // Queue delete staging buffers and clear writes
      Buffer source_buffer = w.source_buffer;

      getCurrentFrame()._deletion_queue.pushFunction([source_buffer, this]() {
        vmaDestroyBuffer(_allocator, source_buffer.buffer,
                         source_buffer.allocation);
      });
    }

    vkEndCommandBuffer(transfer_command_buffer);

    VkSemaphoreSubmitInfo signal_info =
        vk_utils::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                      getCurrentFrame()._transfer_semaphore);

    VkCommandBufferSubmitInfo command_buffer_submit_info =
        vk_utils::commandBufferSubmitInfo(transfer_command_buffer);

    VkSubmitInfo2 submit =
        vk_utils::submitInfo(&command_buffer_submit_info,
                             _dedicated_transfer ? &signal_info : nullptr,
                             _dedicated_transfer ? 1 : 0, nullptr, 0);

    VK_CHECK(vkQueueSubmit2(_transfer_queue, 1, &submit, 0),
             "Submit Transfer Commands");
  }

  _resource_manager->_deletion_queue.pushFunction(
      [](ResourceManager *manager) { manager->clearWrites(); });

#pragma endregion

#pragma region Start Drawing

  constexpr VkExtent3D g_buffer_extent = {1280, 720, 1};

  constexpr TransientImageKey albedo_key = {

      VK_FORMAT_R8G8B8A8_UNORM,
      g_buffer_extent,
      VK_IMAGE_TYPE_2D,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_VIEW_TYPE_2D,
      1,
      1

  };

  constexpr TransientImageKey normal_key = {

      VK_FORMAT_R8G8B8A8_UNORM,
      g_buffer_extent,
      VK_IMAGE_TYPE_2D,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_VIEW_TYPE_2D,
      1,
      1

  };

  constexpr TransientImageKey metallic_roughness_ao_key = {

      VK_FORMAT_R8G8B8A8_UNORM,
      g_buffer_extent,
      VK_IMAGE_TYPE_2D,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_VIEW_TYPE_2D,
      1,
      1

  };

  constexpr TransientImageKey depth_key = {
      VK_FORMAT_D32_SFLOAT,
      g_buffer_extent,
      VK_IMAGE_TYPE_2D,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      VK_IMAGE_ASPECT_DEPTH_BIT,
      VK_IMAGE_VIEW_TYPE_2D,
      1,
      1};

  constexpr const char *GBufferNames[] = {"Albedo", "Depth", "Normal", "MRAO"};

  _resource_manager->registerTransientImage(GBufferNames[0], albedo_key);
  _resource_manager->registerTransientImage(GBufferNames[1], depth_key);
  _resource_manager->registerTransientImage(GBufferNames[2], normal_key);
  _resource_manager->registerTransientImage(GBufferNames[3],
                                            metallic_roughness_ao_key);

  const uint32_t current_frame = _frame_number % FRAMES_IN_FLIGHT;

  Image _albedo_image =
      _resource_manager->getTransientImage(GBufferNames[0], current_frame);

  Image _depth_image =
      _resource_manager->getTransientImage(GBufferNames[1], current_frame);

  Image _normal_image =
      _resource_manager->getTransientImage(GBufferNames[2], current_frame);

  Image _mrao_image =
      _resource_manager->getTransientImage(GBufferNames[3], current_frame);

  {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(graphics_command_buffer, &begin_info),
             "Start Command Buffer");
  }

  {
    // Ownership transfer must happen on dst command buffer, so final wrie
    // barrier happens here

    for (auto &w : _resource_manager->getWrites()) {

      _resource_manager->commitWriteTransmit(graphics_command_buffer, w,
                                             _transfer_queue_family,
                                             _graphics_queue_family);
    }
  }

  _resource_manager->transistionTransientImage(
      GBufferNames[0], current_frame, graphics_command_buffer,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  _resource_manager->transistionTransientImage(
      GBufferNames[1], current_frame, graphics_command_buffer,
      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  _resource_manager->transistionTransientImage(
      GBufferNames[2], current_frame, graphics_command_buffer,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  _resource_manager->transistionTransientImage(
      GBufferNames[3], current_frame, graphics_command_buffer,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkClearValue color_clear_value{};
  color_clear_value.color = {1.0f, 1.0f, 1.0f, 1.0f};

  VkClearValue depth_clear_value{};
  depth_clear_value.depthStencil.depth = 1.0f;

  VkRenderingAttachmentInfo g_buffer_info[] = {

      vk_utils::attachmentInfo(_albedo_image.view, &color_clear_value,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
      vk_utils::attachmentInfo(_normal_image.view, &color_clear_value,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
      vk_utils::attachmentInfo(_mrao_image.view, &color_clear_value,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),

  };

  auto depth_info =
      vk_utils::attachmentInfo(_depth_image.view, &depth_clear_value,
                               VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  auto render_info = vk_utils::renderingInfo(
      g_buffer_info, 3, {g_buffer_extent.width, g_buffer_extent.height}, {0, 0},
      &depth_info, VK_NULL_HANDLE);

  vkCmdBeginRendering(graphics_command_buffer, &render_info);

  VkViewport viewport = {
      0,    0,   (float)g_buffer_extent.width, (float)g_buffer_extent.height,
      0.0f, 1.0f};

  VkRect2D scissor = {0, 0, g_buffer_extent.width, g_buffer_extent.height};

  vkCmdSetViewport(graphics_command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(graphics_command_buffer, 0, 1, &scissor);

  auto p = _pipeline_manager->getPipelineByIdx(0);

  // Camera

  std::vector<CombinedResourceIndexAndDescriptorType> cam_resources(1);
  cam_resources[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  cam_resources[0].idx = camera.camera_data.id;
  cam_resources[0].size = sizeof(GpuCameraData);
  Descriptor cam_desc = _resource_manager->bindResources(
      cam_resources, p.set_layouts[0]); // Bind to first Descriptor Set

  vkCmdBindDescriptorSets(graphics_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout, 0, 1,
                          &cam_desc.set, 1, &camera.camera_data.offset);

  std::vector<CombinedResourceIndexAndDescriptorType> transform_resources(1);

  std::vector<CombinedResourceIndexAndDescriptorType> material_resources(1);

  for (auto &m : meshes) {

    transform_resources[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    transform_resources[0].idx = m.transform.id;
    transform_resources[0].size = sizeof(Mat4<float>);
    Descriptor transform_desc =
        _resource_manager->bindResources(transform_resources, p.set_layouts[1]);

    vkCmdBindDescriptorSets(graphics_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout, 1, 1,
                            &transform_desc.set, 1, &m.transform.offset);

    material_resources[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    material_resources[0].idx = m.material.id;
    material_resources[0].size = sizeof(RenderMaterial);

    size_t img_bind_index = 5; // Img starts at slot 5
    for (auto &img_index : m.images) {

      if (img_bind_index > 5)
        continue; // Only bind albedo for now

      auto &img_to_bind = _resource_manager->getImage(img_index);

      std::vector<CombinedResourceIndexAndDescriptorType> img_to_bind_res(1);
      img_to_bind_res[0].idx = img_index;
      img_to_bind_res[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      img_to_bind_res[0].size = 0;

      Descriptor image_desc = _resource_manager->bindResources(

          img_to_bind_res, p.set_layouts[img_bind_index]);

      vkCmdBindDescriptorSets(graphics_command_buffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout,
                              img_bind_index, 1, &image_desc.set, 0, nullptr);

      img_bind_index++;
    }

    Descriptor mat_desc =
        _resource_manager->bindResources(material_resources, p.set_layouts[2]);

    vkCmdBindDescriptorSets(graphics_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout, 2, 1,
                            &mat_desc.set, 1, &m.material.offset);

    auto &vertex_buffer = _resource_manager->getBuffer(m.vertex.id);

    VkDeviceSize vertex_offset = m.vertex.offset;

    vkCmdBindVertexBuffers(graphics_command_buffer, 0, 1, &vertex_buffer.buffer,
                           &vertex_offset);

    VkDeviceSize index_offset = vertex_offset + m.index_offset;

    vkCmdBindIndexBuffer(graphics_command_buffer, vertex_buffer.buffer,
                         index_offset, VK_INDEX_TYPE_UINT32);

    vkCmdBindPipeline(graphics_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      _pipeline_manager->getPipelineByIdx(0).pipeline);

    vkCmdDrawIndexed(graphics_command_buffer, m.index_count, 1, 0, 0, 0);
  }

  vkCmdEndRendering(graphics_command_buffer);

  _resource_manager->transistionTransientImage(
      GBufferNames[0], current_frame, graphics_command_buffer,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  vk_utils::transistionImage(graphics_command_buffer, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             _swapchain_images[swapchain_image_index]);

  vk_utils::copyImageToImage(
      graphics_command_buffer, _albedo_image.image,
      _swapchain_images[swapchain_image_index],
      {_albedo_image.extent.width, _albedo_image.extent.height},
      _swapchain_extent);

  vk_utils::transistionImage(graphics_command_buffer,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                             _swapchain_images[swapchain_image_index]);

  VK_CHECK(vkEndCommandBuffer(graphics_command_buffer), "End Command Buffer");
  {

    VkCommandBufferSubmitInfo command_submit_info =
        vk_utils::commandBufferSubmitInfo(graphics_command_buffer);

    VkSemaphoreSubmitInfo wait_swapchain_info = vk_utils::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        getCurrentFrame()._swapchain_semaphore);

    VkSemaphoreSubmitInfo wait_transfer_info =
        vk_utils::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                      getCurrentFrame()._transfer_semaphore);

    VkSemaphoreSubmitInfo signal_info = vk_utils::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
        getCurrentFrame()._render_semaphores[swapchain_image_index]);

    VkSubmitInfo2 submit_info;

    if (_dedicated_transfer) {

      VkSemaphoreSubmitInfo wait_infos[] = {wait_swapchain_info,
                                            wait_transfer_info};

      submit_info = vk_utils::submitInfo(&command_submit_info, &signal_info, 1,
                                         wait_infos, 2);

    } else {

      submit_info = vk_utils::submitInfo(&command_submit_info, &signal_info, 1,
                                         &wait_swapchain_info, 1);
    }

    VK_CHECK(vkQueueSubmit2(_graphics_queue, 1, &submit_info,
                            getCurrentFrame()._render_fence),
             "Submit Graphics Commands");
  }

#pragma endregion

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &_swapchain;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores =
      &getCurrentFrame()._render_semaphores[swapchain_image_index];

  present_info.pImageIndices = &swapchain_image_index;

  VkResult present_res = vkQueuePresentKHR(_graphics_queue, &present_info);

  if (present_res == VK_ERROR_OUT_OF_DATE_KHR) {
    _resized_requested = true;
  }

  _resource_manager->resetAllTransientImages(_frame_number % FRAMES_IN_FLIGHT);

  _frame_number++;
}

void VulkanRenderer::initImgui() {

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VK_ERROR(vkCreateDescriptorPool(_device, &pool_info, nullptr, &_imm_pool),
           "Create ImGui Descriptor Pool");

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();

  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui_ImplGlfw_InitForVulkan(_window_handle, true);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.ApiVersion = VK_VERSION_1_3;
  init_info.Instance = _instance;
  init_info.PhysicalDevice = _chosen_gpu;
  init_info.Device = _device;
  init_info.QueueFamily = _graphics_queue_family;
  init_info.Queue = _graphics_queue;
  init_info.DescriptorPool = _imm_pool;
  init_info.MinImageCount = 2;
  init_info.ImageCount = 2;
  init_info.UseDynamicRendering = true;

  init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {};
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount =
      1;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo
      .pColorAttachmentFormats = &_swapchain_image_format;

  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  _main_deletion_queue.pushFunction([&]() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    vkDestroyDescriptorPool(_device, _imm_pool, nullptr);
  });
}

void VulkanRenderer::resizeSwapchain() {
  vkDeviceWaitIdle(_device);

  int width, height;

  glfwGetFramebufferSize(_window_handle, &width, &height);

  if (width == 0 || height == 0) {
    return;
  }

  VkSwapchainKHR old_swapchain = _swapchain;

  createSwapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                  old_swapchain);

  _resized_requested = false;
}

ResourceHandle VulkanRenderer::createBuffer(size_t size,
                                            VkBufferUsageFlags usage_flags) {

  return _resource_manager->createBuffer(size, usage_flags);
}

BufferHandle
VulkanRenderer::writeBuffer(ResourceHandle handle, void *data, uint32_t size,
                            uint32_t offset,
                            VkAccessFlags new_buffer_access_flags) {

  return _resource_manager->writeBuffer(handle, data, size, offset,
                                        new_buffer_access_flags);
}

ResourceHandle VulkanRenderer::createImage(
    std::array<uint32_t, 3> extent, VkImageType image_type,
    VkFormat image_format, VkImageUsageFlags image_usage,
    VkImageViewType view_type, VkImageAspectFlags aspect_mask,
    uint32_t number_mipmaps, uint32_t array_layers) {

  return _resource_manager->createImage(extent, image_type, image_format,
                                        image_usage, view_type, aspect_mask,
                                        number_mipmaps, array_layers);
}

void VulkanRenderer::writeImage(ResourceHandle handle, void *data,
                                uint32_t size, std::array<uint32_t, 3> offset,
                                std::span<MipMapData> mipmap_data,
                                VkImageLayout new_layout) {

  _resource_manager->writeImage(handle, data, size, offset, mipmap_data,
                                new_layout);
}