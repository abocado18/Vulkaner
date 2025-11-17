#include "renderer.h"
#include "GLFW/glfw3.h"
#include "platform/render/allocator/vk_mem_alloc.h"
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
#include <map>
#include <optional>
#include <ostream>
#include <sys/types.h>
#include <vector>

constexpr bool useValidationLayers = true;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *userData) {

  std::cerr << "[VULKAN DEBUG] " << callbackData->pMessage << "\n"
            << std::endl;
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

    vkDestroyDebugUtilsMessengerEXT(_instance, _debug_messenger, nullptr);

    vkDestroyInstance(_instance, nullptr);

    glfwDestroyWindow(_window_handle);

    glfwTerminate();
  } else {
    std::cout << "Renderer was never initialized\n";
  }
}

bool Renderer::initVulkan() {

  volkInitialize();

  {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    uint extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                           available_extensions.data());

    std::vector<const char *> extensions(
        glfw_extensions, glfw_extensions + glfw_extension_count);
    if constexpr (useValidationLayers) {
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    bool allExtensionsSupported = [&]() -> bool {
      for (uint32_t i = 0; i < extensions.size(); i++) {
        const char *req = extensions[i];
        bool found = false;

        for (const auto &ext : available_extensions) {
          if (strcmp(req, ext.extensionName) == 0) {
            found = true;
            break;
          }
        }

        if (!found) {
          return false;
        }
      }

      return true;
    }();

    if (!allExtensionsSupported) {
      std::cerr << "Extensions not supported\n";
      return false;
    };

    std::vector<const char *> requested_layers;

    if constexpr (useValidationLayers) {

      requested_layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    uint layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    bool allLayersAvailable = [&requested_layers, &available_layers]() -> bool {
      for (const char *layerName : requested_layers) {
        bool layerFound = false;

        for (const auto &layerProperties : available_layers) {
          if (strcmp(layerName, layerProperties.layerName) == 0) {
            layerFound = true;
            break;
          }
        }

        if (!layerFound) {
          return false;
        }
      }

      return true;
    }();

    if (allLayersAvailable == false) {

      std::cerr << "Layers not available\n";

      return false;
    }

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = requested_layers.size();
    create_info.ppEnabledLayerNames = requested_layers.data();

    VK_ERROR(vkCreateInstance(&create_info, nullptr, &_instance),
             "Instance not created");

    volkLoadInstance(_instance);

    if constexpr (useValidationLayers) {
      VkDebugUtilsMessengerCreateInfoEXT debug_info{};
      debug_info.sType =
          VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debug_info.messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      debug_info.pfnUserCallback = debugCallback;

      auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          _instance, "vkCreateDebugUtilsMessengerEXT");
      if (!func || func(_instance, &debug_info, nullptr, &_debug_messenger) !=
                       VK_SUCCESS) {
        std::cerr << "Failed to create debug messenger!" << std::endl;
        return false;
      }
    }
  }

  {

    VK_ERROR(
        glfwCreateWindowSurface(_instance, _window_handle, nullptr, &_surface),
        "Could not create Surface\n");

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> available_devices(device_count);
    vkEnumeratePhysicalDevices(_instance, &device_count,
                               available_devices.data());

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {};
    dynamic_rendering_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2_features = {};
    synchronization2_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    synchronization2_features.synchronization2 = VK_TRUE;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR address_features = {};
    address_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
    address_features.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexing_features = {};
    indexing_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    indexing_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    indexing_features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;

    VkPhysicalDeviceFeatures2 features = {};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    features.pNext = &indexing_features;
    indexing_features.pNext = &address_features;
    address_features.pNext = &synchronization2_features;
    synchronization2_features.pNext = &dynamic_rendering_features;
    dynamic_rendering_features.pNext = nullptr;

    std::vector<const char *> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
    };

    auto getSuitableDevice = [&](std::vector<VkPhysicalDevice> &devices)
        -> std::optional<VkPhysicalDevice> {
      std::multimap<int, VkPhysicalDevice> candidates;

      for (const auto &device : devices) {

        int32_t score = 0;

        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
          score += 1000;

        score += properties.limits.maxImageDimension2D;

        // Check for Extension support
        // Extensions

        vkGetPhysicalDeviceFeatures2(device, &features);

        if (!dynamic_rendering_features.dynamicRendering)
          continue;

        if (!synchronization2_features.synchronization2)
          continue;

        if (!address_features.bufferDeviceAddress)
          continue;

        if (!indexing_features.descriptorBindingPartiallyBound)
          continue;
        if (!indexing_features.descriptorBindingVariableDescriptorCount)
          continue;
        if (!indexing_features.descriptorBindingSampledImageUpdateAfterBind)
          continue;
        if (!indexing_features.descriptorBindingStorageBufferUpdateAfterBind)
          continue;

        uint ext_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count,
                                             nullptr);
        std::vector<VkExtensionProperties> available_device_extensions(
            ext_count);
        vkEnumerateDeviceExtensionProperties(
            device, nullptr, &ext_count, available_device_extensions.data());

        bool extensions_supported = true;

        for (auto &req : device_extensions) {
          bool found = false;
          for (auto &avail : available_device_extensions) {
            if (strcmp(req, avail.extensionName) == 0) {
              found = true;
              break;
            }
          }
          if (!found) {
            extensions_supported = false;
            break;
          }
        }

        for (auto &e : available_device_extensions) {
        }

        if (!extensions_supported)
          continue;

        candidates.insert(std::make_pair(score, device));
      }

      VkPhysicalDeviceProperties properties = {};
      vkGetPhysicalDeviceProperties(candidates.rbegin()->second, &properties);
      if (!candidates.empty()) {
        std::cout << "Suitable Device is: " << properties.deviceName << "\n";
        return candidates.rbegin()->second;
      } else {

        std::cerr << "failed to find a suitable GPU!\n";
        return std::nullopt;
      }
    };

    auto suitable_device = getSuitableDevice(available_devices);

    if (!suitable_device.has_value()) {
      std::cerr << "No suitable GPU found \n”";
      return false;
    }

    _chosen_gpu = suitable_device.value();

    uint queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_chosen_gpu, &queue_family_count,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(
        queue_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(_chosen_gpu, &queue_family_count,
                                             queue_family_properties.data());

    bool graphics_found = false;
    bool compute_found = false;
    bool transfer_found = false;

    for (size_t i = 0; i < queue_family_properties.size(); i++) {
      const auto &queue_family = queue_family_properties[i];

      if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphics_found = true;
        _graphics_queue_family = i;
      }

      if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
        transfer_found = true;
        _transfer_queue_family = i;
      }

      if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        _compute_queue_family = i;
        compute_found = true;
      }

      if (compute_found & graphics_found & transfer_found)
        break;
    }

    // Dedicated Queues

    _dedicated_transfer = false;
    _dedicated_compute = false;

    for (size_t i = 0; i < queue_family_properties.size(); i++) {
      const auto &queue_family = queue_family_properties[i];

      if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
        if (!(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)) {

          _transfer_queue_family = i;
          _dedicated_transfer = true;
          break;
        }
      }
    }

    for (size_t i = 0; i < queue_family_properties.size(); i++) {
      const auto &queue_family = queue_family_properties[i];

      if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        if (!(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {

          _compute_queue_family = i;
          _dedicated_compute = true;
          break;
        }
      }
    }

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos = {};

    constexpr float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo graphics_queue_create_info = {};
    graphics_queue_create_info.sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_create_info.queueCount = 1;
    graphics_queue_create_info.queueFamilyIndex = _graphics_queue_family;
    graphics_queue_create_info.pQueuePriorities = &queue_priority;

    queue_create_infos.push_back(graphics_queue_create_info);

    if (_dedicated_compute) {
      VkDeviceQueueCreateInfo compute_queue_create_info = {};
      compute_queue_create_info.sType =
          VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      compute_queue_create_info.queueCount = 1;
      compute_queue_create_info.queueFamilyIndex = _compute_queue_family;
      compute_queue_create_info.pQueuePriorities = &queue_priority;

      queue_create_infos.push_back(compute_queue_create_info);
    }

    if (_dedicated_transfer)

    {
      VkDeviceQueueCreateInfo transfer_queue_create_info = {};
      transfer_queue_create_info.sType =
          VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      transfer_queue_create_info.queueCount = 1;
      transfer_queue_create_info.queueFamilyIndex = _transfer_queue_family;
      transfer_queue_create_info.pQueuePriorities = &queue_priority;

      queue_create_infos.push_back(transfer_queue_create_info);
    }

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = queue_create_infos.size();
    device_create_info.pQueueCreateInfos = queue_create_infos.data();

    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    device_create_info.pNext = &features;

    VK_ERROR(
        vkCreateDevice(_chosen_gpu, &device_create_info, nullptr, &_device),
        "Could not create Device");

    vkGetDeviceQueue(_device, _graphics_queue_family, 0, &_graphics_queue);
    vkGetDeviceQueue(_device, _transfer_queue_family, 0, &_transfer_queue);
    vkGetDeviceQueue(_device, _compute_queue_family, 0, &_compute_queue);

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

  return true;
}

void Renderer::initSwapchain() {
  int width, height;
  glfwGetFramebufferSize(_window_handle, &width, &height);

  createSwapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void Renderer::createSwapchain(uint32_t width, uint32_t height,
                               VkSwapchainKHR old_swapchain) {

  uint surface_format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(_chosen_gpu, _surface,
                                       &surface_format_count, nullptr);

  std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);

  vkGetPhysicalDeviceSurfaceFormatsKHR(
      _chosen_gpu, _surface, &surface_format_count, surface_formats.data());

  if (surface_format_count == 0) {
    std::cerr << "No surface formats available!" << std::endl;
    return;
  }

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
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_chosen_gpu, _surface,
                                            &surface_capabilities);

  uint32_t image_count = surface_capabilities.minImageCount + 1;
  if (surface_capabilities.maxImageCount > 0 &&
      image_count > surface_capabilities.maxImageCount)
    image_count = surface_capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.clipped = VK_TRUE;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.minImageCount = image_count;
  create_info.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
  create_info.queueFamilyIndexCount = 1;
  create_info.pQueueFamilyIndices = &_graphics_queue_family;
  create_info.imageArrayLayers = 1;

  if (surface_capabilities.currentExtent.width == UINT32_MAX) {
    create_info.imageExtent = {width, height};
  } else {
    create_info.imageExtent = surface_capabilities.currentExtent;
  }

  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  create_info.preTransform = surface_capabilities.currentTransform;
  ;
  create_info.surface = _surface;
  create_info.oldSwapchain = old_swapchain;

  VK_ERROR(vkCreateSwapchainKHR(_device, &create_info, nullptr, &_swapchain),
           "Could not create swapchain");

  _swapchain_image_format = surface_format.format;

  _swapchain_extent = create_info.imageExtent;

  _swapchain_images.clear();

  uint swapchain_image_count = 0;
  vkGetSwapchainImagesKHR(_device, _swapchain, &swapchain_image_count, nullptr);
  _swapchain_images.resize(swapchain_image_count);
  vkGetSwapchainImagesKHR(_device, _swapchain, &swapchain_image_count,
                          _swapchain_images.data());

  if (old_swapchain != VK_NULL_HANDLE) {

    for (auto &v : _swapchain_images_views) {
      vkDestroyImageView(_device, v, nullptr);
    }

    _swapchain_images_views.clear();

    vkDestroySwapchainKHR(_device, old_swapchain, nullptr);
  }

  _swapchain_images_views.resize(_swapchain_images.size());

  for (size_t i = 0; i < _swapchain_images_views.size(); i++) {
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = _swapchain_images[i];
    view_create_info.format = _swapchain_image_format;

    view_create_info.components = {
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A};

    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.layerCount = 1;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

    VK_ERROR(vkCreateImageView(_device, &view_create_info, nullptr,
                               &_swapchain_images_views[i]),
             "Failed to create swapchain image view");
  }

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

  VkCommandBufferSubmitInfoKHR command_submit_info =
      vk_utils::commandBufferSubmitInfo(graphics_command_buffer);

  VkSemaphoreSubmitInfoKHR wait_info = vk_utils::semaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      getCurrentFrame()._swapchain_semaphore);

  VkSemaphoreSubmitInfoKHR signal_info = vk_utils::semaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
      getCurrentFrame()._render_semaphores[swapchain_image_index]);

  VkSubmitInfo2KHR submit_info =
      vk_utils::submitInfo(&command_submit_info, &signal_info, &wait_info);

  VK_CHECK(vkQueueSubmit2KHR(_graphics_queue, 1, &submit_info,
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