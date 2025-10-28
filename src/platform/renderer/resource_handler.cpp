#include "resource_handler.h"
#include "stb_image/stb_image.h"
#include "vulkan_macros.h"
#include <cstdint>
#include <cstring>
#include <vulkan/vulkan_core.h>

resource_handler::ResourceHandler::ResourceHandler(
    VkPhysicalDevice &ph_device, VkDevice &device, VmaAllocator &allocator,
    uint32_t graphics_queue_index)
    : device(device), allocator(allocator),
      graphics_queue_index(graphics_queue_index) {

  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  binding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo set_layout_create_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  set_layout_create_info.flags =
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
  const VkDescriptorBindingFlagsEXT flags =
      VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
      VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT;

  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags{};
  binding_flags.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
  binding_flags.bindingCount = 1;
  binding_flags.pBindingFlags = &flags;
  set_layout_create_info.pNext = &binding_flags;

  set_layout_create_info.bindingCount = 1;
  set_layout_create_info.pBindings = &binding;

  VK_ERROR(vkCreateDescriptorSetLayout(device, &set_layout_create_info, nullptr,
                                       &sampled_images_descriptor.layout));

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(ph_device, &properties);

  sampled_images_limit = properties.limits.maxDescriptorSetSampledImages;

  VkDescriptorPoolSize pool_size = {};
  pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_size.descriptorCount = sampled_images_limit;

  VkDescriptorPoolCreateInfo pool_create_info = {};
  pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_create_info.maxSets = 1;
  pool_create_info.poolSizeCount = 1;
  pool_create_info.pPoolSizes = &pool_size;
  pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

  VK_ERROR(vkCreateDescriptorPool(device, &pool_create_info, nullptr,
                                  &sampled_images_descriptor.pool));

  VkDescriptorSetAllocateInfo allocate_info = {};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = sampled_images_descriptor.pool;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &sampled_images_descriptor.layout;

  VK_ERROR(vkAllocateDescriptorSets(device, &allocate_info,
                                    &sampled_images_descriptor.descriptor));

  {
    // Create Staging Buffer
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = 65536;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_create_info.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_ERROR(vmaCreateBuffer(allocator, &buffer_create_info, &alloc_create_info,
                             &staging_buffer.buffer, &staging_buffer.allocation,
                             &staging_buffer.allocation_info));

    staging_buffer.offset = 0;
  }
}

resource_handler::ResourceHandler::~ResourceHandler() {

  vkDeviceWaitIdle(device);

  vkDestroyDescriptorSetLayout(device, sampled_images_descriptor.layout,
                               nullptr);

  vkDestroyDescriptorPool(device, sampled_images_descriptor.pool, nullptr);

  vmaDestroyBuffer(allocator, staging_buffer.buffer, staging_buffer.allocation);
}

uint64_t resource_handler::ResourceHandler::insertResource(
    resource_handler::Resource &resource) {
  static uint64_t next_id = 0;
  uint64_t id = next_id++;

  this->resources[id] = resource;

  return id;
}

void resource_handler::ResourceHandler::updateTransistion(
    VkCommandBuffer command_buffer, TransistionData transistion_data,
    uint64_t resource_idx) {

  auto it = resources.find(resource_idx);

  if (it == resources.end())
    return;

  Resource &resource = it->second;

  if (resource.type == resource_handler::ResourceType::IMAGE) {
    // Replace later with resource transistion manager update

    VkImageMemoryBarrier2KHR memory_barrier = {};
    memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
    memory_barrier.oldLayout =
        getImageLayout(resource.resource_data.image.current_layout);
    memory_barrier.newLayout =
        getImageLayout(transistion_data.data.image_data.image_layout);
    memory_barrier.srcStageMask =
        getStageMask(resource.resource_data.image.current_layout);

    memory_barrier.srcAccessMask =
        getAccessMask(resource.resource_data.image.current_layout);
    memory_barrier.dstAccessMask =
        getAccessMask(transistion_data.data.image_data.image_layout);
    memory_barrier.dstStageMask =
        getStageMask(transistion_data.data.image_data.image_layout);
    memory_barrier.image = resource.resource_data.image.image;
    memory_barrier.subresourceRange = resource.resource_data.image.range;

    memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    VkDependencyInfoKHR dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &memory_barrier;

    vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);

    // Update Layout
    resource.resource_data.image.current_layout =
        transistion_data.data.image_data.image_layout;

  } else {

    // To do: Add Transisition Logic for Buffers
  }
}

uint64_t resource_handler::ResourceHandler::recordloadImageCommand(
    VkCommandBuffer command_buffer, const std::string &image_path,
    ImageLayouts desired_image_layout, VkImageUsageFlags image_usage,
    VkFormat image_format) {

  uint64_t resource_index = UINT64_MAX;

  int width, height, channels;

  stbi_uc *pixels =
      stbi_load(image_path.c_str(), &width, &height, &channels, 4);

  if (!pixels) {
    std::cerr << "No file found\n";
    return UINT64_MAX;
  }

  {

    Image new_image = {};
    new_image.current_layout = ImageLayouts::UNDEFINED;
    new_image.range.layerCount = 1;
    new_image.range.levelCount = 1;
    new_image.range.baseMipLevel = 0;
    new_image.range.baseArrayLayer = 0;
    new_image.range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.usage = image_usage;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 1;
    image_create_info.pQueueFamilyIndices = &graphics_queue_index;
    image_create_info.extent = {static_cast<uint32_t>(width),
                                static_cast<uint32_t>(height), 1};

    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.arrayLayers = 1;
    image_create_info.format = image_format;
    image_create_info.mipLevels = 1;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateImage(allocator, &image_create_info, &allocation_create_info,
                   &new_image.image, &new_image.allocation,
                   &new_image.allocation_info);

    Resource new_resource = {};
    new_resource.type = ResourceType::IMAGE;
    new_resource.resource_data.image = new_image;

    resource_index = insertResource(new_resource);
  }

  std::memcpy(
      reinterpret_cast<uint8_t *>(staging_buffer.allocation_info.pMappedData) +
          staging_buffer.offset,
      pixels, width * height * 4);

  TransistionData transistion_data = {};
  transistion_data.type = ResourceType::IMAGE;
  transistion_data.data.image_data.image_layout =
      ImageLayouts::TRANSFER_DST_OPTIMAL;

  updateTransistion(command_buffer, transistion_data, resource_index);

  auto &r = resources[resource_index];

  VkBufferImageCopy2 region = {};
  region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
  region.imageSubresource.aspectMask = r.resource_data.image.range.aspectMask;
  region.imageSubresource.baseArrayLayer =
      r.resource_data.image.range.baseArrayLayer;
  region.imageSubresource.layerCount = r.resource_data.image.range.layerCount;
  region.imageSubresource.mipLevel = r.resource_data.image.range.baseMipLevel;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height), 1};

  region.bufferOffset = staging_buffer.offset;

  VkCopyBufferToImageInfo2 copy_image_info = {};
  copy_image_info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
  copy_image_info.srcBuffer = staging_buffer.buffer;
  copy_image_info.regionCount = 1;
  copy_image_info.pRegions = &region;
  copy_image_info.dstImageLayout =
      getImageLayout(r.resource_data.image.current_layout);
  copy_image_info.dstImage = r.resource_data.image.image;

  vkCmdCopyBufferToImage2KHR(command_buffer, &copy_image_info);

  TransistionData second_transistion_data = {};
  second_transistion_data.type = ResourceType::IMAGE;
  second_transistion_data.data.image_data.image_layout = desired_image_layout;

  updateTransistion(command_buffer, second_transistion_data, resource_index);
  stbi_image_free(pixels);

  staging_buffer.offset += width * height * 4;

  return resource_index;
}