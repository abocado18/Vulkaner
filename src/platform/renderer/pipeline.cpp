#include "pipeline.h"
#include "vertex.h"
#include <vulkan/vulkan_core.h>

pipeline::PipelineData pipeline::PipelineData::getDefault() {

  pipeline::PipelineData data = {};

  VkPushConstantRange push_constant_range = {};
  push_constant_range.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
  push_constant_range.size = 128;
  push_constant_range.offset = 0;

  data.push_constant_range = push_constant_range;

  vertex::VertexDesc vertex_desc = vertex::getVertexDesc();

  VkPipelineVertexInputStateCreateInfo vertex_input_state = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertex_input_state.vertexAttributeDescriptionCount =
      vertex_desc.attribute_desc_count;
  vertex_input_state.pVertexAttributeDescriptions = vertex_desc.attribute_descs;
  vertex_input_state.vertexBindingDescriptionCount = 2;
  vertex_input_state.pVertexBindingDescriptions = vertex_desc.binding_descs;

  data.vertex_create_info = vertex_input_state;

  VkPipelineInputAssemblyStateCreateInfo assembly_state = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  assembly_state.primitiveRestartEnable = VK_FALSE;

  data.assembly_create_info = assembly_state;

  VkPipelineMultisampleStateCreateInfo multi_sample = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multi_sample.sampleShadingEnable = VK_FALSE;
  multi_sample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multi_sample.minSampleShading = 1.f;
  multi_sample.alphaToCoverageEnable = VK_FALSE;
  multi_sample.alphaToOneEnable = VK_FALSE;

  data.multisample_create_info = multi_sample;

  VkPipelineRasterizationStateCreateInfo rasterization_state = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterization_state.depthBiasClamp = 0.f;
  rasterization_state.depthBiasConstantFactor = 0.f;
  rasterization_state.depthBiasEnable = VK_FALSE;
  rasterization_state.depthBiasSlopeFactor = 0.f;
  rasterization_state.depthClampEnable = VK_FALSE;
  rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization_state.lineWidth = 1.f;
  rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;

  data.rasterization_create_info = rasterization_state;

  data.dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic_state.dynamicStateCount = dynamic_states.size();
  dynamic_state.pDynamicStates = dynamic_states.data();

  data.dynamic_create_info = dynamic_state;

  VkPipelineViewportStateCreateInfo viewport_state = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport_state.scissorCount = 1;
  viewport_state.viewportCount = 1;

  data.viewport_create_info = viewport_state;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depth_stencil_state.stencilTestEnable = VK_FALSE;
  depth_stencil_state.minDepthBounds = 0.f;
  depth_stencil_state.maxDepthBounds = 1.f;
  depth_stencil_state.depthWriteEnable = VK_TRUE;
  depth_stencil_state.depthTestEnable = VK_TRUE;
  depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  depth_stencil_state.depthBoundsTestEnable = VK_FALSE;

  data.depth_stencil_create_info = depth_stencil_state;

  VkPipelineColorBlendAttachmentState blend_attachment = {};
  blend_attachment.blendEnable = VK_TRUE;
  blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

  data.blend_attachment_states.push_back(blend_attachment);

  VkPipelineColorBlendStateCreateInfo blend_state = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blend_state.attachmentCount = 1;
  blend_state.pAttachments = &data.blend_attachment_states[0];
  blend_state.logicOpEnable = VK_FALSE;

  data.blend_state_create_info = blend_state;

  VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;

  VkPipelineRenderingCreateInfoKHR rendering_create_info = {};
  rendering_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  rendering_create_info.colorAttachmentCount = 1;
  rendering_create_info.pColorAttachmentFormats = &color_format;
  rendering_create_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
  rendering_create_info.stencilAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

  data.rendering_create_info = rendering_create_info;

  return data;
}