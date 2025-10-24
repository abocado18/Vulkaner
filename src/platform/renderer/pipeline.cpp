#include "pipeline.h"
#include "vertex.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <slang-com-ptr.h>
#include <slang-deprecated.h>
#include <slang.h>
#include <string>

#include "vulkan_macros.h"

pipeline::PipelineManager::PipelineManager(VkDevice &device,
                                           const std::string &shader_path)
    : device(device) {

  slang::createGlobalSession(global_session.writeRef());

  session_desc = {};

  slang::TargetDesc target_desc = {};

  target_desc.format = SLANG_SPIRV;
  target_desc.profile = global_session->findProfile("spirv_1_5");

  session_desc.targetCount = 1;
  session_desc.targets = &target_desc;

  std::array<slang::CompilerOptionEntry, 1> options = {
      {slang::CompilerOptionName::EmitSpirvDirectly,
       {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}}};
  session_desc.compilerOptionEntries = options.data();
  session_desc.compilerOptionEntryCount = options.size();

  std::array<const char *, 1> search_paths = {shader_path.c_str()};

  session_desc.searchPathCount = search_paths.size();
  session_desc.searchPaths = search_paths.data();

  global_session->createSession(session_desc, session.writeRef());
}

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

uint64_t pipeline::PipelineManager::createRenderPipeline(
    pipeline::PipelineData &pipeline_data,
    const std::string &vertex_shader_path,
    const std::string &pixel_shader_path) {

  static uint64_t next_id = 0;
  const uint64_t id = next_id++;

  pipeline::RenderPipeline pipeline = {};

  std::array<VkShaderModule, 2> modules = {};

  modules[0] = createShaderModule(vertex_shader_path);

  if (pixel_shader_path != "") {
    modules[1] = createShaderModule(pixel_shader_path);
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stage_create_infos = {};
  shader_stage_create_infos[0].sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_infos[0].pName = "main";
  shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stage_create_infos[0].module = modules[0];

  if (pixel_shader_path != "") {
    shader_stage_create_infos[1].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[1].pName = "main";
    shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_create_infos[1].module = modules[1];
  }

  VkPipelineLayoutCreateInfo layout_create_info = {};
  layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_create_info.pushConstantRangeCount = 1;
  layout_create_info.pPushConstantRanges = &pipeline_data.push_constant_range;
  layout_create_info.setLayoutCount = 0; // Add bindless descriptor sets later

  VK_ERROR(vkCreatePipelineLayout(device, &layout_create_info, nullptr,
                                  &pipeline.layout));

  VkGraphicsPipelineCreateInfo pipeline_create_info = {};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_create_info.layout = pipeline.layout;
  pipeline_create_info.pColorBlendState =
      &pipeline_data.blend_state_create_info;
  pipeline_create_info.pDepthStencilState =
      &pipeline_data.depth_stencil_create_info;
  pipeline_create_info.pDynamicState = &pipeline_data.dynamic_create_info;
  pipeline_create_info.pInputAssemblyState =
      &pipeline_data.assembly_create_info;
  pipeline_create_info.pMultisampleState =
      &pipeline_data.multisample_create_info;
  pipeline_create_info.pRasterizationState =
      &pipeline_data.rasterization_create_info;
  pipeline_create_info.pVertexInputState = &pipeline_data.vertex_create_info;
  pipeline_create_info.pViewportState = &pipeline_data.viewport_create_info;
  pipeline_create_info.subpass = 0;
  pipeline_create_info.stageCount = pixel_shader_path == "" ? 1 : 2;
  pipeline_create_info.pStages = shader_stage_create_infos.data();
  pipeline_create_info.renderPass = 0; // Dynamic Rendering
  pipeline_create_info.pNext = &pipeline_data.rendering_create_info;

  // To do: Add Pipeline Caching
  VK_ERROR(vkCreateGraphicsPipelines(device, 0, 1, &pipeline_create_info,
                                     nullptr, &pipeline.pipeline));

  pipelines[id] = pipeline;

  return id;
}

VkShaderModule
pipeline::PipelineManager::createShaderModule(const std::string &path) {

#ifndef PRODUCTION_BUILD
  if (path.find(".spv") == std::string::npos &&
      path.find(".slang") == std::string::npos) {
    std::cerr << "Must be Spir-V binary file or a slang source file\n";
    std::abort();
  }

#endif

  if (path.find(".slang") != std::string::npos) {
    Slang::ComPtr<slang::ICompileRequest> compileRequest;
    session->createCompileRequest(compileRequest.writeRef());

    compileRequest->addTranslationUnitSourceFile(int translationUnitIndex, const char *path)
  }

  std::ifstream file(path, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t file_size = (size_t)file.tellg();
  std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

  file.seekg(0);
  file.read((char *)buffer.data(), file_size);

  file.close();

  VkShaderModuleCreateInfo create_info = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  create_info.codeSize = file_size;
  create_info.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

  VkShaderModule shader_module;
  VK_ERROR(vkCreateShaderModule(device, &create_info, VK_NULL_HANDLE,
                                &shader_module));

  return shader_module;
}