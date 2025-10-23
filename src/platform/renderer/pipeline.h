#pragma once

#include "volk.h"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace pipeline {

struct PipelineData {

  VkPushConstantRange push_constant_range;

  VkPipelineVertexInputStateCreateInfo vertex_create_info;
  VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
  VkPipelineMultisampleStateCreateInfo multisample_create_info;
  VkPipelineRasterizationStateCreateInfo rasterization_create_info;


    std::vector<VkDynamicState> dynamic_states;
  VkPipelineDynamicStateCreateInfo dynamic_create_info;

  VkPipelineViewportStateCreateInfo viewport_create_info;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;

  std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;
  VkPipelineColorBlendStateCreateInfo blend_state_create_info;

  VkPipelineRenderingCreateInfoKHR rendering_create_info;

  PipelineData getDefault();

};

struct RenderPipeline
{
  VkPipelineLayout layout;
  VkPipeline pipeline;
};

class PipelineManager {};

} // namespace pipeline