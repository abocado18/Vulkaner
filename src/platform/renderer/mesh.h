#pragma once

#include "volk.h"
#include <cassert>
#include <cinttypes>
#include <cstdint>

namespace mesh {

    //Gets send to gpu later as push constant
struct Keys {
  uint64_t material_pointer;
  uint64_t material_index;
  uint64_t vertex_index;
  uint64_t vertex_offset;
  uint64_t indices_index;
  uint64_t indices_offset;
};

static_assert(sizeof(Keys) < 128);

struct Mesh
{
  Keys keys;
  uint32_t number_of_indices;
  uint64_t pipeline_key;
};

} // namespace mesh