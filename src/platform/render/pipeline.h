#pragma once

#include "slang-com-helper.h"
#include "slang-com-ptr.h"
#include "slang.h"

#include "volk.h"
#include "vulkan/vulkan_core.h"
#include <optional>
#include <span>
#include <string>
#include <vector>

#ifndef PRODUCTION_BUILD

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
  PipelineManager(const std::string path, VkDevice &device);
  ~PipelineManager();


   std::optional<VkShaderModule> createShaderModule(const std::string &name, const std::string &entry_point_name);

private:
#ifndef PRODUCTION_BUILD

  Slang::ComPtr<slang::IGlobalSession> _global_session;
  Slang::ComPtr<slang::ISession> _session;

  bool slangToSpv(const std::string &name, const std::string &entry_point_name,
                  std::vector<uint32_t> &out_code);

#endif

 

  bool runtime_compilation_possible;

  const std::string _shader_path;

  VkDevice &_device;
};