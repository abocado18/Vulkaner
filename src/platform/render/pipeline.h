#pragma once

#include "volk.h"
#include "vulkan/vulkan_core.h"
#include <span>
#include <vector>

#ifndef PRODUCTION_BUILD

#ifdef WIN32
#include "dxc/WinAdapter.h"

#else

#define BOOL VulkanDXCBool

#endif

#include "dxc/dxcapi.h"

#ifndef WIN32

#undef BOOL

#endif

#endif

struct DescriptorSetLayoutBuilder {
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  void addBinding(uint32_t binding, VkDescriptorType type);
  void clear();

  VkDescriptorSetLayout
  build(VkDevice device, VkShaderStageFlags shader_stages,
        VkDescriptorSetLayoutCreateFlags create_flags = 0);
};

struct DescriptorAllocator {

  struct PoolSizeRatio {
    VkDescriptorType type;
    float ratio;
  };

  VkDescriptorPool pool;

  void initPool(VkDevice device, uint32_t max_sets,
                std::span<PoolSizeRatio> pool_ratios);

  void clearDescriptors(VkDevice device);

  void destroyPool(VkDevice device);

  VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

class PipelineManager {
public:
  PipelineManager();
  ~PipelineManager();

private:
#ifndef PRODUCTION_BUILD

  CComPtr<IDxcLibrary> library;
  CComPtr<IDxcCompiler3> compiler;
  CComPtr<IDxcUtils> utils;

#ifdef _WIN32

  HMODULE dxcLibHandle;

#else
  void *dxcLibHandle;

#endif

#endif

      bool runtime_compilation_possible;
};