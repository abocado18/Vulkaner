#pragma once

#include "game/game.h"
#include "game/plugin.h"
#include "game/plugins/asset_plugin.h"
#include "game/plugins/scene_plugin.h"
#include "nlohmann/json.hpp"
#include "platform/math/math.h"
#include "platform/render/render_object.h"
#include "platform/render/resources.h"
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>


struct AssetImage {


  uint32_t width;
  uint32_t height;

  uint32_t channels;



  ResourceHandle image_handle {};

};

struct AssetMaterial {
  struct Params {
    std::array<float, 4> albedo;
    float metallic;
    float roughness;
    std::array<float, 3> emissive;
    float _pad[3];
  };

  static_assert(sizeof(Params) % 16 == 0);

  Params material_parameters;

  std::vector<ResourceHandle> images{};

  BufferHandle buffer_handle{};
};


struct AssetMesh {
  struct SubMesh {
    BufferHandle vertex;
    BufferHandle index;
    uint32_t index_count;

    AssetHandle<AssetMaterial> material{};
  };

  std::vector<SubMesh> sub_meshes;
};

struct MeshComponent {
  AssetHandle<AssetMesh> mesh;
};

struct TransformComponent {
  Vec3<float> translation;
  Quat<float> rotation;
  Vec3<float> scale;


};

struct GpuTransform {
  BufferHandle buffer;
};

struct CameraComponent {

  Vec3<float> eye;
  Vec3<float> target;
  float y_fov;
  float near_plane;
  float far_plane;

  enum class Projection { Ortho, Perspective };

  Projection projection = Projection::Perspective;


};

struct GpuCamera {
  BufferHandle buffer;
};

struct ExtractedRendererResources {
  RenderCamera camera;
  std::vector<RenderMesh> meshes;
  std::vector<RenderLight> lights;
};

enum class BufferType {
  Vertex,
  Material,
  Transform,
};

/// Handle to Render Buffers to upload data
struct RenderBuffersResource {

  std::unordered_map<BufferType, ResourceHandle> data = {};
};

struct RenderPlugin : public IPlugin {

  void build(game::Game &game) override;
};