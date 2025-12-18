#pragma once

#include <array>
#include <vulkan/vulkan_core.h>

#include "volk.h"

namespace vertex {

struct Vertex {

  std::array<float, 3> position;
  uint32_t _pad0;

  std::array<float, 3> color;
  uint32_t _pad1;

  std::array<float, 3> normals;
  uint32_t _pad2;

  std::array<float, 4> tangent;

  std::array<float, 2> tex_coords_0;
  std::array<float, 2> tex_coords_1;
};

static_assert(sizeof(Vertex) % 16 == 0);

inline std::array<VkVertexInputAttributeDescription, 6>
getVertexAttributeDescription() {

  std::array<VkVertexInputAttributeDescription, 6> descs = {};

  descs[0].binding = 0;
  descs[0].location = 0;
  descs[0].offset = 0;
  descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;

  descs[1].binding = 0;
  descs[1].location = 1;
  descs[1].offset = 16;
  descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;

  descs[2].binding = 0;
  descs[2].location = 2;
  descs[2].offset = 32;
  descs[2].format = VK_FORMAT_R32G32B32_SFLOAT;

  descs[3].binding = 0;
  descs[3].location = 3;
  descs[3].offset = 48;
  descs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;

  descs[4].binding = 0;
  descs[4].location = 4;
  descs[4].offset = 64;
  descs[4].format = VK_FORMAT_R32G32_SFLOAT;

  descs[5].binding = 0;
  descs[5].location = 5;
  descs[5].offset = 72;
  descs[5].format = VK_FORMAT_R32G32_SFLOAT;

  return descs;
};

inline std::array<VkVertexInputBindingDescription, 1>
getVertexBindingDescription() {

  VkVertexInputBindingDescription desc = {};
  desc.binding = 0;
  desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  desc.stride = sizeof(Vertex);

  return {desc};
}

} // namespace vertex