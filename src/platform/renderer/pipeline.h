#pragma once

#include "volk.h"
#include <string>
#include <unordered_map>
#include <vector>

#include "slang-com-helper.h"
#include "slang-com-ptr.h"
#include "slang.h"

namespace pipeline {

struct PipelineData {

  VkPushConstantRange push_constant_range;

  VkPipelineVertexInputStateCreateInfo vertex_create_info;
  VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
  VkPipelineMultisampleStateCreateInfo multisample_create_info;
  VkPipelineRasterizationStateCreateInfo rasterization_create_info;

  std::vector<VkDynamicState> dynamic_states;
  VkPipelineDynamicStateCreateInfo dynamic_create_info;

  VkPipelineViewportStateCreateInfo viewport_create_info;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;

  std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;
  VkPipelineColorBlendStateCreateInfo blend_state_create_info;

  VkPipelineRenderingCreateInfoKHR rendering_create_info;

  PipelineData getDefault();
};

struct RenderPipeline {
  VkPipelineLayout layout;
  VkPipeline pipeline;
};

class PipelineManager {
public:
  PipelineManager(VkDevice &device, const std::string &shader_path);
  ~PipelineManager() = default;

  uint64_t createRenderPipeline(PipelineData &pipeline_data,
                                const std::string &vertex_shader_path,
                                const std::string &pixel_shader_path = "");

  inline RenderPipeline getPipeline(uint64_t key) { return pipelines.at(key); }

private:
  VkDevice &device;

  Slang::ComPtr<slang::IGlobalSession> global_session;
  slang::SessionDesc session_desc;
  Slang::ComPtr<slang::ISession> session;

  std::unordered_map<uint64_t, RenderPipeline> pipelines = {};

  VkShaderModule createShaderModule(const std::string &path);
};

} // namespace pipeline
