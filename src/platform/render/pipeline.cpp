#include "pipeline.h"
#include "platform/render/vk_utils.h"
#include "platform/render/vulkan_macros.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <slang-com-helper.h>
#include <string>
#include <vector>

#include "slang-com-ptr.h"
#include "slang.h"

void diagnoseIfNeeded(slang::IBlob *diagnosticsBlob) {
  if (diagnosticsBlob != nullptr) {
    std::cout << (const char *)diagnosticsBlob->getBufferPointer() << std::endl;
  }
}



PipelineManager::PipelineManager(const std::string path, VkDevice &device)
    : _device(device), _shader_path(path) {

#ifndef PRODUCTION_BUILD

  slang::createGlobalSession(_global_session.writeRef());

  slang::SessionDesc session_desc = {};

  slang::TargetDesc target_desc = {};
  target_desc.format = SLANG_SPIRV;
  target_desc.profile = _global_session->findProfile("spirv_1_5");

  session_desc.targetCount = 1;
  session_desc.targets = &target_desc;

  std::array<const char *, 1> search_paths = {_shader_path.c_str()};

  session_desc.searchPathCount = search_paths.size();
  session_desc.searchPaths = search_paths.data();

  std::array<slang::CompilerOptionEntry, 1> options = {
      {slang::CompilerOptionName::EmitSpirvDirectly,
       {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}}};
  session_desc.compilerOptionEntries = options.data();
  session_desc.compilerOptionEntryCount = options.size();

  _global_session->createSession(session_desc, _session.writeRef());

#endif

  std::cout << "Pipeline Manager created\n";
}

PipelineManager::~PipelineManager() {}

std::optional<VkShaderModule>
PipelineManager::createShaderModule(const std::string &name,
                                    const std::string &entry_point_name) {

  const std::string total_file_path = _shader_path + "/" + name;

  bool isSpv = [&]() -> bool {
    std::ifstream file(total_file_path, std::ios::binary);
    if (!file)
      return false;

    uint32_t magic = 0;
    file.read(reinterpret_cast<char *>(&magic), sizeof(magic));

    if (file.gcount() != sizeof(magic))
      return false;

    return magic == 0x07230203;
  }();

  std::vector<uint32_t> spirv_data = {};

#ifndef PRODUCTION_BUILD

  if (!isSpv) {
    if (!slangToSpv(name, entry_point_name, spirv_data)) {
      return std::nullopt;
    }
  }

#else

  if (!isSpv)
    return std::nullopt;

#endif

  else {

    std::ifstream file(total_file_path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      return std::nullopt;
    }

    size_t file_size = (size_t)file.tellg();

    spirv_data.resize(file_size / sizeof(uint32_t));

    file.seekg(0);

    file.read((char *)spirv_data.data(), file_size);

    file.close();
  }

  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.pNext = nullptr;

  create_info.codeSize = spirv_data.size() * sizeof(uint32_t);
  create_info.pCode = spirv_data.data();

  VkShaderModule shader_module;

  if (vkCreateShaderModule(_device, &create_info, nullptr, &shader_module) !=
      VK_SUCCESS)
    return std::nullopt;

  return shader_module;
}

#ifndef PRODUCTION_BUILD

bool PipelineManager::slangToSpv(const std::string &name,
                                 const std::string &entry_point_name,
                                 std::vector<uint32_t> &out_code) {

  Slang::ComPtr<slang::IModule> slang_module;

  {
    Slang::ComPtr<slang::IBlob> diagnosticsBlob;
    const char *moduleName = "shortest";
    const char *modulePath = "shortest.slang";
    slang_module = _session->loadModule(name.c_str());

    diagnoseIfNeeded(diagnosticsBlob);
    if (!slang_module) {
      return false;
    }
  }

  Slang::ComPtr<slang::IEntryPoint> entry_point;

  {

    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    slang_module->findEntryPointByName(entry_point_name.c_str(),
                                       entry_point.writeRef());

    if (!entry_point) {
      std::cout << "Error getting entry point" << std::endl;
      return false;
    }
  }

  std::array<slang::IComponentType *, 2> component_types = {slang_module,
                                                            entry_point};

  Slang::ComPtr<slang::IComponentType> composed_program;
  {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;

    SlangResult r = _session->createCompositeComponentType(
        component_types.data(), component_types.size(),
        composed_program.writeRef(), diagnostics_blob.writeRef());

    SLANG_RETURN_FALSE_ON_FAIL(r);
  }

  Slang::ComPtr<slang::IComponentType> linked_program;
  {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;

    SlangResult r = composed_program->link(linked_program.writeRef(),
                                           diagnostics_blob.writeRef());

    diagnoseIfNeeded(diagnostics_blob);

    SLANG_RETURN_FALSE_ON_FAIL(r);
  }

  Slang::ComPtr<slang::IBlob> spirv_code;
  {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;

    SlangResult r = linked_program->getEntryPointCode(
        0, 0, spirv_code.writeRef(), diagnostics_blob.writeRef());

    diagnoseIfNeeded(diagnostics_blob);

    SLANG_RETURN_FALSE_ON_FAIL(r);
  }

  out_code.resize(spirv_code->getBufferSize() / sizeof(uint32_t));

  std::memcpy(out_code.data(), spirv_code->getBufferPointer(),
              spirv_code->getBufferSize());

  return true;
}

#endif

void PipelineBuilder::clear() {
  _input_assembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

  _rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

  _color_blend_attachment = {};

  _multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

  _pipeline_layout = {};

  _depth_stencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

  _render_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

  _shader_stages.clear();
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice device) {
  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

  viewport_state.scissorCount = 1;
  viewport_state.viewportCount = 1;

  VkPipelineColorBlendStateCreateInfo color_blending = {};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &_color_blend_attachment;

  VkPipelineVertexInputStateCreateInfo _vertex_input_info = {};
  _vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.pStages = _shader_stages.data();
  pipeline_info.stageCount = _shader_stages.size();
  pipeline_info.pVertexInputState = &_vertex_input_info;
  pipeline_info.pInputAssemblyState = &_input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &_rasterizer;
  pipeline_info.pMultisampleState = &_multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDepthStencilState = &_depth_stencil;
  pipeline_info.layout = _pipeline_layout;

  VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT,
                            VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic_info.pDynamicStates = state;
  dynamic_info.dynamicStateCount = 2;

  pipeline_info.pDynamicState = &dynamic_info;

  VkPipeline pipeline;

  VkResult create_res = vkCreateGraphicsPipelines(
      device, nullptr, 1, &pipeline_info, nullptr, &pipeline);

  if (create_res != VK_SUCCESS) {
    std::cerr << "Could not create graphics pipeline\n";
    return VK_NULL_HANDLE;
  }

  return pipeline;
}

void PipelineBuilder::setShaders(VkShaderModule vertex_shader,
                                 VkShaderModule pixel_shader) {
  _shader_stages.clear();
  _shader_stages.push_back(vk_utils::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_VERTEX_BIT, vertex_shader));

  _shader_stages.push_back(vk_utils::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT, pixel_shader));
}

void PipelineBuilder::setInputTopology(VkPrimitiveTopology topology) {

  _input_assembly.topology = topology;
  _input_assembly.primitiveRestartEnable = VK_FALSE;
};

void PipelineBuilder::setPolygonMode(VkPolygonMode mode) {
  _rasterizer.polygonMode = mode;
  _rasterizer.lineWidth = 1.0f;
}

void PipelineBuilder::setCullMode(VkCullModeFlagBits cull_mode,
                                  VkFrontFace front_face) {
  _rasterizer.cullMode = cull_mode;
  _rasterizer.frontFace = front_face;
}

void PipelineBuilder::setMultiSamplingNone() {
  _multisampling.sampleShadingEnable = VK_FALSE;
  _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  _multisampling.minSampleShading = 1.0f;
  _multisampling.pSampleMask = nullptr;
  _multisampling.alphaToCoverageEnable = VK_FALSE;
  _multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disableBlending() {

  _color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  _color_blend_attachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::setColorAttachmentFormat(VkFormat format) {
  _color_attachment_format = format;

  _render_info.colorAttachmentCount = 1;
  _render_info.pColorAttachmentFormats = &_color_attachment_format;
}

void PipelineBuilder::setDepthFormat(VkFormat format) {
  _render_info.depthAttachmentFormat = format;
}

void PipelineBuilder::disableDepthtest() {
  _depth_stencil.depthTestEnable = VK_FALSE;
  _depth_stencil.depthWriteEnable = VK_FALSE;
  _depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  _depth_stencil.depthBoundsTestEnable = VK_FALSE;
  _depth_stencil.stencilTestEnable = VK_FALSE;
  _depth_stencil.front = {};
  _depth_stencil.back = {};
  _depth_stencil.minDepthBounds = 0.f;
  _depth_stencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enableBlendingAdditive() {
  _color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _color_blend_attachment.blendEnable = VK_TRUE;
  _color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  _color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  _color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  _color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  _color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  _color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enableBlendingAlphaBlend() {
  _color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _color_blend_attachment.blendEnable = VK_TRUE;
  _color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  _color_blend_attachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  _color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  _color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  _color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  _color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}
