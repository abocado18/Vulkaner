#pragma once
#include <cstddef>
#include <cstdint>

struct RenderObject {
  uint32_t index_count;
  uint32_t first_index;
  uint32_t first_vertex;

  size_t transform_index;
};
