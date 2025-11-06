#pragma once

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "volk.h"

#include "resource_handler.h"

namespace material {


struct TextureRef
{

};


using TextureHandle = std::shared_ptr<uint64_t>;

class MaterialManager {

public:
  MaterialManager() = default;
  ~MaterialManager() = default;

  template <typename T> void getNewTextureHandle() {}

private:
};

} // namespace material