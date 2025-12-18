#include "pipeline.h"
#include "platform/render/vertex.h"
#include "platform/render/vk_utils.h"
#include "platform/render/vulkan_macros.h"
#include "spirv.hpp"
#include "spirv_common.hpp"
#include "spirv_cross.hpp"
#include "spirv_cross_containers.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

#include <algorithm>

#ifndef PRODUCTION_BUILD

#include "slang-com-ptr.h"
#include "slang.h"

void diagnoseIfNeeded(slang::IBlob *diagnosticsBlob) {
  if (diagnosticsBlob != nullptr) {
    std::cout << (const char *)diagnosticsBlob->getBufferPointer() << std::endl;
  }
}

#endif

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

PipelineManager::~PipelineManager() {

  for (auto &p : _pipelines) {

    vkDestroyPipeline(_device, p.pipeline, nullptr);
    vkDestroyPipelineLayout(_device, p.layout, nullptr);

    for (auto &l : p.set_layouts) {
      vkDestroyDescriptorSetLayout(_device, l, nullptr);
    }
  }
}

std::optional<VkShaderModule> PipelineManager::createShaderModule(
    const std::string &name, const std::string &entry_point_name,
    std::vector<uint32_t> &out_spv_shader_data) {

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

#ifndef PRODUCTION_BUILD

  if (!isSpv) {
    if (!slangToSpv(name, entry_point_name, out_spv_shader_data)) {
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

    out_spv_shader_data.resize(file_size / sizeof(uint32_t));

    file.seekg(0);

    file.read((char *)out_spv_shader_data.data(), file_size);

    file.close();
  }

  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.pNext = nullptr;

  create_info.codeSize = out_spv_shader_data.size() * sizeof(uint32_t);
  create_info.pCode = out_spv_shader_data.data();

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

void PipelineBuilder2::makeGraphicsDefault() {

  PipelineBuilder2 &builder = *this;

  dynamic_states = {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT,
  };

  builder.dynamic_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  builder.dynamic_info.dynamicStateCount = dynamic_states.size();
  builder.dynamic_info.pDynamicStates = dynamic_states.data();

  vertex_attribute_desc = vertex::getVertexAttributeDescription();
  vertex_binding_desc = vertex::getVertexBindingDescription();

  builder.vertex_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  builder.vertex_info.vertexAttributeDescriptionCount =
      vertex_attribute_desc.size();
  builder.vertex_info.pVertexAttributeDescriptions =
      vertex_attribute_desc.data();

  builder.vertex_info.vertexBindingDescriptionCount =
      vertex_binding_desc.size();
  builder.vertex_info.pVertexBindingDescriptions = vertex_binding_desc.data();

  builder.assembly_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  builder.assembly_info.primitiveRestartEnable = VK_FALSE;
  builder.assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  builder.viewport_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  builder.viewport_info.viewportCount = 1;
  builder.viewport_info.scissorCount = 1;

  builder.rasterization_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  builder.rasterization_info.depthClampEnable = VK_FALSE;
  builder.rasterization_info.rasterizerDiscardEnable = VK_FALSE;
  builder.rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
  builder.rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
  builder.rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  builder.rasterization_info.depthBiasEnable = VK_FALSE;
  builder.rasterization_info.depthBiasSlopeFactor = 1.0f;
  builder.rasterization_info.lineWidth = 1.0f;

  builder.multisample_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  builder.multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  builder.multisample_info.sampleShadingEnable = VK_FALSE;

  builder.color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  builder.color_blend_attachment.blendEnable = VK_TRUE;
  builder.color_blend_attachment.srcColorBlendFactor =
      VK_BLEND_FACTOR_SRC_ALPHA;
  builder.color_blend_attachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  builder.color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  builder.color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  builder.color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  builder.color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

  builder.color_blend_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  builder.color_blend_info.logicOpEnable = VK_FALSE;
  builder.color_blend_info.logicOp = VK_LOGIC_OP_COPY;
  builder.color_blend_info.attachmentCount = 1;
  builder.color_blend_info.pAttachments = &builder.color_blend_attachment;

  builder.color_rendering_formats = {VK_FORMAT_R16G16B16A16_SFLOAT};

  builder.rendering_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  builder.rendering_info.colorAttachmentCount =
      builder.color_rendering_formats.size();
  builder.rendering_info.viewMask = 0;
  builder.rendering_info.pColorAttachmentFormats =
      builder.color_rendering_formats.data();
  builder.rendering_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
  builder.rendering_info.stencilAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

  builder.depth_stencil_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  builder.depth_stencil_info.depthWriteEnable = VK_TRUE;
  builder.depth_stencil_info.depthTestEnable = VK_TRUE;
  builder.depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
  builder.depth_stencil_info.maxDepthBounds = 1.0f;
  builder.depth_stencil_info.minDepthBounds = 0.0f;
  builder.depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  builder.depth_stencil_info.stencilTestEnable = VK_FALSE;
}

size_t PipelineManager::createGraphicsPipeline(
    PipelineBuilder2 pipeline_builder,
    std::array<std::string, 4> shader_modules_name_and_entry_point) {

  std::vector<uint32_t> vertex_spv_data{};
  std::vector<uint32_t> frag_spv_data{};

  const std::string &vertex_shader_name =
      shader_modules_name_and_entry_point[0];
  const std::string &vertex_shader_entry =
      shader_modules_name_and_entry_point[1];
  const std::string &frag_shader_name = shader_modules_name_and_entry_point[2];
  const std::string &frag_shader_entry = shader_modules_name_and_entry_point[3];

  auto vertex_module_res = createShaderModule(
      vertex_shader_name, vertex_shader_entry, vertex_spv_data);

  auto frag_module_res =
      createShaderModule(frag_shader_name, frag_shader_entry, frag_spv_data);

  if (!vertex_module_res.has_value() || !frag_module_res.has_value()) {
    return SIZE_MAX;
  }

  VkShaderModule vertex_module = vertex_module_res.value();
  VkShaderModule frag_module = frag_module_res.value();

  // Use for Input Data Reflection
  spirv_cross::Compiler vertex_comp(std::move(vertex_spv_data));
  spirv_cross::Compiler frag_comp(std::move(frag_spv_data));

  auto vertex_active = vertex_comp.get_active_interface_variables();
  auto frag_active = frag_comp.get_active_interface_variables();

  vertex_comp.set_enabled_interface_variables(vertex_active);
  frag_comp.set_enabled_interface_variables(frag_active);

  spirv_cross::ShaderResources vertex_res =
      vertex_comp.get_shader_resources(vertex_active);
  spirv_cross::ShaderResources frag_res =
      frag_comp.get_shader_resources(frag_active);

  VkPushConstantRange range{};
  range.offset = 0;
  range.size = 128;
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  std::map<uint32_t, std::map<uint32_t, VkDescriptorSetLayoutBinding>>
      descriptor_set_bindings{};

  getDescriptorSetLayoutBindingsFromCross(vertex_comp, vertex_res,
                                          VK_SHADER_STAGE_VERTEX_BIT,
                                          descriptor_set_bindings);

  getDescriptorSetLayoutBindingsFromCross(frag_comp, frag_res,
                                          VK_SHADER_STAGE_FRAGMENT_BIT,
                                          descriptor_set_bindings);

  std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      final_set_layout_bindings;

  for (auto &set : descriptor_set_bindings) {
    auto &vec = final_set_layout_bindings[set.first];

    for (auto &binding : set.second) {

      vec.push_back(binding.second);
    }
  }

  uint64_t key = generateDescriptorSetLayoutHashKey(final_set_layout_bindings);

  auto it = _set_layouts.find(key);

  std::vector<VkDescriptorSetLayout> layouts{};

  if (it == _set_layouts.end()) {

    uint32_t max_set_index = 0;
    for (auto &pair : final_set_layout_bindings) {
      max_set_index = std::max(max_set_index, pair.first);
    }

    layouts.resize(max_set_index + 1, VK_NULL_HANDLE);

    for (auto &pair : final_set_layout_bindings) {
      uint32_t setIndex = pair.first;
      std::vector<VkDescriptorSetLayoutBinding> &set_bindings = pair.second;

      std::sort(set_bindings.begin(), set_bindings.end(),
                [](const VkDescriptorSetLayoutBinding &a,
                   const VkDescriptorSetLayoutBinding &b) {
                  return a.binding < b.binding;
                });

      VkDescriptorSetLayoutCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      create_info.bindingCount = static_cast<uint32_t>(set_bindings.size());
      create_info.pBindings = set_bindings.data();

      VkDescriptorSetLayout layout;
      VK_ERROR(
          vkCreateDescriptorSetLayout(_device, &create_info, nullptr, &layout),
          "Create Set Layout");

      layouts[setIndex] = layout;
    }

    for (auto &l : layouts) {
      if (l == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutCreateInfo empty_info{};
        empty_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        empty_info.bindingCount = 0;
        empty_info.pBindings = nullptr;
        VK_ERROR(vkCreateDescriptorSetLayout(_device, &empty_info, nullptr, &l),
                 "Create empty Set Layout");
      }
    }

    _set_layouts.insert_or_assign(key, layouts);

  } else {

    layouts = it->second;
  }

  Pipeline new_pipeline{};

  new_pipeline.set_layouts = layouts;

  VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
  pipeline_layout_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.setLayoutCount = layouts.size();
  pipeline_layout_create_info.pSetLayouts = layouts.data();
  pipeline_layout_create_info.pushConstantRangeCount = 1;
  pipeline_layout_create_info.pPushConstantRanges = &range;

  VK_ERROR(vkCreatePipelineLayout(_device, &pipeline_layout_create_info,
                                  nullptr, &new_pipeline.layout),
           "Create Pipeline Layout");

  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
       VK_SHADER_STAGE_VERTEX_BIT, vertex_module, "main"},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
       VK_SHADER_STAGE_FRAGMENT_BIT, frag_module, "main"},
  };

  VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};
  graphics_pipeline_create_info.sType =
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphics_pipeline_create_info.pVertexInputState =
      &pipeline_builder.vertex_info;
  graphics_pipeline_create_info.pInputAssemblyState =
      &pipeline_builder.assembly_info;
  graphics_pipeline_create_info.pViewportState =
      &pipeline_builder.viewport_info;
  graphics_pipeline_create_info.pRasterizationState =
      &pipeline_builder.rasterization_info;
  graphics_pipeline_create_info.pMultisampleState =
      &pipeline_builder.multisample_info;
  graphics_pipeline_create_info.pDepthStencilState =
      &pipeline_builder.depth_stencil_info;
  graphics_pipeline_create_info.pColorBlendState =
      &pipeline_builder.color_blend_info;
  graphics_pipeline_create_info.pDynamicState = &pipeline_builder.dynamic_info;
  graphics_pipeline_create_info.pStages = stages;
  graphics_pipeline_create_info.stageCount = 2;
  graphics_pipeline_create_info.layout = new_pipeline.layout;
  graphics_pipeline_create_info.pNext = &pipeline_builder.rendering_info;

  VK_ERROR(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1,
                                     &graphics_pipeline_create_info, nullptr,
                                     &new_pipeline.pipeline),
           "Create Graphics Pipeline");

  _pipelines.push_back(new_pipeline);

  vkDestroyShaderModule(_device, vertex_module, nullptr);
  vkDestroyShaderModule(_device, frag_module, nullptr);

  return _pipelines.size() - 1;
}

uint64_t PipelineManager::generateDescriptorSetLayoutHashKey(
    const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> &sets)
    const {
  uint64_t hash = 0xcbf29ce484222325ULL;

  auto fnv1a_combine = [](uint64_t h, uint64_t v) -> uint64_t {
    return (h ^ v) * 0x100000001b3ULL;
  };

  for (auto &pair : sets) {
    uint32_t setNumber = pair.first;
    auto vec = pair.second;
    std::sort(vec.begin(), vec.end(),
              [](const VkDescriptorSetLayoutBinding &a,
                 const VkDescriptorSetLayoutBinding &b) {
                return a.binding < b.binding;
              });
    hash = fnv1a_combine(hash, setNumber);
    for (auto &b : vec) {
      uint64_t v = 0;
      v |= static_cast<uint64_t>(b.binding);
      v |= static_cast<uint64_t>(b.descriptorType) << 32;
      v |= static_cast<uint64_t>(b.descriptorCount) << 40;
      v |= static_cast<uint64_t>(b.stageFlags) << 48;
      hash = fnv1a_combine(hash, v);
    }
  }
  return hash;
}

void PipelineManager::getDescriptorSetLayoutBindingsFromCross(
    spirv_cross::Compiler &comp,
    spirv_cross::ShaderResources &available_resources,
    VkShaderStageFlags shader_stages,
    std::map<uint32_t, std::map<uint32_t, VkDescriptorSetLayoutBinding>>
        &out_binding_map) {

  auto addBinding = [&](uint32_t set, uint32_t binding, VkDescriptorType type,
                        VkShaderStageFlags stage, uint32_t descriptor_count) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = descriptor_count;
    b.stageFlags = stage;

    auto &exists = out_binding_map[set][binding];

    if (exists.descriptorType == 0 && exists.stageFlags == 0 &&
        exists.descriptorCount == 0) {
      exists = b;
    } else {
      exists.stageFlags |= stage;
      exists.descriptorCount =
          std::max(exists.descriptorCount, b.descriptorCount);
    }
  };

  auto addResourcesToMap =
      [&](spirv_cross::SmallVector<spirv_cross::Resource> &resources,
          VkDescriptorType descriptor_type) {
        for (const auto &r : resources) {

          uint32_t set =
              comp.get_decoration(r.id, spv::DecorationDescriptorSet);
          uint32_t binding = comp.get_decoration(r.id, spv::DecorationBinding);

          uint32_t descriptor_count = 1;

          const spirv_cross::SPIRType &type = comp.get_type(r.type_id);

          if (!type.array.empty()) {
            descriptor_count = type.array[0];
          }

          addBinding(set, binding, descriptor_type, shader_stages,
                     descriptor_count);
        }
      };

  addResourcesToMap(available_resources.uniform_buffers,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
  addResourcesToMap(available_resources.storage_buffers,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
  addResourcesToMap(available_resources.sampled_images,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  addResourcesToMap(available_resources.storage_images,
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
  addResourcesToMap(available_resources.separate_images,
                    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
  addResourcesToMap(available_resources.separate_samplers,
                    VK_DESCRIPTOR_TYPE_SAMPLER);
}
