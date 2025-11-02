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

} // namespace mesh