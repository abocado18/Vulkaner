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

void DescriptorSetLayoutBuilder::addBinding(uint32_t binding,
                                            VkDescriptorType type) {
  VkDescriptorSetLayoutBinding new_binding = {};
  new_binding.binding = binding;
  new_binding.descriptorCount = 1;
  new_binding.descriptorType = type;

  bindings.push_back(new_binding);
}

void DescriptorSetLayoutBuilder::clear() { bindings.clear(); }

VkDescriptorSetLayout DescriptorSetLayoutBuilder::build(
    VkDevice device, VkShaderStageFlags shader_stages,
    VkDescriptorSetLayoutCreateFlags create_flags) {

  for (auto &b : bindings) {
    b.stageFlags |= shader_stages;
  }

  VkDescriptorSetLayoutCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  create_info.bindingCount = bindings.size();
  create_info.pBindings = bindings.data();
  create_info.flags = create_flags;

  VkDescriptorSetLayout layout;
  VK_CHECK(vkCreateDescriptorSetLayout(device, &create_info, nullptr, &layout),
           "Create Descriptor Set Layout\n");

  return layout;
}

void DescriptorAllocator::initPool(VkDevice device, uint32_t max_sets,
                                   std::span<PoolSizeRatio> pool_ratios) {

  std::vector<VkDescriptorPoolSize> pool_sizes = {};

  for (PoolSizeRatio &ratio : pool_ratios) {
    pool_sizes.push_back(VkDescriptorPoolSize{
        ratio.type, static_cast<uint>(ratio.ratio * max_sets)});
  }

  VkDescriptorPoolCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  create_info.flags = 0;
  create_info.maxSets = max_sets;
  create_info.poolSizeCount = pool_sizes.size();
  create_info.pPoolSizes = pool_sizes.data();

  VK_CHECK(vkCreateDescriptorPool(device, &create_info, nullptr, &pool),
           "Create Descriptor Pool");
}

void DescriptorAllocator::clearDescriptors(VkDevice device) {

  VK_CHECK(vkResetDescriptorPool(device, pool, 0), "Reset Descriptor Pool");
}

void DescriptorAllocator::destroyPool(VkDevice device) {
  vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device,
                                              VkDescriptorSetLayout layout) {
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet set;
  VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &set),
           "Create Descriptor Set");

  return set;
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

VkDescriptorPool DescriptorAllocatorGrowable::getPool(VkDevice device) {

  VkDescriptorPool new_pool;

  if (ready_pools.size() != 0) {

    new_pool = ready_pools.back();
    ready_pools.pop_back();
  } else {

    new_pool = createPool(device, sets_per_pool, ratios);

    sets_per_pool = sets_per_pool * 1.5f;

    if (sets_per_pool > 4092) {

      sets_per_pool = 4092;
    }
  }

  return new_pool;
}

VkDescriptorPool
DescriptorAllocatorGrowable::createPool(VkDevice device, uint32_t set_count,
                                        std::span<PoolSizeRatio> pool_ratios) {

  std::vector<VkDescriptorPoolSize> pool_sizes;

  pool_sizes.reserve(pool_ratios.size());

  for (PoolSizeRatio ratio : pool_ratios) {

    pool_sizes.push_back(VkDescriptorPoolSize{
        .type = ratio.type,
        .descriptorCount = static_cast<uint32_t>(ratio.ratio * set_count)});
  }

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = 0;
  pool_info.maxSets = set_count;
  pool_info.poolSizeCount = pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();

  VkDescriptorPool new_pool;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &new_pool);

  return new_pool;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t max_sets,
                                       std::span<PoolSizeRatio> pool_ratios) {
  ratios.clear();

  for (auto r : pool_ratios) {
    ratios.push_back(r);
  }

  VkDescriptorPool new_pool = createPool(device, max_sets, pool_ratios);

  sets_per_pool = max_sets * 1.5f;

  ready_pools.push_back(new_pool);
}

void DescriptorAllocatorGrowable::clearPools(VkDevice device) {
  for (auto p : ready_pools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }

  for (auto p : full_pools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }

  full_pools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(
    VkDevice device, VkDescriptorSetLayout layout, void *p_next) {
  VkDescriptorPool pool_to_use = getPool(device);

  VkDescriptorSetAllocateInfo allocate_info = {};
  allocate_info.pNext = p_next;
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = pool_to_use;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &layout;

  VkDescriptorSet ds;

  VkResult result = vkAllocateDescriptorSets(device, &allocate_info, &ds);

  if (result == VK_ERROR_OUT_OF_POOL_MEMORY ||
      result == VK_ERROR_FRAGMENTED_POOL) {

    full_pools.push_back(pool_to_use);

    pool_to_use = getPool(device);

    VK_ERROR(vkAllocateDescriptorSets(device, &allocate_info, &ds),
             "Allocate Descriptor Set");
  }

  ready_pools.push_back(pool_to_use);

  return ds;
}

void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size,
                                   size_t offset, VkDescriptorType type) {

  VkDescriptorBufferInfo &info =
      buffer_infos.emplace_back(VkDescriptorBufferInfo{
          .buffer = buffer, .offset = offset, .range = size});

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = &info;

  writes.push_back(write);
}

void DescriptorWriter::writeImage(int binding, VkImageView image,
                                  VkSampler sampler, VkImageLayout layout,
                                  VkDescriptorType type) {

  VkDescriptorImageInfo &info = image_infos.emplace_back(VkDescriptorImageInfo{
      .sampler = sampler, .imageView = image, .imageLayout = layout});

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorType = type;
  write.pImageInfo = &info;
  write.descriptorCount = 1;

  writes.push_back(write);
}

void DescriptorWriter::clear() {

  image_infos.clear();
  writes.clear();
  buffer_infos.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set) {
  for (auto &write : writes) {
    write.dstSet = set;
  }

  vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}
