#include "resource_handler.h"
#include "vulkan_macros.h"
#include <vulkan/vulkan_core.h>

resource_handler::ResourceHandler::ResourceHandler(VkPhysicalDevice &ph_device,
                                                   VkDevice &device,
                                                   VmaAllocator &allocator)
    : device(device), allocator(allocator) {

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

uint64_t resource_handler::ResourceHandler::loadImage(
    VkCommandBuffer command_buffer, const std::string &image_path,
    ImageLayouts desired_image_layout) {}