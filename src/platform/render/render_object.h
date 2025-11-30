#pragma once
#include <cstddef>
#include <cstdint>

struct RenderObject {

  size_t vertex_buffer_id;
  uint32_t vertex_offset;

  size_t transform_buffer_id;
  uint32_t transform_offset;

  uint32_t instance_count;

  uint32_t index_count;
  uint32_t index_offset;

  size_t material_buffer_id;
  uint32_t material_offset;

  size_t pipeline_id;
};
