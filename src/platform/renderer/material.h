#pragma once

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "volk.h"

#include "resource_handler.h"

namespace material {

struct TextureHandle {

  TextureHandle() = default;
  ~TextureHandle() = default;

  uint64_t value;
};

class MaterialManager {

public:
  MaterialManager() = default;
  ~MaterialManager() = default;

  template <typename T> void getNewTextureHandle() {}

private:
  std::unordered_map<TextureHandle, uint64_t> texture_reference_counter = {};
};

} // namespace material