#include "pipeline.h"
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