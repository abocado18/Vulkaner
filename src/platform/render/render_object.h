#pragma once
#include "platform/math/math.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

struct BufferIdAndOffset {
  uint32_t id;
  uint32_t offset;
};

struct GpuCameraData {
  Mat4<float> proj_matrix;
  Mat4<float> view_matrix;
};

struct RenderCamera {
  BufferIdAndOffset camera_data {};
};

struct RenderMesh {
  BufferIdAndOffset vertex{};
  BufferIdAndOffset transform{};

  uint32_t index_count;
  uint32_t index_offset;

  BufferIdAndOffset material{};

  uint32_t pipeline_id;
};

struct GpuLightData {

  Vec3<float> color;
  float range;
  float intensity;
  uint32_t type;
  uint32_t _pad[2];
};

static_assert(sizeof(GpuLightData) % 16 == 0);

struct RenderLight {

  BufferIdAndOffset transform{};
  BufferIdAndOffset light{};
};



