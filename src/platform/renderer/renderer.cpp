#include "renderer.h"

#include "allocator/vk_mem_alloc.h"
#include "pipeline.h"
#include "resource_handler.h"

#include "vulkan_macros.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

render::RenderContext::RenderContext(uint32_t width, uint32_t height,
                                     const std::string &shader_path) {

  if (glfwInit() == GLFW_FALSE) {
    std::cerr << "Could not init GLFW\n";
    abort();
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window = glfwCreateWindow(width, height, "Renderer", nullptr, nullptr);

  if (window == nullptr) {
    std::cerr << "Could not create window\n";
  }

  {
    auto windowResize = [](GLFWwindow *window, int width, int height) {
      if (width == 0 || height == 0) {
        return;
      }

      RenderContext *ctx = (RenderContext *)glfwGetWindowUserPointer(window);

      vkDeviceWaitIdle(ctx->device);

      ctx->createSwapchain(true, ctx->swapchain);
    };

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, windowResize);
  }

  VK_ERROR(volkInitialize());

  uint32_t glfw_instance_extension_count = 0;
  const char **glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_instance_extension_count);

  std::vector<const char *> enabled_layers = {};
#ifdef VULKAN_USE_VALIDATION_LAYERS
  {

    uint32_t property_count;

    vkEnumerateInstanceLayerProperties(&property_count, nullptr);

    std::vector<VkLayerProperties> available_properties(property_count);

    vkEnumerateInstanceLayerProperties(&property_count,
                                       available_properties.data());

    for (const auto &property : available_properties) {
      std::string name = property.layerName;

      if (name == "VK_LAYER_KHRONOS_validation") {
        enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
        break;
      }
    }
  }
#endif

  {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Renderer";
    app_info.apiVersion = VK_API_VERSION_1_2;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Renderer";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = glfw_instance_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;
    create_info.enabledLayerCount = enabled_layers.size();
    create_info.ppEnabledLayerNames = enabled_layers.data();

    VK_ERROR(vkCreateInstance(&create_info, nullptr, &instance));
  }

  volkLoadInstance(instance);

  if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
      VK_SUCCESS) {
    std::cerr << "Could not create surface\n";
    abort();
  }

  {

    uint32_t count;
    vkEnumeratePhysicalDevices(this->instance, &count, nullptr);

    std::vector<VkPhysicalDevice> physical_devices(count);

    vkEnumeratePhysicalDevices(this->instance, &count, physical_devices.data());

    std::multimap<uint32_t, VkPhysicalDevice> candidates = {};

    for (const VkPhysicalDevice &d : physical_devices) {
      VkPhysicalDeviceProperties properties;
      uint32_t score = 0;

      vkGetPhysicalDeviceProperties(d, &properties);

      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
      }

      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 500;
      }

      score += properties.limits.maxImageDimension2D;

      auto areExtensionsSupported =
          [&](const std::vector<std::string> &required_extensions) -> bool {
        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(d, nullptr, &extension_count,
                                             nullptr);

        std::vector<VkExtensionProperties> extension_properties(
            extension_count);

        vkEnumerateDeviceExtensionProperties(d, nullptr, &extension_count,
                                             extension_properties.data());

        for (auto &ex : required_extensions) {
          bool found_extension = false;

          for (VkExtensionProperties &property : extension_properties) {
            std::string extension_name = property.extensionName;
            if (extension_name == ex) {
              found_extension = true;
              break;
            }
          }

          if (!found_extension)
            return false;
        }

        return true;
      };

      std::vector<std::string> required_extensions = {
          VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
          VK_KHR_MAINTENANCE2_EXTENSION_NAME,
          VK_KHR_MULTIVIEW_EXTENSION_NAME,
          VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
          VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
          VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
          VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
          VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
          VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,

      };

      if (areExtensionsSupported(required_extensions)) {
        candidates.insert(std::make_pair(score, d));
      }
    }

    if (candidates.rbegin()->first > 0) {
      physical_device = candidates.rbegin()->second;
    } else {
      std::cerr << "No physical device found\n";
      abort();
    }
  }

  {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(this->physical_device, &count,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(this->physical_device, &count,
                                             queue_family_properties.data());

    bool found_graphics = false;
    bool found_transfer = false;

    uint32_t index = 0;

    for (const auto &queue_family_property : queue_family_properties) {
      if (found_graphics == false) {
        if (queue_family_property.queueFlags &
            VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT) {
          found_graphics = true;
          this->graphics_queue_index = index;
        }

        if (queue_family_property.queueFlags &
            VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT) {
          found_transfer = true;
          this->transfer_queue_index = index;
        }
      }

      index++;
    }

    if (found_graphics == false) {
      std::cout << "Could not find graphics queue\n";
      abort();
    }

    if (found_transfer == false) {
      std::cout << "Could not find transfer queue, use graphics queue as "
                   "transfer queue\n";
      transfer_queue_index = graphics_queue_index;
    }
  }

  {

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos = {};

    const float priority = 1.0f;

    VkDeviceQueueCreateInfo graphics_queue_create_info = {};
    graphics_queue_create_info.sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_create_info.pQueuePriorities = &priority;
    graphics_queue_create_info.queueCount = 1;
    graphics_queue_create_info.queueFamilyIndex = this->graphics_queue_index;

    queue_create_infos.push_back(graphics_queue_create_info);

    if (transfer_queue_index != graphics_queue_index) {
      VkDeviceQueueCreateInfo transfer_queue_create_info = {};
      transfer_queue_create_info.sType =
          VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      transfer_queue_create_info.pQueuePriorities = &priority;
      transfer_queue_create_info.queueCount = 1;
      transfer_queue_create_info.queueFamilyIndex = transfer_queue_index;

      queue_create_infos.push_back(transfer_queue_create_info);
    }

    std::vector<const char *> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        VK_KHR_MULTIVIEW_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,

    };

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexing_features = {};
    indexing_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;

    indexing_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    indexing_features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    indexing_features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    indexing_features.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_features = {};
    dynamic_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync_features = {};
    sync_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    sync_features.synchronization2 = VK_TRUE;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR address_features = {};
    address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
    address_features.bufferDeviceAddress = VK_TRUE;
    

    // Chain together
    dynamic_features.pNext = &sync_features;
    sync_features.pNext = &indexing_features;
    indexing_features.pNext = &address_features;
    address_features.pNext = nullptr;

    VkPhysicalDeviceFeatures ph_features = {};
    ph_features.robustBufferAccess = VK_TRUE;

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.enabledExtensionCount = device_extensions.size();
    create_info.ppEnabledExtensionNames = device_extensions.data();
    create_info.enabledLayerCount = 0;
    create_info.queueCreateInfoCount = queue_create_infos.size();
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pNext = &dynamic_features;
    create_info.pEnabledFeatures = &ph_features;

    VK_ERROR(vkCreateDevice(this->physical_device, &create_info, nullptr,
                            &this->device));

    std::cout << "Created Device\n";
  }

  volkLoadDevice(device);

  {
    const float priority = 0.0f;

    vkGetDeviceQueue(this->device, this->graphics_queue_index, 0,
                     &this->graphics_queue);
  }

  {
    const float priority = 0.0f;

    vkGetDeviceQueue(this->device, this->transfer_queue_index, 0,
                     &this->transfer_queue);
  }

  {
    VmaVulkanFunctions vma_vulkan_func = {};

    VmaAllocatorCreateInfo create_info = {};
    create_info.pVulkanFunctions = &vma_vulkan_func;
    create_info.device = this->device;
    create_info.instance = this->instance;
    create_info.physicalDevice = this->physical_device;
    create_info.preferredLargeHeapBlockSize = 0;
    create_info.vulkanApiVersion = VK_API_VERSION_1_2;

    vmaImportVulkanFunctionsFromVolk(&create_info, &vma_vulkan_func);

    VK_ERROR(vmaCreateAllocator(&create_info, &this->vma_allocator));
  }

  {
    resource_handler = new resource_handler::ResourceHandler(
        physical_device, device, vma_allocator, transfer_queue_index);
  }

  {
    createSwapchain();
  }

  {
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = graphics_queue_index;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_ERROR(vkCreateCommandPool(device, &create_info, nullptr,
                                 &graphics_command_pool));

    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = graphics_command_pool;
    allocate_info.commandBufferCount = 1;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_ERROR(vkAllocateCommandBuffers(device, &allocate_info,
                                      &graphics_command_buffer));
  }

  {
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = transfer_queue_index;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_ERROR(vkCreateCommandPool(device, &create_info, nullptr,
                                 &transfer_command_pool));

    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = transfer_command_pool;
    allocate_info.commandBufferCount = 1;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_ERROR(vkAllocateCommandBuffers(device, &allocate_info,
                                      &transfer_command_buffer));
  }

  {
    VkSemaphoreCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_ERROR(vkCreateSemaphore(device, &create_info, nullptr,
                               &aquire_image_semaphore));

    VK_ERROR(vkCreateSemaphore(device, &create_info, nullptr,
                               &transfer_finished_semaphore));

    rendering_finished_semaphores.resize(swapchain.images.size());

    for (size_t i = 0; i < swapchain.images.size(); i++) {
      VK_ERROR(vkCreateSemaphore(device, &create_info, nullptr,
                                 &rendering_finished_semaphores[i]));
    }
  }

  {
    VkFenceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_ERROR(vkCreateFence(device, &create_info, nullptr, &fence));
  }

  {
    pipeline_manager =
        new pipeline::PipelineManager(device, shader_path, *resource_handler);
  }

  {
    pipeline::PipelineData pipeline_data = {};
    pipeline::PipelineData::getDefault(pipeline_data);
    pipeline_data.rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    pipeline_data.vertex_desc.attribute_descs.clear();
    pipeline_data.vertex_desc.binding_descs.clear();

    pipeline_manager->createRenderPipeline(pipeline_data, "triangle");

    resource_handler->loadImage(SHADER_PATH "/zelda.jpg",
                                VK_FORMAT_R8G8B8A8_UNORM,
                                VK_IMAGE_USAGE_SAMPLED_BIT);
  }
}

render::RenderContext::~RenderContext() {

  delete pipeline_manager;
  delete resource_handler;

  vkDeviceWaitIdle(device);

  vmaDestroyAllocator(vma_allocator);

  vkDestroyFence(device, fence, nullptr);

  vkDestroySemaphore(device, aquire_image_semaphore, nullptr);
  vkDestroySemaphore(device, transfer_finished_semaphore, nullptr);

  for (auto &semaphore : rendering_finished_semaphores) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }

  vkDestroyCommandPool(device, transfer_command_pool, nullptr);
  vkDestroyCommandPool(device, graphics_command_pool, nullptr);

  for (auto &v : swapchain.views) {
    vkDestroyImageView(device, v, nullptr);
  }

  vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);

  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);

  vkDestroyInstance(instance, nullptr);

  glfwDestroyWindow(window);

  glfwTerminate();
}

bool render::RenderContext::windowShouldClose() const {
  return glfwWindowShouldClose(window);
}

void render::RenderContext::update() {
  glfwPollEvents();
#ifndef PRODUCTION_BUILD

  pipeline_manager->reload();

#endif

  render();
}

void render::RenderContext::render() {

  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

  VkAcquireNextImageInfoKHR aquire_info = {};
  aquire_info.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
  aquire_info.swapchain = swapchain.swapchain;
  aquire_info.timeout = UINT64_MAX;
  aquire_info.semaphore = aquire_image_semaphore;
  aquire_info.deviceMask = 1;

  uint32_t swapchain_image_index;
  VkResult aquire_image_result =
      vkAcquireNextImage2KHR(device, &aquire_info, &swapchain_image_index);

  if (aquire_image_result == VK_ERROR_OUT_OF_DATE_KHR ||
      aquire_image_result == VK_SUBOPTIMAL_KHR) {
    vkDeviceWaitIdle(device);

    int width;
    int height;

    glfwGetFramebufferSize(window, &width, &height);

    if (width == 0 || height == 0) {
      return;
    }

    createSwapchain(true, swapchain);
    return;
  }

  vkResetFences(device, 1, &fence);
  vkResetCommandPool(device, graphics_command_pool, 0);
  vkResetCommandPool(device, transfer_command_pool, 0);

  // Record Command Buffers

  {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    std::vector<resource_handler::StagingTransferData> &transfer_data =
        resource_handler->getStagingTransferData();

    VK_ERROR(vkBeginCommandBuffer(transfer_command_buffer, &begin_info));

    for (auto &t : transfer_data) {
      if (t.target->type == resource_handler::ResourceType::IMAGE) {

        VkBufferImageCopy2KHR region = {};
        region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR;
        region.imageSubresource.aspectMask =
            t.target->resource_data.image.range.aspectMask;
        region.imageSubresource.baseArrayLayer =
            t.target->resource_data.image.range.baseArrayLayer;
        region.imageSubresource.layerCount =
            t.target->resource_data.image.range.layerCount;
        region.imageSubresource.mipLevel =
            t.target->resource_data.image.range.baseMipLevel;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = t.target->resource_data.image.extent;

        region.bufferOffset = t.source_offset;

        VkCopyBufferToImageInfo2KHR image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR;
        image_info.dstImage = t.target->resource_data.image.image;
        image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        image_info.regionCount = 1;
        image_info.pRegions = &region;
        image_info.srcBuffer = resource_handler->getStagingBuffer();

        assert(vkCmdCopyBufferToImage2KHR != nullptr);

        assert(t.target != nullptr);
        assert(t.target->resource_data.image.image != VK_NULL_HANDLE);
        assert(t.target->resource_data.image.extent.width > 0);
        assert(t.target->resource_data.image.extent.height > 0);
        assert(t.target->resource_data.image.extent.depth > 0);
        assert(resource_handler->getStagingBuffer() != VK_NULL_HANDLE);

        {
          resource_handler::TransistionData transistion_data = {};
          transistion_data.data.image_data.image_layout =
              resource_handler::ImageLayouts::TRANSFER_DST_OPTIMAL;
          transistion_data.type = resource_handler::ResourceType::IMAGE;

          resource_handler->updateTransistion(transfer_command_buffer,
                                              transistion_data, t.resource_idx);
        }

        vkCmdCopyBufferToImage2KHR(transfer_command_buffer, &image_info);

        {
          resource_handler::TransistionData transistion_data = {};
          transistion_data.data.image_data.image_layout =
              resource_handler::ImageLayouts::SHADER_READ_ONLY_OPTIMAL;
          transistion_data.type = resource_handler::ResourceType::IMAGE;
          transistion_data.source_queue_family = transfer_queue_index;
          transistion_data.dst_queue_family = graphics_queue_index;

          resource_handler->updateTransistion(transfer_command_buffer,
                                              transistion_data, t.resource_idx);
        }

      } else {

        // To do: Implmemet buffer transfer
      }
    }

    this->resource_handler->clearStagingTransferData();

    vkEndCommandBuffer(transfer_command_buffer);

    VkSemaphoreSubmitInfoKHR signal_submit_info = {};
    signal_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    signal_submit_info.semaphore = transfer_finished_semaphore;
    signal_submit_info.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    signal_submit_info.deviceIndex = 0;
    signal_submit_info.value = 0;

    VkCommandBufferSubmitInfoKHR submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
    submit_info.commandBuffer = transfer_command_buffer;
    submit_info.deviceMask = 0;

    VkSubmitInfo2KHR submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    submit.waitSemaphoreInfoCount = 0;
    submit.pWaitSemaphoreInfos = nullptr;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal_submit_info;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &submit_info;

    VK_ERROR(vkQueueSubmit2KHR(transfer_queue, 1, &submit, 0));
  }

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_ERROR(vkBeginCommandBuffer(graphics_command_buffer, &begin_info));

  {

    {

      resource_handler::TransistionData transistion_data = {};
      transistion_data.data.image_data.image_layout =
          resource_handler::COLOR_ATTACHMENT_OPTIMAL;

      resource_handler->updateTransistion(
          graphics_command_buffer, transistion_data,
          swapchain.resource_image_indices[swapchain_image_index]);
    }

    VkRenderingAttachmentInfoKHR attachment_info = {};
    attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    attachment_info.imageView = swapchain.views[swapchain_image_index];
    attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_info.clearValue = {{0.0f, 0.0f, 1.0f, 1.0f}};

    VkRenderingInfoKHR rendering_info = {};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = swapchain.extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &attachment_info;

    vkCmdBeginRenderingKHR(graphics_command_buffer, &rendering_info);

    vkCmdBindPipeline(graphics_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline_manager->getPipelineByName("triangle").pipeline);

    VkViewport viewport = {};
    viewport.height = swapchain.extent.height;
    viewport.width = swapchain.extent.width;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.x = 0.f;
    viewport.y = 0.f;

    VkRect2D scissor = {};
    scissor.extent = swapchain.extent;
    scissor.offset = {0, 0};

    vkCmdBindDescriptorSets(
        graphics_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_manager->getPipelineByName("triangle").layout, 0, 1,
        &resource_handler->getSampledImagesDescriptor().descriptor, 0, nullptr);

    vkCmdSetViewport(graphics_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(graphics_command_buffer, 0, 1, &scissor);

    vkCmdDraw(graphics_command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderingKHR(graphics_command_buffer);
  }

  {

    resource_handler::TransistionData transistion_data = {};
    transistion_data.data.image_data.image_layout =
        resource_handler::PRESENT_SRC_KHR;

    resource_handler->updateTransistion(
        graphics_command_buffer, transistion_data,
        swapchain.resource_image_indices[swapchain_image_index]);
  }

  vkEndCommandBuffer(graphics_command_buffer);

  std::array<VkSemaphore, 2> wait_semaphores = {
      aquire_image_semaphore,
      transfer_finished_semaphore,
  };

  VkSemaphoreSubmitInfoKHR wait_submit_info_1 = {};
  wait_submit_info_1.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
  wait_submit_info_1.semaphore = aquire_image_semaphore;
  wait_submit_info_1.stageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
  wait_submit_info_1.deviceIndex = 0;
  wait_submit_info_1.value = 0;

  VkSemaphoreSubmitInfoKHR wait_submit_info_2 = {};
  wait_submit_info_2.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
  wait_submit_info_2.semaphore = transfer_finished_semaphore;
  wait_submit_info_2.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
  wait_submit_info_2.deviceIndex = 0;
  wait_submit_info_2.value = 0;

  std::array<VkPipelineStageFlags2KHR, 2> wait_stage_masks = {
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
  };

  std::array<VkSemaphoreSubmitInfoKHR, 2> wait_submits = {wait_submit_info_1,
                                                          wait_submit_info_2};

  VkSemaphoreSubmitInfoKHR signal_submit_info = {};
  signal_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
  signal_submit_info.semaphore =
      rendering_finished_semaphores[swapchain_image_index];
  signal_submit_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
  signal_submit_info.deviceIndex = 0;
  signal_submit_info.value = 0;

  VkCommandBufferSubmitInfoKHR submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
  submit_info.commandBuffer = graphics_command_buffer;
  submit_info.deviceMask = 0;

  VkSubmitInfo2KHR submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
  submit.waitSemaphoreInfoCount = wait_submits.size();
  submit.pWaitSemaphoreInfos = wait_submits.data();
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal_submit_info;
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &submit_info;

  VK_ERROR(vkQueueSubmit2KHR(graphics_queue, 1, &submit, fence));

  // Present

  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pImageIndices = &swapchain_image_index;
  present.swapchainCount = 1;
  present.pSwapchains = &swapchain.swapchain;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores =
      &rendering_finished_semaphores[swapchain_image_index];

  VkResult present_result = vkQueuePresentKHR(graphics_queue, &present);

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
      present_result == VK_SUBOPTIMAL_KHR) {
    vkDeviceWaitIdle(device);

    int width;
    int height;

    glfwGetFramebufferSize(window, &width, &height);

    if (width == 0 || height == 0) {
      return;
    }

    createSwapchain(true, swapchain);
    return;
  }
}

void render::RenderContext::createSwapchain(bool has_old_swapchain,
                                            Swapchain old_swapchain) {
  uint32_t surface_format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(this->physical_device, this->surface,
                                       &surface_format_count, nullptr);

  std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);

  vkGetPhysicalDeviceSurfaceFormatsKHR(this->physical_device, this->surface,
                                       &surface_format_count,
                                       surface_formats.data());

  VkSurfaceFormatKHR surface_format;

  surface_format = surface_formats[0];

  for (const auto &f : surface_formats) {
    if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
        f.format == VK_FORMAT_B8G8R8A8_SRGB) {
      surface_format = f;
      break;
    }
  }

  VkSurfaceCapabilitiesKHR surface_capabilities = {};
  VK_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                                     &surface_capabilities));

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.clipped = VK_TRUE;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.minImageCount = 3;
  create_info.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
  create_info.queueFamilyIndexCount = 1;
  create_info.pQueueFamilyIndices = &graphics_queue_index;
  create_info.imageArrayLayers = 1;
  create_info.imageExtent = {surface_capabilities.currentExtent.width,
                             surface_capabilities.currentExtent.height};
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  create_info.surface = this->surface;
  create_info.oldSwapchain = has_old_swapchain ? old_swapchain.swapchain : 0;

  VK_ERROR(vkCreateSwapchainKHR(this->device, &create_info, nullptr,
                                &this->swapchain.swapchain));

  swapchain.extent = create_info.imageExtent;

  uint32_t swapchain_image_count = 0;

  vkGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchain_image_count,
                          nullptr);

  swapchain.images.resize(swapchain_image_count);
  swapchain.views.resize(swapchain_image_count);

  vkGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchain_image_count,
                          swapchain.images.data());

  for (size_t i = 0; i < swapchain_image_count; i++) {
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = this->swapchain.images[i];
    view_create_info.format = surface_format.format;
    view_create_info.components = {
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A};
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.layerCount = 1;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

    VK_ERROR(vkCreateImageView(this->device, &view_create_info, nullptr,
                               &this->swapchain.views[i]));
  }

  swapchain.resource_image_indices.resize(swapchain_image_count);

  for (size_t i = 0; i < swapchain_image_count; i++) {

    resource_handler::Resource resource = {};
    resource.type = resource_handler::ResourceType::IMAGE;
    resource.resource_data.image.image = swapchain.images[i];
    resource.resource_data.image.current_layout = resource_handler::UNDEFINED;
    resource.resource_data.image.range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    resource.resource_data.image.range.baseArrayLayer = 0;
    resource.resource_data.image.range.baseMipLevel = 0;
    resource.resource_data.image.range.layerCount = 1;
    resource.resource_data.image.range.levelCount = 1;

    swapchain.resource_image_indices[i] =
        resource_handler->insertResource(resource);
  }

  if (has_old_swapchain) {

    for (size_t i = 0; i < old_swapchain.views.size(); i++) {
      vkDestroyImageView(device, old_swapchain.views[i], nullptr);
    }

    vkDestroySwapchainKHR(device, old_swapchain.swapchain, nullptr);
  }
}
