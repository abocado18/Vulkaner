#include "pipeline.h"
#include "vertex.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang-deprecated.h>
#include <slang.h>
#include <string>
#include <vulkan/vulkan_core.h>

#include "vulkan_macros.h"

pipeline::PipelineManager::PipelineManager(VkDevice &device,
                                           const std::string &shader_path)
    : shader_path(shader_path), device(device) {

  slang::createGlobalSession(global_session.writeRef());

  session_desc = {};

  target_desc = {};

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

pipeline::PipelineManager::~PipelineManager() {

  vkDeviceWaitIdle(device);

  for (auto &p : pipelines) {
    vkDestroyPipeline(device, p.second.pipeline, nullptr);
    vkDestroyPipelineLayout(device, p.second.layout, nullptr);
  }
}

void pipeline::PipelineData::getDefault(pipeline::PipelineData &data) {

  VkPushConstantRange push_constant_range = {};
  push_constant_range.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
  push_constant_range.size = 128;
  push_constant_range.offset = 0;

  data.push_constant_range = push_constant_range;

  data.vertex_desc = vertex::getVertexDesc();

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

  data.logic_op_enable = VK_FALSE;
  data.logic_op = VK_LOGIC_OP_CLEAR;

  data.color_formats = {VK_FORMAT_B8G8R8A8_SRGB};

  data.depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
  data.stencil_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
}

uint64_t pipeline::PipelineManager::createRenderPipeline(
    pipeline::PipelineData &pipeline_data, const std::string &shader_name,
    bool has_pixel_entry, uint64_t pipeline_index) {

  static uint64_t next_id = 0;
  const uint64_t id = pipeline_index == UINT64_MAX ? next_id++ : pipeline_index;

  pipeline::RenderPipeline pipeline = {};

  std::array<std::optional<VkShaderModule>, 2> modules = {};

  modules[0] = createShaderModule(shader_name, "vertexMain");

  if (modules[0].has_value() == false) {
    return UINT64_MAX;
  }

  if (has_pixel_entry) {
    modules[1] = createShaderModule(shader_name, "pixelMain");

    if (modules[1].has_value() == false) {
      return UINT64_MAX;
    }
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stage_create_infos = {};
  shader_stage_create_infos[0].sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_infos[0].pName = "main";
  shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stage_create_infos[0].module = modules[0].value();

  if (has_pixel_entry) {
    shader_stage_create_infos[1].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[1].pName = "main";
    shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_create_infos[1].module = modules[1].value();
  }

  VkPipelineLayoutCreateInfo layout_create_info = {};
  layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_create_info.pushConstantRangeCount = 1;
  layout_create_info.pPushConstantRanges = &pipeline_data.push_constant_range;
  layout_create_info.setLayoutCount = 0; // Add bindless descriptor sets later

  VK_ERROR(vkCreatePipelineLayout(device, &layout_create_info, nullptr,
                                  &pipeline.layout));

  VkPipelineVertexInputStateCreateInfo vertex_create_info = {};
  vertex_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_create_info.vertexBindingDescriptionCount =
      pipeline_data.vertex_desc.binding_descs.size();
  vertex_create_info.pVertexBindingDescriptions =
      pipeline_data.vertex_desc.binding_descs.data();
  vertex_create_info.vertexAttributeDescriptionCount =
      pipeline_data.vertex_desc.attribute_descs.size();
  vertex_create_info.pVertexAttributeDescriptions =
      pipeline_data.vertex_desc.attribute_descs.data();

  VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
  dynamic_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_create_info.dynamicStateCount = pipeline_data.dynamic_states.size();
  dynamic_create_info.pDynamicStates = pipeline_data.dynamic_states.data();

  VkPipelineRenderingCreateInfoKHR rendering_create_info = {};
  rendering_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  rendering_create_info.colorAttachmentCount =
      pipeline_data.color_formats.size();
  rendering_create_info.pColorAttachmentFormats =
      pipeline_data.color_formats.data();
  rendering_create_info.depthAttachmentFormat = pipeline_data.depth_format;
  rendering_create_info.stencilAttachmentFormat = pipeline_data.stencil_format;
  rendering_create_info.viewMask = 0;

  VkPipelineColorBlendStateCreateInfo blend_state_create_info = {};
  blend_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend_state_create_info.logicOpEnable = pipeline_data.logic_op_enable;
  blend_state_create_info.logicOp = pipeline_data.logic_op;
  blend_state_create_info.attachmentCount =
      pipeline_data.blend_attachment_states.size();
  blend_state_create_info.pAttachments =
      pipeline_data.blend_attachment_states.data();

  VkGraphicsPipelineCreateInfo pipeline_create_info = {};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_create_info.layout = pipeline.layout;
  pipeline_create_info.pColorBlendState = &blend_state_create_info;
  pipeline_create_info.pDepthStencilState =
      &pipeline_data.depth_stencil_create_info;
  pipeline_create_info.pDynamicState = &dynamic_create_info;
  pipeline_create_info.pInputAssemblyState =
      &pipeline_data.assembly_create_info;
  pipeline_create_info.pMultisampleState =
      &pipeline_data.multisample_create_info;
  pipeline_create_info.pRasterizationState =
      &pipeline_data.rasterization_create_info;
  pipeline_create_info.pVertexInputState = &vertex_create_info;
  pipeline_create_info.pViewportState = &pipeline_data.viewport_create_info;
  pipeline_create_info.subpass = 0;
  pipeline_create_info.stageCount = has_pixel_entry ? 2 : 1;
  pipeline_create_info.pStages = shader_stage_create_infos.data();
  pipeline_create_info.renderPass = 0; // Dynamic Rendering
  pipeline_create_info.pNext = &rendering_create_info;

  // To do: Add Pipeline Caching
  VK_ERROR(vkCreateGraphicsPipelines(device, 0, 1, &pipeline_create_info,
                                     nullptr, &pipeline.pipeline));

  pipelines[id] = pipeline;
  name_to_pipeline[shader_name] = id;

  vkDestroyShaderModule(device, modules[0].value(), nullptr);

  if (has_pixel_entry)
    vkDestroyShaderModule(device, modules[1].value(), nullptr);

#ifndef PRODUCTION_BUILD

  std::time_t last_changed_time;
  std::string name;
  std::string path;

  for (size_t i = 0; i < session->getLoadedModuleCount(); i++) {

    if (session->getLoadedModule(i)->getName() == shader_name) {

      std::filesystem::file_time_type last_changed =
          std::filesystem::last_write_time(
              session->getLoadedModule(i)->getFilePath());

      auto changed = decltype(last_changed)::clock::to_sys(last_changed);

      last_changed_time = std::chrono::system_clock::to_time_t(changed);

      name = session->getLoadedModule(i)->getName();
      path = session->getLoadedModule(i)->getFilePath();
    }
  }

  CachedPipelineForHotReload cached_pipeline = {};
  cached_pipeline.pipeline = pipeline;
  cached_pipeline.pipeline_data = pipeline_data;
  cached_pipeline.has_pixel_entry = has_pixel_entry;
  cached_pipeline.index = id;
  cached_pipeline.last_changed_time = std::to_string(last_changed_time);
  cached_pipeline.name = name;
  cached_pipeline.path = path;

  pipelines_for_reload[shader_name] = cached_pipeline;

#endif

  return id;
}

std::optional<VkShaderModule> pipeline::PipelineManager::createShaderModule(
    const std::string &name, const std::string &entry_point_name) {

  Slang::ComPtr<slang::IModule> module;

  module = session->loadModule(name.c_str());

  if (!module) {
    std::cerr << "Could not create module\n";
    return {};
  }

  Slang::ComPtr<slang::IEntryPoint> entry_point;
  module->findEntryPointByName(entry_point_name.c_str(),
                               entry_point.writeRef());

  if (!entry_point) {
    std::cerr << "Could not write entry point\n";
    return {};
  }

  std::array<slang::IComponentType *, 2> component_types = {module,
                                                            entry_point};

  Slang::ComPtr<slang::IComponentType> composed_program;

  SlangResult compose_result = session->createCompositeComponentType(
      component_types.data(), component_types.size(),
      composed_program.writeRef());

  if (SLANG_FAILED(compose_result)) {
    std::cerr << "Could not compose program\n";
    return {};
  }

  Slang::ComPtr<slang::IComponentType> linked_program;

  SlangResult link_result = composed_program->link(linked_program.writeRef());

  if (SLANG_FAILED(link_result)) {
    std::cerr << "Could not link program\n";
    return {};
  }

  Slang::ComPtr<slang::IBlob> spirv_code;

  SlangResult spirv_result =
      linked_program->getTargetCode(0, spirv_code.writeRef());

  if (SLANG_FAILED(spirv_result)) {
    std::cerr << "Could not create Spir-V code\n";
    return {};
  }

  size_t spirv_size = spirv_code->getBufferSize();
  const uint32_t *spirv_data =
      reinterpret_cast<const uint32_t *>(spirv_code->getBufferPointer());

  VkShaderModuleCreateInfo create_info = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  create_info.codeSize = spirv_size;
  create_info.pCode = spirv_data;

  VkShaderModule shader_module;
  VK_ERROR(vkCreateShaderModule(device, &create_info, nullptr, &shader_module));

  return shader_module;
}

#ifndef PRODUCTION_BUILD

void pipeline::PipelineManager::reload()

{

  vkDeviceWaitIdle(device);

  std::vector<CachedPipelineForHotReload> changed_pipelines_to_reload = {};
  volatile size_t module_count = session->getLoadedModuleCount();
  changed_pipelines_to_reload.reserve(module_count);

  for (auto &v : pipelines_for_reload) {
    auto &p = v.second;

    std::filesystem::file_time_type last_changed =
        std::filesystem::last_write_time(p.path);

    auto changed = decltype(last_changed)::clock::to_sys(last_changed);

    std::time_t last_changed_time =
        std::chrono::system_clock::to_time_t(changed);

    if (std::to_string(last_changed_time) == p.last_changed_time)
      continue;

    std::cout << "Pipeline: " << p.name << " changed\n";

    changed_pipelines_to_reload.push_back(p);
  }

  // Recreate Session for forces recompilation
  this->session = nullptr;
  session_desc = {};
  target_desc = {};

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

  for (auto &p : changed_pipelines_to_reload) {
    uint64_t res = createRenderPipeline(p.pipeline_data, p.name,
                                        p.has_pixel_entry, p.index);

    if (res != UINT64_MAX) {
      vkDestroyPipelineLayout(device, p.pipeline.layout, nullptr);
      vkDestroyPipeline(device, p.pipeline.pipeline, nullptr);
    }
  }
}

#endif