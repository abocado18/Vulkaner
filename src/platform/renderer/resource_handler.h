#pragma once

#include "volk.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "allocator/vk_mem_alloc.h"

#include "stb_image/stb_image.h"

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

enum BufferAccessMasks {
  INDIRECT_COMMAND_READ_BIT = 1 << 0,
  INDEX_READ_BIT = 1 << 1,
  VERTEX_ATTRIBUTE_READ_BIT = 1 << 2,
  UNIFORM_READ_BIT = 1 << 3,
  SHADER_READ_BIT = 1 << 4,
  SHADER_WRITE_BIT = 1 << 5,
  COLOR_ATTACHMENT_READ_BIT = 1 << 6,
  COLOR_ATTACHMENT_WRITE_BIT = 1 << 7,
  DEPTH_STENCIL_ATTACHMENT_READ_BIT = 1 << 8,
  DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 1 << 9,
  TRANSFER_READ_BIT = 1 << 10,
  TRANSFER_WRITE_BIT = 1 << 11,
  HOST_READ_BIT = 1 << 12,
  HOST_WRITE_BIT = 1 << 13,
  MEMORY_READ_BIT = 1 << 14,
  MEMORY_WRITE_BIT = 1 << 15,

};

enum class ResourceType { BUFFER, IMAGE };

struct TransistionImageData {

  ImageLayouts image_layout;
};

struct TransistionBufferData {
  BufferAccessMasks access_mask;
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
  BufferAccessMasks current_access_mask;
  VmaAllocationInfo allocation_info;
  VmaAllocation allocation;
  uint32_t offset = 0;
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


  Resource *target;

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

  uint64_t loadImage(const std::string &path, VkFormat image_format, VkImageUsageFlags image_usage);

  uint64_t bindSampledImage(uint64_t resource_idx);

  std::vector<StagingTransferData> &getStagingTransferData();

  void clearStagingTransferData();

  const VkBuffer &getStagingBuffer()
  {
    return staging_buffer.buffer;
  }

private:
  

  std::unordered_map<uint64_t, Resource> resources = {};

  VkDevice &device;
  VmaAllocator &allocator;

  Buffer staging_buffer;

  Descriptor sampled_images_descriptor;

  uint32_t sampled_images_limit;

  uint32_t transfer_queue_index;

  std::vector<StagingTransferData> transfers;


  uint64_t getNewSampledImageBindingSlot()
  {
    static uint64_t new_slot = 0;

    if(new_slot > sampled_images_limit)
    {
      std::cerr << "Max value for sampled images readched\n";
      return UINT64_MAX;
    }

    return new_slot++;
  }

  
};

} // namespace resource_handler