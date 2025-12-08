#pragma once

#include "slang-com-helper.h"
#include "slang-com-ptr.h"
#include "slang.h"

#include "volk.h"
#include "vulkan/vulkan_core.h"
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "spirv_cross.hpp"

#ifndef PRODUCTION_BUILD

#endif

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> _shader_stages;

  VkPipelineInputAssemblyStateCreateInfo _input_assembly;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineColorBlendAttachmentState _color_blend_attachment;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineLayout _pipeline_layout;
  VkPipelineDepthStencilStateCreateInfo _depth_stencil;
  VkPipelineRenderingCreateInfo _render_info;
  VkFormat _color_attachment_format;

  PipelineBuilder() { clear(); }

  void clear();

  void setShaders(VkShaderModule vertex_shader, VkShaderModule pixel_shader);

  void setInputTopology(VkPrimitiveTopology topology);
  void setPolygonMode(VkPolygonMode mode);
  void setCullMode(VkCullModeFlagBits cull_mode, VkFrontFace front_Face);

  void setColorAttachmentFormat(VkFormat format);

  void setMultiSamplingNone();

  void disableBlending();

  void enableBlendingAdditive();

  void enableBlendingAlphaBlend();

  void setDepthFormat(VkFormat format);

  void disableDepthtest();

  VkPipeline buildPipeline(VkDevice device);
};

struct Pipeline {
  VkPipelineLayout layout;
  VkPipeline pipeline;
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

  std::array<VkVertexInputAttributeDescription, 5> vertex_attribute_desc;
  std::array<VkVertexInputBindingDescription, 1> vertex_binding_desc;

  std::vector<VkDynamicState> dynamic_states{};

  VkPipelineLayout layout;

  PipelineBuilder2() {}

  void makeGraphicsDefault();

  VkPipeline buildPipeline(VkDevice device);
};

class PipelineManager {
public:
  PipelineManager(const std::string path, VkDevice &device);
  ~PipelineManager();

  std::optional<VkShaderModule>
  createShaderModule(const std::string &name,
                     const std::string &entry_point_name,
                     std::vector<uint32_t> &out_spv_shader_data);

  size_t createGraphicsPipeline(
      PipelineBuilder2 pipeline_builder,
      std::array<std::string, 4> shader_modules_name_and_entry_point);

private:
  uint64_t generateDescriptorSetLayoutHashKey(
      const std::unordered_map<
          uint32_t, std::vector<VkDescriptorSetLayoutBinding>> &sets) const;

#ifndef PRODUCTION_BUILD

  Slang::ComPtr<slang::IGlobalSession> _global_session;
  Slang::ComPtr<slang::ISession> _session;

  bool slangToSpv(const std::string &name, const std::string &entry_point_name,
                  std::vector<uint32_t> &out_code);

#endif

  std::unordered_map<uint64_t, std::vector<VkDescriptorSetLayout>>
      _set_layouts{};

  bool runtime_compilation_possible;

  const std::string _shader_path;

  VkDevice &_device;
};