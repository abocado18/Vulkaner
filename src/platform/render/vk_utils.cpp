#include "vk_utils.h"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

void vk_utils::transistionImage(VkCommandBuffer cmd_buffer,
                                VkImageLayout current_layout,
                                VkImageLayout new_layout, VkImage image, uint32_t mip_levels, uint32_t array_layers,
                                uint32_t src_queue_family,
                                uint32_t dst_queue_family) {

  VkImageMemoryBarrier2 image_barrier = {};
  image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

  

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
      vk_utils::getImageSubResourceRange(aspect_mask, mip_levels, array_layers);

  image_barrier.image = image;

  VkDependencyInfo dep_info = {};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &image_barrier;

  vkCmdPipelineBarrier2(cmd_buffer, &dep_info);
}

void vk_utils::transistionBuffer(VkCommandBuffer command_buffer,
                       VkAccessFlags current_access, VkAccessFlags new_access, uint32_t size, uint32_t offset,
                       VkBuffer buffer, uint32_t src_queue_family,
                       uint32_t dst_queue_family) {

  VkBufferMemoryBarrier buffer_barrier = {};
  buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  buffer_barrier.buffer = buffer;
  buffer_barrier.srcAccessMask = current_access;
  buffer_barrier.dstAccessMask = new_access;
  buffer_barrier.offset = offset;
  buffer_barrier.size = size;
  buffer_barrier.srcQueueFamilyIndex = src_queue_family == UINT32_MAX
                                           ? VK_QUEUE_FAMILY_IGNORED
                                           : src_queue_family;
  buffer_barrier.dstQueueFamilyIndex = dst_queue_family == UINT32_MAX
                                           ? VK_QUEUE_FAMILY_IGNORED
                                           : dst_queue_family;

  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1,
                       &buffer_barrier, 0, nullptr);
}

VkImageSubresourceRange
vk_utils::getImageSubResourceRange(VkImageAspectFlags aspect_mask, uint32_t mip_levels, uint32_t array_layers) {
  VkImageSubresourceRange range = {};
  range.aspectMask = aspect_mask;
  range.baseMipLevel = 0;
  range.baseArrayLayer = 0;

  range.levelCount = mip_levels;
  range.layerCount = array_layers;

  return range;
}

VkCommandBufferSubmitInfo
vk_utils::commandBufferSubmitInfo(VkCommandBuffer cmd_buffer) {

  VkCommandBufferSubmitInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.pNext = nullptr;
  info.commandBuffer = cmd_buffer;
  info.deviceMask = 0;

  return info;
}

VkSemaphoreSubmitInfo
vk_utils::semaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask,
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

VkSubmitInfo2
vk_utils::submitInfo(VkCommandBufferSubmitInfo *cmd,
                     VkSemaphoreSubmitInfo *signal_semaphore_infos,
                     uint32_t signal_semaphore_count,
                     VkSemaphoreSubmitInfo *wait_semaphore_infos,
                     uint32_t wait_semaphore_count) {

  VkSubmitInfo2 info = {};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  info.pNext = nullptr;

  info.waitSemaphoreInfoCount = wait_semaphore_count;
  info.pWaitSemaphoreInfos = wait_semaphore_infos;

  info.signalSemaphoreInfoCount = signal_semaphore_count;
  info.pSignalSemaphoreInfos = signal_semaphore_infos;

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

  VkImageBlit2 blit_region = {};
  blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

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

  VkBlitImageInfo2 blit_info = {};
  blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
  blit_info.dstImage = dst;
  blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blit_info.srcImage = source;
  blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blit_info.filter = VK_FILTER_LINEAR;
  blit_info.regionCount = 1;
  blit_info.pRegions = &blit_region;

  vkCmdBlitImage2(cmd, &blit_info);
}

VkImageViewCreateInfo
vk_utils::imageViewCreateInfo(VkFormat format, VkImage image,
                              VkImageAspectFlags aspect_flags,
                              VkImageViewType view_type) {
  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

  info.viewType = view_type;
  info.format = format;
  info.image = image;

  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspect_flags;

  return info;
}

VkRenderingAttachmentInfo vk_utils::attachmentInfo(VkImageView view,
                                                   VkClearValue *clear,
                                                   VkImageLayout layout) {
  VkRenderingAttachmentInfo colorAttachment{};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.pNext = nullptr;

  colorAttachment.imageView = view;
  colorAttachment.imageLayout = layout;
  colorAttachment.loadOp =
      clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (clear) {
    colorAttachment.clearValue = *clear;
  }

  return colorAttachment;
}

VkRenderingInfo
vk_utils::renderingInfo(VkRenderingAttachmentInfo *color_attachments,
                        uint32_t color_attachments_count, VkExtent2D extent,
                        VkOffset2D offset, VkRenderingAttachmentInfo *depth,
                        VkRenderingAttachmentInfo *stencil) {
  VkRenderingInfo rendering_info = {};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.pColorAttachments = color_attachments;
  rendering_info.colorAttachmentCount = color_attachments_count;
  rendering_info.renderArea.extent = extent;
  rendering_info.renderArea.offset = offset;
  rendering_info.layerCount = 1;
  rendering_info.viewMask = 0;
  rendering_info.pDepthAttachment = depth;
  rendering_info.pStencilAttachment = stencil;

  return rendering_info;
}

VkPipelineShaderStageCreateInfo
vk_utils::pipelineShaderStageCreateInfo(VkShaderStageFlagBits shader_stage,
                                        VkShaderModule shader_module) {

  VkPipelineShaderStageCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  create_info.stage = shader_stage;
  create_info.module = shader_module;
  return create_info;
}
