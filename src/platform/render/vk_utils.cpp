#include "vk_utils.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

void vk_utils::transistionImage(VkCommandBuffer cmd_buffer,
                                VkImageLayout current_layout,
                                VkImageLayout new_layout, VkImage image,
                                uint32_t src_queue_family,
                                uint32_t dst_queue_family) {

  VkImageMemoryBarrier2KHR image_barrier = {};
  image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;

  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.dstAccessMask =
      VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  image_barrier.oldLayout = current_layout;
  image_barrier.newLayout = new_layout;

  image_barrier.srcQueueFamilyIndex = src_queue_family == UINT32_MAX
                                          ? VK_QUEUE_FAMILY_IGNORED
                                          : src_queue_family;

  image_barrier.dstQueueFamilyIndex = dst_queue_family == UINT32_MAX
                                          ? VK_QUEUE_FAMILY_IGNORED
                                          : dst_queue_family;

  VkImageAspectFlags aspect_mask =
      (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : VK_IMAGE_ASPECT_COLOR_BIT;

  image_barrier.subresourceRange =
      vk_utils::getImageSubResourceRange(aspect_mask);

  image_barrier.image = image;

  VkDependencyInfoKHR dep_info = {};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &image_barrier;

  vkCmdPipelineBarrier2KHR(cmd_buffer, &dep_info);
}

VkImageSubresourceRange
vk_utils::getImageSubResourceRange(VkImageAspectFlags aspect_mask) {
  VkImageSubresourceRange range = {};
  range.aspectMask = aspect_mask;
  range.baseMipLevel = 0;
  range.baseArrayLayer = 0;

  range.levelCount = 1;
  range.layerCount = 1;

  return range;
}

VkCommandBufferSubmitInfoKHR
vk_utils::commandBufferSubmitInfo(VkCommandBuffer cmd_buffer) {

  VkCommandBufferSubmitInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.pNext = nullptr;
  info.commandBuffer = cmd_buffer;
  info.deviceMask = 0;

  return info;
}

VkSemaphoreSubmitInfoKHR
vk_utils::semaphoreSubmitInfo(VkPipelineStageFlags2KHR stage_mask,
                              VkSemaphore semaphore) {

  VkSemaphoreSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  submit_info.semaphore = semaphore;
  submit_info.stageMask = stage_mask;
  submit_info.deviceIndex = 0;
  submit_info.value = 1;

  return submit_info;
}

VkSubmitInfo2KHR
vk_utils::submitInfo(VkCommandBufferSubmitInfoKHR *cmd,
                     VkSemaphoreSubmitInfoKHR *signal_semaphore_info,
                     VkSemaphoreSubmitInfoKHR *wait_semaphore_info) {

  VkSubmitInfo2KHR info = {};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
  info.pNext = nullptr;

  info.waitSemaphoreInfoCount = wait_semaphore_info == nullptr ? 0 : 1;
  info.pWaitSemaphoreInfos = wait_semaphore_info;

  info.signalSemaphoreInfoCount = signal_semaphore_info == nullptr ? 0 : 1;
  info.pSignalSemaphoreInfos = signal_semaphore_info;

  info.commandBufferInfoCount = 1;
  info.pCommandBufferInfos = cmd;

  return info;
}

VkImageCreateInfo vk_utils::imageCreateInfo(VkFormat format,
                                            VkImageUsageFlags usage_flags,
                                            VkExtent3D extent) {
  VkImageCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

  create_info.imageType = VK_IMAGE_TYPE_2D;

  create_info.format = format;
  create_info.usage = usage_flags;

  create_info.mipLevels = 1;
  create_info.arrayLayers = 1;

  create_info.samples = VK_SAMPLE_COUNT_1_BIT;

  create_info.tiling = VK_IMAGE_TILING_OPTIMAL;

  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  create_info.extent = extent;

  return create_info;
}

void vk_utils::copyImageToImage(VkCommandBuffer cmd, VkImage source,
                                VkImage dst, VkExtent2D src_size,
                                VkExtent2D dst_size) {

  VkImageBlit2KHR blit_region = {};
  blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR;

  blit_region.srcOffsets[0] = {0, 0, 0};
  blit_region.dstOffsets[0] = {0, 0, 0};

  blit_region.srcOffsets[1].x = src_size.width;
  blit_region.srcOffsets[1].y = src_size.height;
  blit_region.srcOffsets[1].z = 1;

  blit_region.dstOffsets[1].x = dst_size.width;
  blit_region.dstOffsets[1].y = dst_size.height;
  blit_region.dstOffsets[1].z = 1;

  blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.srcSubresource.baseArrayLayer = 0;
  blit_region.srcSubresource.layerCount = 1;
  blit_region.srcSubresource.mipLevel = 0;

  blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.dstSubresource.baseArrayLayer = 0;
  blit_region.dstSubresource.layerCount = 1;
  blit_region.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2KHR blit_info = {};
  blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR;
  blit_info.dstImage = dst;
  blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blit_info.srcImage = source;
  blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blit_info.filter = VK_FILTER_LINEAR;
  blit_info.regionCount = 1;
  blit_info.pRegions = &blit_region;

  vkCmdBlitImage2KHR(cmd, &blit_info);
}

VkImageViewCreateInfo
vk_utils::imageViewCreateInfo(VkFormat format, VkImage image,
                              VkImageAspectFlags aspect_flags) {
  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = format;
  info.image = image;

  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspect_flags;

  return info;
}