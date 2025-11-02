#pragma once

#include "volk.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "allocator/vk_mem_alloc.h"

#include "stb_image/stb_image.h"
#include "vulkan_macros.h"

namespace resource_handler {

enum ImageLayouts : uint32_t {
  UNDEFINED = 1 << 0,
  GENERAL = 1 << 1,
  COLOR_ATTACHMENT_OPTIMAL = 1 << 2,
  DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 1 << 3,
  DEPTH_STENCIL_READ_ONLY_OPTIMAL = 1 << 4,
  SHADER_READ_ONLY_OPTIMAL = 1 << 5,
  TRANSFER_SRC_OPTIMAL = 1 << 6,
  TRANSFER_DST_OPTIMAL = 1 << 7,
  PREINITIALIZED = 1 << 8,
  DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1 << 9,
  DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1 << 10,
  PRESENT_SRC_KHR = 1 << 11,
  SHARED_PRESENT_KHR = 1 << 12,
};

inline VkImageLayout getImageLayout(ImageLayouts layout) {
  switch (layout) {
  case UNDEFINED:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case GENERAL:
    return VK_IMAGE_LAYOUT_GENERAL;
  case COLOR_ATTACHMENT_OPTIMAL:
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  case DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  case DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  case SHADER_READ_ONLY_OPTIMAL:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case TRANSFER_SRC_OPTIMAL:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case TRANSFER_DST_OPTIMAL:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case PREINITIALIZED:
    return VK_IMAGE_LAYOUT_PREINITIALIZED;
  case DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
  case DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
  case PRESENT_SRC_KHR:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  case SHARED_PRESENT_KHR:
    return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
  default:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

inline VkPipelineStageFlags2 getStageMask(ImageLayouts layout) {
  switch (layout) {
  case UNDEFINED:
    return VK_PIPELINE_STAGE_2_NONE;
  case GENERAL:
    return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
  case COLOR_ATTACHMENT_OPTIMAL:
    return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
  case DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR |
           VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;
  case DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR |
           VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR |
           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  case SHADER_READ_ONLY_OPTIMAL:
    return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR |
           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR |
           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
  case TRANSFER_SRC_OPTIMAL:
  case TRANSFER_DST_OPTIMAL:
    return VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;
  case PREINITIALIZED:
    return VK_PIPELINE_STAGE_2_HOST_BIT_KHR;
  case DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
  case DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR |
           VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR |
           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  case PRESENT_SRC_KHR:
    return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
  case SHARED_PRESENT_KHR:
    return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
  default:
    return VK_PIPELINE_STAGE_2_NONE;
  }
}

inline VkAccessFlags2 getAccessMask(ImageLayouts layout) {
  switch (layout) {
  case UNDEFINED:
    return 0;
  case GENERAL:
    return VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
  case COLOR_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR |
           VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
  case DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR |
           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR;
  case DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR;
  case SHADER_READ_ONLY_OPTIMAL:
    return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR |
           VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR;
  case TRANSFER_SRC_OPTIMAL:
    return VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
  case TRANSFER_DST_OPTIMAL:
    return VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
  case PREINITIALIZED:
    return VK_ACCESS_2_HOST_WRITE_BIT_KHR;
  case DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR;
  case DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR;
  case PRESENT_SRC_KHR:
    return 0;
  case SHARED_PRESENT_KHR:
    return 0;
  default:
    return 0;
  }
}

enum BufferUsages : uint32_t {
  BUFFER_USAGE_UNDEFINED = 1 << 0,
  BUFFER_USAGE_VERTEX_BUFFER = 1 << 1,
  BUFFER_USAGE_INDEX_BUFFER = 1 << 2,
  BUFFER_USAGE_UNIFORM_BUFFER = 1 << 3,
  BUFFER_USAGE_STORAGE_BUFFER = 1 << 4,
  BUFFER_USAGE_INDIRECT_BUFFER = 1 << 5,
  BUFFER_USAGE_TRANSFER_SRC = 1 << 6,
  BUFFER_USAGE_TRANSFER_DST = 1 << 7,
  BUFFER_USAGE_HOST_READ = 1 << 8,
  BUFFER_USAGE_HOST_WRITE = 1 << 9,
};

inline VkBufferUsageFlags getBufferUsageFlags(BufferUsages usage) {
  switch (usage) {
  case BUFFER_USAGE_VERTEX_BUFFER:
    return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  case BUFFER_USAGE_INDEX_BUFFER:
    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  case BUFFER_USAGE_UNIFORM_BUFFER:
    return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  case BUFFER_USAGE_STORAGE_BUFFER:
    return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  case BUFFER_USAGE_INDIRECT_BUFFER:
    return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  case BUFFER_USAGE_TRANSFER_SRC:
    return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  case BUFFER_USAGE_TRANSFER_DST:
    return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  default:
    return 0;
  }
}

inline VkPipelineStageFlags2 getStageMask(BufferUsages usage) {
  switch (usage) {
  case BUFFER_USAGE_UNDEFINED:
    return VK_PIPELINE_STAGE_2_NONE;

  case BUFFER_USAGE_VERTEX_BUFFER:
    return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR;

  case BUFFER_USAGE_INDEX_BUFFER:
    return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR;

  case BUFFER_USAGE_UNIFORM_BUFFER:
    return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR |
           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR |
           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

  case BUFFER_USAGE_STORAGE_BUFFER:
    return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR |
           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR |
           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

  case BUFFER_USAGE_INDIRECT_BUFFER:
    return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR;

  case BUFFER_USAGE_TRANSFER_SRC:
  case BUFFER_USAGE_TRANSFER_DST:
    return VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR;

  case BUFFER_USAGE_HOST_READ:
  case BUFFER_USAGE_HOST_WRITE:
    return VK_PIPELINE_STAGE_2_HOST_BIT_KHR;

  default:
    return VK_PIPELINE_STAGE_2_NONE;
  }
}

inline VkAccessFlags2 getAccessMask(BufferUsages usage) {
  switch (usage) {
  case BUFFER_USAGE_UNDEFINED:
    return 0;

  case BUFFER_USAGE_VERTEX_BUFFER:
    return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;

  case BUFFER_USAGE_INDEX_BUFFER:
    return VK_ACCESS_2_INDEX_READ_BIT_KHR;

  case BUFFER_USAGE_UNIFORM_BUFFER:
    return VK_ACCESS_2_UNIFORM_READ_BIT_KHR;

  case BUFFER_USAGE_STORAGE_BUFFER:
    return VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR |
           VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;

  case BUFFER_USAGE_INDIRECT_BUFFER:
    return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;

  case BUFFER_USAGE_TRANSFER_SRC:
    return VK_ACCESS_2_TRANSFER_READ_BIT_KHR;

  case BUFFER_USAGE_TRANSFER_DST:
    return VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;

  case BUFFER_USAGE_HOST_READ:
    return VK_ACCESS_2_HOST_READ_BIT_KHR;

  case BUFFER_USAGE_HOST_WRITE:
    return VK_ACCESS_2_HOST_WRITE_BIT_KHR;

  default:
    return 0;
  }
}

enum class ResourceType { BUFFER, IMAGE };

struct TransistionImageData {

  ImageLayouts image_layout;
};

struct TransistionBufferData {
  BufferUsages buffer_usage;
};

struct TransistionData {
  ResourceType type;

  union {
    TransistionBufferData buffer_data;
    TransistionImageData image_data;
  } data;

  uint32_t source_queue_family = VK_QUEUE_FAMILY_IGNORED;
  uint32_t dst_queue_family = VK_QUEUE_FAMILY_IGNORED;
};

struct Image {
  VkImage image;
  ImageLayouts current_layout;
  VkImageSubresourceRange range;

  VkExtent3D extent;

  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;

  VkImageView view;

  VkSampler sampler;
  uint64_t sampled_image_binding_slot = UINT64_MAX;
};

struct Buffer {

  Buffer() = default;

  VkBuffer buffer;
  BufferUsages current_buffer_usage;
  VmaAllocationInfo allocation_info;
  VmaAllocation allocation;
  uint64_t size;
  uint32_t offset = 0;
  uint64_t address = UINT64_MAX;
};

struct Descriptor {
  VkDescriptorSetLayout layout;
  VkDescriptorSet descriptor;
  VkDescriptorPool pool;
};

struct Resource {

  ResourceType type;

  union {
    Image image;
    Buffer buffer;
  } resource_data;
};

struct StagingTransferData {

  // Idx of Target
  uint64_t resource_idx;

  uint32_t source_offset;
  uint32_t target_offset;
  uint32_t size;
};

class ResourceHandler {
public:
  ResourceHandler(VkPhysicalDevice &ph_device, VkDevice &device,
                  VmaAllocator &allocator, uint32_t transfer_queue_index);
  ~ResourceHandler();

  // Use this for barriers for resources
  void updateTransistion(VkCommandBuffer command_buffer,
                         TransistionData transistion_data,
                         uint64_t resource_idx);

  uint64_t insertResource(Resource &resource);

  uint64_t loadImage(const std::string &path, VkFormat image_format,
                     VkImageUsageFlags image_usage);

  uint64_t bindSampledImage(uint64_t resource_idx);

  // Create buffer and caches in hashmap, returns resource index
  template <typename T>
  uint64_t createBuffer(uint32_t size, BufferUsages buffer_usage) {
    Buffer buffer;

    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.queueFamilyIndexCount = 1;
    buffer_create_info.pQueueFamilyIndices = &transfer_queue_index;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.size = sizeof(T) * size;
    buffer_create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                               getBufferUsageFlags(buffer_usage);

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    alloc_create_info.priority = 1.0f;
    assert(buffer_create_info.size > 0);

    VK_ERROR(vmaCreateBuffer(allocator, &buffer_create_info, &alloc_create_info,
                             &buffer.buffer, &buffer.allocation,
                             &buffer.allocation_info));

    VkBufferDeviceAddressInfoKHR address_info = {};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
    address_info.buffer = buffer.buffer;

    buffer.address = vkGetBufferDeviceAddressKHR(device, &address_info);
    buffer.offset = 0;

    Resource r = {};
    r.type = ResourceType::BUFFER;
    r.resource_data.buffer = buffer;
    r.resource_data.buffer.current_buffer_usage =
        BufferUsages::BUFFER_USAGE_UNDEFINED;
    r.resource_data.buffer.size = sizeof(T) * size;

    uint64_t resource_idx = insertResource(r);

    buffers_per_type[typeid(T)] = resource_idx;

    return resource_idx;
  }

  const Resource &getResource(uint64_t idx) const { return resources.at(idx); };

  // writes to position in buffer and returns correct element index
  template <typename T>
  uint32_t writeToBuffer(T *data, uint32_t index = UINT32_MAX) {

    auto it = buffers_per_type.find(std::type_index(typeid(T)));

    if (it == buffers_per_type.end()) {
      std::cerr << "Buffer of type " << typeid(T).name()
                << "does not exist yet\n";
      return UINT32_MAX;
    }

    uint64_t buffer_resource_index = it->second;

    Resource &buffer_resource = resources.at(buffer_resource_index);

    if (staging_buffer.offset + sizeof(T) > staging_buffer.size) {
      std::cout << "Max size of staging buffer reached\n";
      return UINT32_MAX;
    }

    if (index == UINT32_MAX &&
        buffer_resource.resource_data.buffer.offset + sizeof(T) >
            buffer_resource.resource_data.buffer.size) {
      std::cout << "Full size of buffer reached\n";
      return UINT32_MAX;
    }

    std::memcpy(reinterpret_cast<uint8_t *>(
                    staging_buffer.allocation_info.pMappedData) +
                    staging_buffer.offset,
                data, sizeof(T));

    StagingTransferData transfer_data = {};
    transfer_data.source_offset = staging_buffer.offset;
    transfer_data.size = sizeof(T);
    transfer_data.resource_idx = buffer_resource_index;

    transfer_data.target_offset =
        index == UINT32_MAX ? buffer_resource.resource_data.buffer.offset
                            : index * sizeof(T);

    {
      staging_buffer.offset += sizeof(T);

      if (index == UINT32_MAX)
        buffer_resource.resource_data.buffer.offset += sizeof(T);
    }

    transfers.push_back(transfer_data);

    uint32_t element_index =
        static_cast<uint32_t>(transfer_data.target_offset / sizeof(T));
    return element_index;
  }

  template <typename T> Buffer &getBufferPerType() {
    uint64_t idx = buffers_per_type.at(typeid(T));

    return resources.at(idx).resource_data.buffer;
  }

  std::vector<StagingTransferData> &getStagingTransferData();

  void clearStagingTransferData();

  const VkBuffer &getStagingBuffer() { return staging_buffer.buffer; }

  const Descriptor &getSampledImagesDescriptor() const {
    return sampled_images_descriptor;
  }

private:
  std::unordered_map<uint64_t, Resource> resources = {};

  VkPhysicalDeviceProperties device_properties;
  VkDevice &device;
  VmaAllocator &allocator;

  Buffer staging_buffer;

  uint32_t storage_pointer_buffer_max;

  Descriptor sampled_images_descriptor;

  uint32_t sampled_images_limit;

  uint32_t transfer_queue_index;

  std::vector<StagingTransferData> transfers;

  uint64_t getNewSampledImageBindingSlot() {
    static uint64_t new_slot = 0;

    if (new_slot > sampled_images_limit) {
      std::cerr << "Max value for sampled images readched\n";
      return UINT64_MAX;
    }

    return new_slot++;
  }

  uint32_t getNewStoragePointerIndexSlot() {
    static uint32_t new_slot = 0;

    if (new_slot > storage_pointer_buffer_max) {
      std::cerr << "Max valufe for storage pointer index reached\n";
      return UINT32_MAX;
    }
    return new_slot++;
  }

  std::unordered_map<std::type_index, uint64_t> buffers_per_type = {};
};

} // namespace resource_handler