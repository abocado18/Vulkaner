#include "resource_handler.h"
#include <vulkan/vulkan_core.h>

resource_handler::ResourceHandler::ResourceHandler() {}

resource_handler::ResourceHandler::~ResourceHandler() {}

uint64_t resource_handler::ResourceHandler::insertResource(
    resource_handler::Resource &resource) {
  static uint64_t next_id = 0;
  uint64_t id = next_id++;

  this->resources[id] = resource;

  return id;
}

void resource_handler::ResourceHandler::updateTransistionForLayouts(
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