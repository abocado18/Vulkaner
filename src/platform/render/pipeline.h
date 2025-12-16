#pragma once

#include "slang-com-helper.h"
#include "slang-com-ptr.h"
#include "slang.h"

#include "volk.h"
#include "vulkan/vulkan_core.h"
#include <deque>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "spirv_cross.hpp"

#ifndef PRODUCTION_BUILD

#endif



struct Pipeline {
  VkPipelineLayout layout;
  VkPipeline pipeline;

  std::vector<VkDescriptorSetLayout> set_layouts;
};

class PipelineBuilder2 {
public:
  std::vector<VkPipelineShaderStageCreateInfo> _shader_stages;

  VkPipelineDynamicStateCreateInfo dynamic_info{};
  VkPipelineVertexInputStateCreateInfo vertex_info{};
  VkPipelineInputAssemblyStateCreateInfo assembly_info{};
  VkPipelineViewportStateCreateInfo viewport_info{};
  VkPipelineRasterizationStateCreateInfo rasterization_info{};
  VkPipelineMultisampleStateCreateInfo multisample_info{};
  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  VkPipelineColorBlendStateCreateInfo color_blend_info{};

  VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};

  std::array<VkVertexInputAttributeDescription, 5> vertex_attribute_desc;
  std::array<VkVertexInputBindingDescription, 1> vertex_binding_desc;

  std::vector<VkDynamicState> dynamic_states{};

  std::vector<VkFormat> color_rendering_formats{};

  VkPipelineRenderingCreateInfo rendering_info{};

  PipelineBuilder2() {}

  void makeGraphicsDefault();

  VkPipeline buildPipeline(VkDevice device);
};

class PipelineManager {
public:
  PipelineManager(const std::string path, VkDevice &device);
  ~PipelineManager();

  const Pipeline &getPipelineByIdx(size_t idx)
  {
    return _pipelines.at(idx);
  }

  std::optional<VkShaderModule>
  createShaderModule(const std::string &name,
                     const std::string &entry_point_name,
                     std::vector<uint32_t> &out_spv_shader_data);

  size_t createGraphicsPipeline(
      PipelineBuilder2 pipeline_builder,
      std::array<std::string, 4> shader_modules_name_and_entry_point);

private:
  uint64_t generateDescriptorSetLayoutHashKey(
      const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> &sets)
      const;

  void getDescriptorSetLayoutBindingsFromCross(
      spirv_cross::Compiler &comp,
      spirv_cross::ShaderResources &available_resources,
      VkShaderStageFlags shader_stages,
      std::map<uint32_t, std::map<uint32_t, VkDescriptorSetLayoutBinding>>
          &out_binding_map);

#ifndef PRODUCTION_BUILD

  Slang::ComPtr<slang::IGlobalSession> _global_session;
  Slang::ComPtr<slang::ISession> _session;

  bool slangToSpv(const std::string &name, const std::string &entry_point_name,
                  std::vector<uint32_t> &out_code);

#endif

  std::map<uint32_t, std::vector<VkDescriptorSetLayout>> _set_layouts{};

  bool runtime_compilation_possible;

  const std::string _shader_path;

  std::vector<Pipeline> _pipelines{};

  VkDevice &_device;
};