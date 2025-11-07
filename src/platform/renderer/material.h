#pragma once

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "volk.h"

#include "resource_handler.h"

namespace material {

struct BaseMaterial {
  virtual ~BaseMaterial() {};
};

struct MaterialHandle {
  resource_handler::ResourceHandle
      material_buffer; // Handle to correct Gpu Buffer

  uint64_t index = UINT64_MAX; // Index into the buffer

  std::unique_ptr<BaseMaterial> cpu_material_reference;
};

} // namespace material