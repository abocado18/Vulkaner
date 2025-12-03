#pragma once

#include <cstdint>

struct StandardMaterial {

  float albedo[4];
  float emissive[3];
  int32_t _padding;
  float metallic;
  float roughness;
  int32_t _padding0[2];
};

static_assert(sizeof(StandardMaterial) % 16 == 0);