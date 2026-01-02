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
  Mat4<float> inv_view_matrix;
};

struct RenderCamera {
  BufferIdAndOffset camera_data{};

  Mat4<float> view_matrix {}; //For Cpu Clustering
};

struct RenderMesh {
  BufferIdAndOffset vertex{};
  BufferIdAndOffset transform{};

  uint32_t index_count;
  uint32_t index_offset;

  BufferIdAndOffset material{};

  std::vector<uint32_t> images{}; //Indices for bound material textures

  uint32_t object_id;

  uint32_t pipeline_id;

  Vec3<float> world_pos;


};

enum class GpuLightType : uint32_t {
  Directional = 0,
  Spot = 1,
  Point = 2,

};

struct GpuLightData {

  std::array<float, 3> color;
  int32_t _pad;

  float range;

  float intensity;
  GpuLightType type;
  
  
  float inner_cone_angle;
  float outer_cone_angle;
  int32_t _pad0[3];
};

static_assert(sizeof(GpuLightData) % 16 == 0);

struct RenderLight {

  BufferIdAndOffset transform{};
  BufferIdAndOffset light{};

  Vec3<float> position_world_space;
  Quat<float> rotation_world_space;
  float radius;
  float angle;
  GpuLightType light_type;
};

struct RenderModelMatrix {

  Mat4<float> model_matrix;
  Mat4<float> normal_matrix; // inverse and transposed model for TBN
};

static_assert(sizeof(RenderModelMatrix) % 16 == 0);

inline RenderModelMatrix createRenderModelMatrix(Mat4<float> &model) {

  RenderModelMatrix r;
  r.model_matrix = model;
  r.normal_matrix = model.inverse().transpose();

  return r;
}

struct RenderMaterial {
  alignas(16) std::array<float, 4> albedo;
  alignas(16) std::array<float, 3> emissive;
  float metallic;
  float roughness;
  alignas(16) std::array<uint32_t, 8> use_textures;
};

static_assert(sizeof(RenderMaterial) % 16 == 0);

// Level is known per index in array
struct MipMapData {
  size_t size;
  size_t offset;
};
