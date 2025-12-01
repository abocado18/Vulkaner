#include "renderer.h"
#include "GLFW/glfw3.h"
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

Renderer::Renderer(uint32_t width, uint32_t height) : _frame_number(0) {

  _isInitialized = false;
  _resized_requested = false;

  if (glfwInit() == GLFW_FALSE)
    return;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window_handle = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

  glfwSetFramebufferSizeCallback(
      _window_handle, [](GLFWwindow *window, int width, int height) {
        Renderer *r =
            reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));

        r->resizeSwapchain();
      });

  glfwSetWindowUserPointer(_window_handle, this);

  if (!_window_handle)
    return;

  if (!initVulkan())
    return;

  std::cout << "Create Resources\n";

  initSwapchain();

  initDrawImage();

  initCommands();

  initSyncStructures();

  initDescriptors();

  initPipelines();

  initImgui();

  _isInitialized = true;
}

Renderer::~Renderer() {

  if (_isInitialized) {

    std::cout << "Destroy Render Resources\n";

    vkDeviceWaitIdle(_device);

    for (size_t i = 0; i < FRAME_OVERLAP; i++) {

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

    _main_deletion_queue.flush();

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

void Renderer::initDescriptors() {

  _resource_manager = new ResourceManager(_device, _chosen_gpu, _allocator);

  _main_deletion_queue.pushFunction([&] { delete _resource_manager; });

  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
  };

  _global_descriptor_allocator.init(_device, 10, sizes);
  {
    DescriptorSetLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _draw_image_descriptor_layout =
        builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  _draw_image_descriptors = _global_descriptor_allocator.allocate(
      _device, _draw_image_descriptor_layout);

  DescriptorWriter writer;
  writer.writeImage(0, _draw_image.view, VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  writer.updateSet(_device, _draw_image_descriptors);

  _main_deletion_queue.pushFunction([&]() {
    _global_descriptor_allocator.destroyPools(_device);

    vkDestroyDescriptorSetLayout(_device, _draw_image_descriptor_layout,
                                 nullptr);
  });
}

bool Renderer::initVulkan() {

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

    VkPhysicalDeviceVulkan13Features features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector(vkb_inst);
    vkb::PhysicalDevice physical_device =
        selector.set_minimum_version(1, 3)
            .set_required_features_13(features)
            .set_required_features_12(features12)
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
    allocator_create_info.flags =
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

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

void Renderer::initSwapchain() {
  int width, height;
  glfwGetFramebufferSize(_window_handle, &width, &height);

  createSwapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void Renderer::createSwapchain(uint32_t width, uint32_t height,
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

void Renderer::destroySwapchain() {

  for (VkImageView &v : _swapchain_images_views) {
    vkDestroyImageView(_device, v, nullptr);
  }

  vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void Renderer::initDrawImage() {
  {
    VkExtent3D draw_image_extent = {_swapchain_extent.width,
                                    _swapchain_extent.height, 1};

    _draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    _draw_image.extent = draw_image_extent;

    VkImageUsageFlags draw_usage_flags =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo draw_img_create_info = vk_utils::imageCreateInfo(
        _draw_image.format, draw_usage_flags, draw_image_extent);

    VmaAllocationCreateInfo draw_img_alloc_info = {};
    draw_img_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    draw_img_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_ERROR(vmaCreateImage(_allocator, &draw_img_create_info,
                            &draw_img_alloc_info, &_draw_image.image,
                            &_draw_image.allocation, nullptr),
             "Could not create Draw Image");

    VkImageViewCreateInfo draw_img_view_create_info =
        vk_utils::imageViewCreateInfo(_draw_image.format, _draw_image.image,
                                      VK_IMAGE_ASPECT_COLOR_BIT,
                                      VK_IMAGE_VIEW_TYPE_2D);

    VK_ERROR(vkCreateImageView(_device, &draw_img_view_create_info, nullptr,
                               &_draw_image.view),
             "Create Draw Image View");

    _main_deletion_queue.pushFunction([&]() {
      vkDestroyImageView(_device, _draw_image.view, nullptr);

      vmaDestroyImage(_allocator, _draw_image.image, _draw_image.allocation);
    });
  }
}

void Renderer::initCommands() {

  VkCommandPoolCreateInfo graphics_pool_create_info = {};
  graphics_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  graphics_pool_create_info.queueFamilyIndex = _graphics_queue_family;

  for (size_t i = 0; i < FRAME_OVERLAP; i++) {
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

    for (size_t i = 0; i < FRAME_OVERLAP; i++) {
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

    for (size_t i = 0; i < FRAME_OVERLAP; i++) {
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

    for (size_t i = 0; i < FRAME_OVERLAP; i++) {
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

    for (size_t i = 0; i < FRAME_OVERLAP; i++) {
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

void Renderer::initSyncStructures() {

  VkFenceCreateInfo fence_create_info = {};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphore_create_info = {};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (size_t i = 0; i < FRAME_OVERLAP; i++) {

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

void Renderer::draw(std::vector<RenderObject> &render_objects) {

  glfwPollEvents();

  {
    if (_resized_requested) {
      resizeSwapchain();
      return;
    }
  }

  _resource_manager->runDeletionQueue();

  VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame()._render_fence, true,
                           UINT64_MAX),
           "Wait for Fence");

  getCurrentFrame()._deletion_queue.flush();

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

    for (const auto &w : _resource_manager->getWrites()) {

      if (std::holds_alternative<Buffer>(w.target)) {

        const Buffer &target_buffer = std::get<Buffer>(w.target);

        VkBufferCopy region = {};
        region.srcOffset = 0;
        region.dstOffset = w.target_offset[0];
        region.size = w.source_buffer.size;

        vk_utils::transistionBuffer(transfer_command_buffer, VK_ACCESS_NONE,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    target_buffer.buffer);

        vkCmdCopyBuffer(transfer_command_buffer, w.source_buffer.buffer,
                        target_buffer.buffer, 1, &region);

        vk_utils::transistionBuffer(
            transfer_command_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
            w.buffer_write_data.new_access, target_buffer.buffer,
            _dedicated_transfer ? _transfer_queue_family : UINT32_MAX,
            _dedicated_transfer ? _graphics_queue_family : UINT32_MAX);

      } else {

        const Image &img = std::get<Image>(w.target);

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.imageExtent = img.extent;
        region.imageOffset = {static_cast<int32_t>(w.target_offset[0]),
                              static_cast<int32_t>(w.target_offset[1]),
                              static_cast<int32_t>(w.target_offset[2])};
        region.imageSubresource.aspectMask = img.aspect_mask;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.layerCount = 1;

        vk_utils::transistionImage(
            transfer_command_buffer, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, img.image);

        vkCmdCopyBufferToImage(transfer_command_buffer, w.source_buffer.buffer,
                               img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &region);

        vk_utils::transistionImage(
            transfer_command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            w.image_write_data.new_layout, img.image,
            _dedicated_transfer ? _transfer_queue_family : UINT32_MAX,
            _dedicated_transfer ? _graphics_queue_family : UINT32_MAX);
      }
    }

    _resource_manager->_deletion_queue.pushFunction(
        [](ResourceManager *manager) {
          for (const auto &w : manager->getWrites()) {

            vmaDestroyBuffer(manager->_allocator, w.source_buffer.buffer,
                             w.source_buffer.allocation);
          }

          manager->clearWrites();
        });

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

#pragma endregion

  {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(graphics_command_buffer, &begin_info),
             "Start Command Buffer");
  }

  vk_utils::transistionImage(graphics_command_buffer, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL, _draw_image.image);

  for (size_t i = 0; i < render_objects.size(); i++) {

    const auto &m = render_objects[i];

    // vkCmdBindPipeline(graphics_command_buffer,
    // VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_manager.)

    VkDeviceSize vertex_offset = m.vertex_offset;

    const Buffer &vertex_index_buffer =
        _resource_manager->getBuffer(m.vertex_buffer_id);

    vkCmdBindVertexBuffers(graphics_command_buffer, 0, 1,
                           &vertex_index_buffer.buffer, &vertex_offset);

    vkCmdBindIndexBuffer(graphics_command_buffer, vertex_index_buffer.buffer,
                         m.index_offset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(graphics_command_buffer, m.index_count, m.instance_count,
                     0, 0, 0);
  }

  drawBackground(graphics_command_buffer);

  vkCmdBindPipeline(graphics_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    _gradient_pipeline);
  vkCmdBindDescriptorSets(
      graphics_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      _gradient_pipeline_layout, 0, 1, &_draw_image_descriptors, 0, nullptr);

  vkCmdDispatch(graphics_command_buffer,
                std::ceil(_draw_image.extent.width / 16.0),
                std::ceil(_draw_image.extent.height / 16.0), 1);

  vk_utils::transistionImage(graphics_command_buffer, VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             _draw_image.image);

  vk_utils::transistionImage(graphics_command_buffer, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             _swapchain_images[swapchain_image_index]);

  vk_utils::copyImageToImage(
      graphics_command_buffer, _draw_image.image,
      _swapchain_images[swapchain_image_index],
      {_draw_image.extent.width, _draw_image.extent.height}, _swapchain_extent);

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
        vk_utils::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
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

  _frame_number++;
}

void Renderer::drawBackground(VkCommandBuffer cmd) {
  VkClearColorValue clear_value = {};

  clear_value = {{0.0f, 1.0f, 1.0f, 1.0f}};

  VkImageSubresourceRange range =
      vk_utils::getImageSubResourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

  vkCmdClearColorImage(cmd, _draw_image.image, VK_IMAGE_LAYOUT_GENERAL,
                       &clear_value, 1, &range);
}

void Renderer::initPipelines() { initBackgroundPipelines(); }

void Renderer::initBackgroundPipelines() {
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &_draw_image_descriptor_layout;
  computeLayout.setLayoutCount = 1;

  VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr,
                                  &_gradient_pipeline_layout),
           "Create Gtadient Layout");

  auto res =
      _pipeline_manager->createShaderModule("compute.slang", "computeMain");

  if (!res.has_value()) {
    std::cout << "COuld not get Gradient";
    std::abort();
  }

  VkShaderModule g_shader_module = res.value();

  VkPipelineShaderStageCreateInfo stageinfo{};
  stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageinfo.pNext = nullptr;
  stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageinfo.module = g_shader_module;
  stageinfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = _gradient_pipeline_layout;
  computePipelineCreateInfo.stage = stageinfo;

  VK_ERROR(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &_gradient_pipeline),
           "ff");

  vkDestroyShaderModule(_device, g_shader_module, nullptr);

  _main_deletion_queue.pushFunction([&]() {
    vkDestroyPipelineLayout(_device, _gradient_pipeline_layout, nullptr);
    vkDestroyPipeline(_device, _gradient_pipeline, nullptr);
  });
}

void Renderer::initImgui() {

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

void Renderer::resizeSwapchain() {
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

Buffer Renderer::createBuffer(size_t alloc_size, VkBufferUsageFlags usage,
                              VmaMemoryUsage memory_usage) {

  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

  buffer_info.size = alloc_size;
  buffer_info.usage = usage;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = memory_usage;
  alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  Buffer new_buffer;

  VK_CHECK(vmaCreateBuffer(_allocator, &buffer_info, &alloc_info,
                           &new_buffer.buffer, &new_buffer.allocation,
                           &new_buffer.allocation_info),
           "Create new Buffer");

  return new_buffer;
}

void Renderer::destroyBuffer(const Buffer &buffer) {
  vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}
