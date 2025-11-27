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
#include <vector>

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

class PipelineManager {
public:
  PipelineManager(const std::string path, VkDevice &device);
  ~PipelineManager();

  std::optional<VkShaderModule>
  createShaderModule(const std::string &name,
                     const std::string &entry_point_name);

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