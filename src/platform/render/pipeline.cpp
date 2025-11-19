#include "pipeline.h"
#include "platform/render/vulkan_macros.h"
#include "vulkan/vulkan_core.h"
#include <dlfcn.h>
#include <dxc/WinAdapter.h>
#include <dxc/dxcapi.h>
#include <fstream>
#include <sys/types.h>
#include <vector>

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

PipelineManager::PipelineManager() {

#ifndef PRODUCTION_BUILD
#ifdef _WIN32
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  dxcLibHandle = LoadLibrary(L"dxcompiler.dll");
  if (!dxcLibHandle) {
    std::cerr << "Failed to load dxcompiler.dll\n";
    abort();
    return;
  }

  auto DxcCreateInstance =
      (HRESULT(WINAPI *)(REFCLSID, REFIID, void **))GetProcAddress(
          dxcLibHandle, "DxcCreateInstance");
#else
  dxcLibHandle = dlopen("libdxcompiler.so", RTLD_LAZY | RTLD_LOCAL);
  if (!dxcLibHandle) {
    std::cerr << "Failed to load libdxcompiler.so\n";
    abort();
    return;
  }

  auto DxcCreateInstance = (HRESULT (*)(REFCLSID, REFIID, void **))dlsym(
      dxcLibHandle, "DxcCreateInstance");
#endif

  if (!DxcCreateInstance) {
    std::cerr << "Failed to get DxcCreateInstance function\n";
    return;
  }

  HRESULT res;
  res = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
  if (FAILED(res)) {
    std::cerr << "Could not initialize Dxc Library\n";
    return;
  }

  res = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
  if (FAILED(res)) {
    std::cerr << "Could not initialize Dxc Compiler\n";
    return;
  }

  res = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
  if (FAILED(res)) {
    std::cerr << "Could not initialize Dxc Utils\n";
    return;
  }

  std::cout << "Pipeline Manager created\n";
#endif
}

PipelineManager::~PipelineManager() {

#ifndef PRODUCTION_BUILD

#ifdef _WIN32
  if (dxcLibHandle) {
    FreeLibrary(dxcLibHandle);
    dxcLibHandle = nullptr;
  }
  CoUninitialize(); // Uninitialize COM
#else
  if (dxcLibHandle) {
    dlclose(dxcLibHandle);
    dxcLibHandle = nullptr;
  }
#endif

#endif
}

#ifndef PRODUCTION_BUILD

bool PipelineManager::hlslToSpv(const std::string &hlsl_path,
                                const std::string &out_spv) {

  HRESULT hres;

  std::string source;

  std::ifstream file(hlsl_path);

  source.assign((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());

  CComPtr<IDxcBlobEncoding> source_blob;

  hres = utils->CreateBlob(source.data(), source.size(), CP_UTF8, &source_blob);

  if (FAILED(hres)) {

    std::cout << "File " << hlsl_path << " not found\n";
    return false;
  }

  


}

#endif