#pragma once

#include "vertex.h"
#include "volk.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "slang-com-ptr.h"
#include "slang.h"

#include <optional>

namespace pipeline {

struct PipelineData {

  VkPushConstantRange push_constant_range;

  VkPipelineInputAssemblyStateCreateInfo assembly_create_info;
  VkPipelineMultisampleStateCreateInfo multisample_create_info;
  VkPipelineRasterizationStateCreateInfo rasterization_create_info;

  std::vector<VkDynamicState> dynamic_states;

  VkPipelineViewportStateCreateInfo viewport_create_info;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;

  std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;
  VkBool32 logic_op_enable = VK_FALSE;
  VkLogicOp logic_op;

  std::vector<VkFormat> color_formats;
  VkFormat depth_format;
  VkFormat stencil_format;

  vertex::VertexDesc vertex_desc;

  static void getDefault(PipelineData &data);
};

struct RenderPipeline {
  VkPipelineLayout layout;
  VkPipeline pipeline;
};

#ifndef PRODUCTION_BUILD

struct CachedPipelineForHotReload {

  RenderPipeline pipeline;
  std::string last_changed_time;
  std::string name;
  std::string path;
  PipelineData pipeline_data;
  bool has_pixel_entry;
  uint64_t index;
};

#endif

class PipelineManager {
public:
  PipelineManager(VkDevice &device, const std::string &shader_path);
  ~PipelineManager();

  uint64_t createRenderPipeline(PipelineData &pipeline_data,
                                const std::string &shader_name,
                                bool has_pixel_entry = true,
                                uint64_t pipeline_index = UINT64_MAX);

  inline RenderPipeline getPipeline(uint64_t key) { return pipelines.at(key); }

  inline RenderPipeline getPipelineByName(const std::string &name) {
    return pipelines.at(name_to_pipeline.at(name));
  }

#ifndef PRODUCTION_BUILD

  void reload();

  std::unordered_map<std::string, CachedPipelineForHotReload>
      pipelines_for_reload = {};

#endif

private:
  VkDevice &device;

  std::string shader_path;

  Slang::ComPtr<slang::IGlobalSession> global_session;
  slang::SessionDesc session_desc;
  slang::TargetDesc target_desc;
  Slang::ComPtr<slang::ISession> session;

  std::unordered_map<uint64_t, RenderPipeline> pipelines = {};
  std::unordered_map<std::string, uint64_t> name_to_pipeline = {};

  std::optional<VkShaderModule>
  createShaderModule(const std::string &name,
                     const std::string &entry_point_name);
};

} // namespace pipeline
