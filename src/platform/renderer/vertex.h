#pragma once

#include "volk.h"

#include "gpu_structs.h"
#include <vector>

namespace vertex {

struct alignas(16) Vertex {

  Vertex() = default;


  Vector3 position;

  Vector3 normal;

  Vector3 tangent;

  Vector3 bitangent;

  IVector4 bone_ids;

  Vector4 bone_weights;

  Vector2 tex_coords;

  int32_t _padding[2];
};

static_assert(sizeof(Vertex) % 16 == 0);

struct Index
{
  uint32_t value;
};

static_assert(sizeof(Index) == sizeof(uint32_t));


struct VertexDesc {

  std::vector<VkVertexInputAttributeDescription> attribute_descs;

  std::vector<VkVertexInputBindingDescription> binding_descs;
};

inline VertexDesc getVertexDesc() {
  VertexDesc vertex_desc = {};

  vertex_desc.attribute_descs.resize(7);
  vertex_desc.binding_descs.resize(1);

  vertex_desc.binding_descs[0].binding = 0;
  vertex_desc.binding_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  vertex_desc.binding_descs[0].stride = sizeof(Vertex);

  vertex_desc.attribute_descs[0].binding = 0;
  vertex_desc.attribute_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_desc.attribute_descs[0].offset = 0;
  vertex_desc.attribute_descs[0].location = 0;

  vertex_desc.attribute_descs[1].binding = 0;
  vertex_desc.attribute_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_desc.attribute_descs[1].offset = 16;
  vertex_desc.attribute_descs[1].location = 1;

  vertex_desc.attribute_descs[2].binding = 0;
  vertex_desc.attribute_descs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_desc.attribute_descs[2].offset = 32;
  vertex_desc.attribute_descs[2].location = 2;

  vertex_desc.attribute_descs[3].binding = 0;
  vertex_desc.attribute_descs[3].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_desc.attribute_descs[3].offset = 48;
  vertex_desc.attribute_descs[3].location = 3;

  vertex_desc.attribute_descs[4].binding = 0;
  vertex_desc.attribute_descs[4].format = VK_FORMAT_R32G32B32A32_SINT;
  vertex_desc.attribute_descs[4].offset = 64;
  vertex_desc.attribute_descs[4].location = 4;

  vertex_desc.attribute_descs[5].binding = 0;
  vertex_desc.attribute_descs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  vertex_desc.attribute_descs[5].offset = 80;
  vertex_desc.attribute_descs[5].location = 5;

  vertex_desc.attribute_descs[6].binding = 0;
  vertex_desc.attribute_descs[6].format = VK_FORMAT_R32G32_SFLOAT;
  vertex_desc.attribute_descs[6].offset = 96;
  vertex_desc.attribute_descs[6].location = 6;


  return vertex_desc;
}

} // namespace vertex