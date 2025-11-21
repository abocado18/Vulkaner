#include "renderer.h"
#include "GLFW/glfw3.h"
#include "platform/render/allocator/vk_mem_alloc.h"
#include "platform/render/pipeline.h"
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

  if (glfwInit() == GLFW_FALSE)
    return;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window_handle = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

  if (!_window_handle)
    return;

  if (!initVulkan())
    return;

  std::cout << "Create Resources\n";

  initSwapchain();

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
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
  };

  _global_descriptor_allocator.initPool(_device, 10, sizes);
  {
    DescriptorSetLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _draw_image_descriptor_layout =
        builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  _draw_image_descriptors = _global_descriptor_allocator.allocate(
      _device, _draw_image_descriptor_layout);

  VkDescriptorImageInfo image_info = {};
  image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  image_info.imageView = _draw_image.view;

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

  write.dstBinding = 0;
  write.dstSet = _draw_image_descriptors;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  write.pImageInfo = &image_info;

  vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

  _main_deletion_queue.pushFunction([&]() {
    _global_descriptor_allocator.destroyPool(_device);

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

  {
    VkExtent3D draw_image_extent = {width, height, 1};

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
                                      VK_IMAGE_ASPECT_COLOR_BIT);

    VK_ERROR(vkCreateImageView(_device, &draw_img_view_create_info, nullptr,
                               &_draw_image.view),
             "Create Draw Image View");

    _main_deletion_queue.pushFunction([&]() {
      vkDestroyImageView(_device, _draw_image.view, nullptr);

      vmaDestroyImage(_allocator, _draw_image.image, _draw_image.allocation);
    });
  }
}

void Renderer::destroySwapchain() {

  for (VkImageView &v : _swapchain_images_views) {
    vkDestroyImageView(_device, v, nullptr);
  }

  vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void Renderer::initCommands() {

  VkCommandPoolCreateInfo pool_create_info = {};
  pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_create_info.queueFamilyIndex = _graphics_queue_family;

  for (size_t i = 0; i < FRAME_OVERLAP; i++) {
    VK_ERROR(vkCreateCommandPool(_device, &pool_create_info, nullptr,
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
    for (size_t i = 0; i < FRAME_OVERLAP; i++) {
      VK_ERROR(vkCreateCommandPool(_device, &pool_create_info, nullptr,
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
      _frames[i]._compute_command_buffer = _frames[i]._graphics_command_buffer;
    }
  }

  if (_dedicated_transfer) {
    for (size_t i = 0; i < FRAME_OVERLAP; i++) {
      VK_ERROR(vkCreateCommandPool(_device, &pool_create_info, nullptr,
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
      _frames[i]._transfer_command_buffer = _frames[i]._graphics_command_buffer;
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
  }
}

void Renderer::draw() {

  glfwPollEvents();

  {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
  }

  VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame()._render_fence, true,
                           UINT64_MAX),
           "Wait for Fence");

  getCurrentFrame()._deletion_queue.flush();

  VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame()._render_fence),
           "Reset Fence");

  uint32_t swapchain_image_index;
  VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX,
                                 getCurrentFrame()._swapchain_semaphore,
                                 nullptr, &swapchain_image_index),
           "Swapchain Image");

  VkCommandBuffer graphics_command_buffer =
      getCurrentFrame()._graphics_command_buffer;
  VkCommandBuffer transfer_command_buffer =
      getCurrentFrame()._transfer_command_buffer;
  VkCommandBuffer compute_command_buffer =
      getCurrentFrame()._compute_command_buffer;

  VK_CHECK(vkResetCommandBuffer(graphics_command_buffer, 0),
           "Graphics Command Buffer");
  VK_CHECK(vkResetCommandBuffer(transfer_command_buffer, 0),
           "Transfer Command Buffer");
  VK_CHECK(vkResetCommandBuffer(compute_command_buffer, 0),
           "Compute Command Buffer");

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(graphics_command_buffer, &begin_info),
           "Start Command Buffer");

  vk_utils::transistionImage(graphics_command_buffer, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL, _draw_image.image);

  drawBackground(graphics_command_buffer);

  vkCmdBindPipeline(graphics_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    _gradient_pipeline);
  vkCmdBindDescriptorSets(
      graphics_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      _gradient_pipeline_layout, 0, 1, &_draw_image_descriptors, 0, nullptr);

  vkCmdDispatch(graphics_command_buffer,
                std::ceil(_draw_image.extent.width / 16.0),
                std::ceil(_draw_image.extent.height / 16.0), 1);

  {
    // ImGUI

    VkRenderingAttachmentInfo imm_attachment_info = vk_utils::attachmentInfo(
        _draw_image.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo imm_rendering_info = vk_utils::renderingInfo(
        &imm_attachment_info, 1, _swapchain_extent, {0, 0}, nullptr, nullptr);

    vkCmdBeginRendering(graphics_command_buffer, &imm_rendering_info);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                    graphics_command_buffer);

    vkCmdEndRendering(graphics_command_buffer);
  }

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

  VkCommandBufferSubmitInfo command_submit_info =
      vk_utils::commandBufferSubmitInfo(graphics_command_buffer);

  VkSemaphoreSubmitInfo wait_info = vk_utils::semaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      getCurrentFrame()._swapchain_semaphore);

  VkSemaphoreSubmitInfo signal_info = vk_utils::semaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
      getCurrentFrame()._render_semaphores[swapchain_image_index]);

  VkSubmitInfo2 submit_info =
      vk_utils::submitInfo(&command_submit_info, &signal_info, &wait_info);

  VK_CHECK(vkQueueSubmit2(_graphics_queue, 1, &submit_info,
                          getCurrentFrame()._render_fence),
           "Submit Graphics Commands");

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &_swapchain;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores =
      &getCurrentFrame()._render_semaphores[swapchain_image_index];

  present_info.pImageIndices = &swapchain_image_index;

  VK_CHECK(vkQueuePresentKHR(_graphics_queue, &present_info),
           "Present Swapchain");

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