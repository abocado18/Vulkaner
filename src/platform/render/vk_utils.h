#pragma once
#include "volk.h"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "allocator/vk_mem_alloc.h"

namespace vk_utils {

void transistionImage(VkCommandBuffer cmd_buffer, VkImageLayout current_layout,
                      VkImageLayout new_layout, VkImage image, uint32_t mip_levels = 1, uint32_t array_layers = 1,
                      uint32_t src_queue_family = UINT32_MAX,
                      uint32_t dst_queue_family = UINT32_MAX);

void transistionBuffer(VkCommandBuffer command_buffer,
                       VkAccessFlags current_access, VkAccessFlags new_access, uint32_t size, uint32_t offset,
                       VkBuffer buffer, uint32_t src_queue_family = UINT32_MAX,
                       uint32_t dst_queue_family = UINT32_MAX);

VkImageSubresourceRange
getImageSubResourceRange(VkImageAspectFlags aspect_mask, uint32_t mip_levels, uint32_t array_layers);

VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask,
                                          VkSemaphore semaphore);

VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd_buffer);

VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signal_semaphore_infos,
                         uint32_t signal_semaphore_count,
                         VkSemaphoreSubmitInfo *wait_semaphore_infos,
                         uint32_t wait_semaphore_count);

VkImageCreateInfo imageCreateInfo(VkFormat format,
                                  VkImageUsageFlags usage_flags, VkExtent3D);

VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image,
                                          VkImageAspectFlags aspect_flags,
                                          VkImageViewType view_type);

void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage dst,
                      VkExtent2D src_size, VkExtent2D dst_size);

VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkClearValue *clear,
                                         VkImageLayout layout);

VkRenderingInfo renderingInfo(VkRenderingAttachmentInfo *color_attachments,
                              uint32_t color_attachments_count,
                              VkExtent2D extent, VkOffset2D offset,
                              VkRenderingAttachmentInfo *depth,
                              VkRenderingAttachmentInfo *stencil);

VkPipelineShaderStageCreateInfo
pipelineShaderStageCreateInfo(VkShaderStageFlagBits shader_stage,
                              VkShaderModule shader_module);

} // namespace vk_utils
