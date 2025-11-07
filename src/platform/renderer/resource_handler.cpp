#include "resource_handler.h"
#include "vulkan_macros.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

resource_handler::ResourceHandler::ResourceHandler(
    VkPhysicalDevice &ph_device, VkDevice &device, VmaAllocator &allocator,
    uint32_t transfer_queue_index)
    : device(device), allocator(allocator),
      transfer_queue_index(transfer_queue_index) {

  vkGetPhysicalDeviceProperties(ph_device, &device_properties);

  {

    storage_pointer_buffer_max = 100'000;
  }

  {
    sampled_images_limit =
        device_properties.limits.maxDescriptorSetSampledImages;

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = sampled_images_limit;
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

    VK_ERROR(vkCreateDescriptorSetLayout(device, &set_layout_create_info,
                                         nullptr,
                                         &sampled_images_descriptor.layout));

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

    uint32_t variableDescriptorCount = sampled_images_limit;

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variable_count_info{};
    variable_count_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
    variable_count_info.descriptorSetCount = 1;
    variable_count_info.pDescriptorCounts = &variableDescriptorCount;
    variable_count_info.pNext = nullptr;

    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = sampled_images_descriptor.pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &sampled_images_descriptor.layout;
    allocate_info.pNext = &variable_count_info;

    VK_ERROR(vkAllocateDescriptorSets(device, &allocate_info,
                                      &sampled_images_descriptor.descriptor));
  }

  {
    // Create Staging Buffer
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = 65'536'000;
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
    staging_buffer.size = buffer_create_info.size;
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
    std::variant<std::weak_ptr<Resource>, std::unique_ptr<Resource>> resource) {
  static uint64_t next_id = 0;
  uint64_t id = next_id++;

  this->resources.insert_or_assign(id, std::move(resource));

  return id;
}

void resource_handler::ResourceHandler::updateTransistion(
    VkCommandBuffer command_buffer, TransistionData transistion_data,
    uint64_t resource_idx) {

  auto it = this->resources.find(resource_idx);

  if (it == resources.end())
    return;

  Resource *resource =
      std::holds_alternative<std::weak_ptr<Resource>>(it->second)
          ? std::get<std::weak_ptr<Resource>>(it->second).lock().get()
          : std::get<std::unique_ptr<Resource>>(it->second).get();

  if (std::holds_alternative<Image>(resource->resource_data)) {
    // Replace later with resource transistion manager update

    auto &image = std::get<Image>(resource->resource_data);

    VkImageMemoryBarrier2KHR memory_barrier = {};
    memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
    memory_barrier.oldLayout = getImageLayout(image.current_layout);
    memory_barrier.newLayout =
        getImageLayout(transistion_data.data.image_data.image_layout);
    memory_barrier.srcStageMask = getStageMask(image.current_layout);

    memory_barrier.srcAccessMask = getAccessMask(image.current_layout);
    memory_barrier.dstAccessMask =
        getAccessMask(transistion_data.data.image_data.image_layout);
    memory_barrier.dstStageMask =
        getStageMask(transistion_data.data.image_data.image_layout);
    memory_barrier.image = image.image;
    memory_barrier.subresourceRange = image.range;

    memory_barrier.srcQueueFamilyIndex = transistion_data.source_queue_family;
    memory_barrier.dstQueueFamilyIndex = transistion_data.dst_queue_family;

    VkDependencyInfoKHR dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &memory_barrier;

    vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);

    // Update Layout
    image.current_layout = transistion_data.data.image_data.image_layout;

  } else {

    auto &buffer = std::get<Buffer>(resource->resource_data);

    // To do: Add Transisition Logic for Buffers
    VkBufferMemoryBarrier2KHR memory_barrier = {};
    memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
    memory_barrier.buffer = buffer.buffer;
    memory_barrier.dstAccessMask =
        getAccessMask(transistion_data.data.buffer_data.buffer_usage);
    memory_barrier.srcAccessMask = getAccessMask(buffer.current_buffer_usage);
    memory_barrier.srcStageMask = getStageMask(buffer.current_buffer_usage);
    memory_barrier.dstStageMask =
        getStageMask(transistion_data.data.buffer_data.buffer_usage);
    memory_barrier.dstQueueFamilyIndex = transistion_data.dst_queue_family;
    memory_barrier.srcQueueFamilyIndex = transistion_data.source_queue_family;
    memory_barrier.offset = 0;
    memory_barrier.size = buffer.size;

    VkDependencyInfoKHR dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependency_info.bufferMemoryBarrierCount = 1;
    dependency_info.pBufferMemoryBarriers = &memory_barrier;

    vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);

    buffer.current_buffer_usage =
        transistion_data.data.buffer_data.buffer_usage;
  }
}

resource_handler::ResourceHandle
resource_handler::ResourceHandler::loadImage(const std::string &path,
                                             VkFormat image_format,
                                             VkImageUsageFlags image_usage) {

  resource_handler::StagingTransferData transfer_data = {};

  int width, height, channels;

  stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);

  if (!pixels) {
    std::cerr << "No file found\n";
    return {UINT64_MAX, nullptr};
  }

  ResourceHandle handle =
      this->createImage(width, height, image_usage, image_format);

  {
    std::memcpy(reinterpret_cast<uint8_t *>(
                    staging_buffer.allocation_info.pMappedData) +
                    staging_buffer.offset,
                pixels, width * height * 4);

    transfer_data.source_offset = staging_buffer.offset;
    transfer_data.size = width * height * 4;

    transfer_data.target_offset = 0;
    transfer_data.resource_idx = handle.idx;
  }

  {
    staging_buffer.offset += width * height * 4;
  }

  this->transfers.push_back(transfer_data);

  bindSampledImage(handle.idx);

  return handle;
}

void resource_handler::ResourceHandler::clearStagingTransferData() {
  transfers.clear();
}

std::vector<resource_handler::StagingTransferData> &
resource_handler::ResourceHandler::getStagingTransferData() {
  return transfers;
}

uint64_t
resource_handler::ResourceHandler::bindSampledImage(uint64_t resource_index) {

  auto it = resources.find(resource_index);

  if (it == resources.end()) {
    return UINT64_MAX;
  }

  Resource *r = std::holds_alternative<std::weak_ptr<Resource>>(it->second)
                    ? std::get<std::weak_ptr<Resource>>(it->second).lock().get()
                    : std::get<std::unique_ptr<Resource>>(it->second).get();

  if (std::holds_alternative<Image>(r->resource_data) == false) {
    return UINT64_MAX;
  }

  auto &image = std::get<Image>(r->resource_data);

  if (image.sampled_image_binding_slot != UINT64_MAX) {
    // Already bound

    return image.sampled_image_binding_slot;
  }

  uint64_t binding_slot = getNewSampledImageBindingSlot();

  VkDescriptorImageInfo image_info = {};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = image.view;
  image_info.sampler = image.sampler;

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.dstArrayElement = binding_slot;
  write.dstBinding = 0;
  write.dstSet = sampled_images_descriptor.descriptor;

  write.pImageInfo = &image_info;

  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

  image.sampled_image_binding_slot = binding_slot;
  return binding_slot;
}

resource_handler::ResourceHandle resource_handler::ResourceHandler::createImage(
    uint32_t width, uint32_t height, VkImageUsageFlags image_usage,
    VkFormat image_format, bool is_permanent) {

  uint64_t resource_index = UINT64_MAX;

  Image new_image = {};
  new_image.current_layout = ImageLayouts::UNDEFINED;
  new_image.range.layerCount = 1;
  new_image.range.levelCount = 1;
  new_image.range.baseMipLevel = 0;
  new_image.range.baseArrayLayer = 0;
  new_image.range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  new_image.extent = {static_cast<uint32_t>(width),
                      static_cast<uint32_t>(height), 1};

  VkImageCreateInfo image_create_info = {};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.usage = image_usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.queueFamilyIndexCount = 1;
  image_create_info.pQueueFamilyIndices = &transfer_queue_index;
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

  VK_ERROR(vmaCreateImage(allocator, &image_create_info,
                          &allocation_create_info, &new_image.image,
                          &new_image.allocation, &new_image.allocation_info));

  VkImageViewCreateInfo view_create_info = {};
  view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_create_info.image = new_image.image;
  view_create_info.subresourceRange = new_image.range;
  view_create_info.format = image_format;

  VK_ERROR(vkCreateImageView(this->device, &view_create_info, nullptr,
                             &new_image.view));

  {
    VkSamplerCreateInfo sampler_create_info = {};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.anisotropyEnable = VK_FALSE;
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_NEVER;
    sampler_create_info.magFilter = VK_FILTER_LINEAR;
    sampler_create_info.maxAnisotropy = 1.0f;
    sampler_create_info.maxLod = 0.0f;
    sampler_create_info.minFilter = VK_FILTER_LINEAR;
    sampler_create_info.mipLodBias = 0.0f;

    VK_ERROR(vkCreateSampler(this->device, &sampler_create_info, nullptr,
                             &new_image.sampler));
  }

  if (is_permanent) {
    std::unique_ptr<Resource> unique_r =
        std::make_unique<Resource>(device, allocator);

    unique_r->resource_data = new_image;

    resource_index = insertResource(std::move(std::move(unique_r)));

    return {resource_index, unique_r.get()};

  } else {

    std::shared_ptr<Resource> shared_r =
        std::make_shared<Resource>(device, allocator);

    shared_r->resource_data = new_image;

    std::weak_ptr<Resource> weak_r = shared_r;

    resource_index = insertResource(weak_r);

    return {resource_index, shared_r};
  }
}
