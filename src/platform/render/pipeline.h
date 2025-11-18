#pragma once

#include "volk.h"
#include "vulkan/vulkan_core.h"
#include <span>
#include <vector>

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