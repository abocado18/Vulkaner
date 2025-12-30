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

  uint32_t number_mipmaps;

  ResourceHandle image_handle{};
};

struct AssetMaterial {
 

  

  RenderMaterial material_parameters;

  std::vector<AssetHandle<AssetImage>> images{};

  BufferHandle buffer_handle{};
};

struct AssetMesh {

  BufferHandle vertex;
  BufferHandle index;
  uint32_t index_count;
  uint32_t instance_count;
};


struct RenderInstance {
  AssetHandle<AssetMesh> mesh{};
  AssetHandle<AssetMaterial> material{};
};

struct RenderComponent {
  std::vector<RenderInstance> meshes{};
};

struct TransformComponent {
  Vec3<float> translation;
  Quat<float> rotation;
  Vec3<float> scale;
};

struct GpuTransformComponent {
  BufferHandle buffer;
};

struct CameraComponent {

  float y_fov;
  float near_plane;
  float far_plane;
  float aspect;


  float x_mag;
  float y_mag;
  

  enum class Projection { Ortho, Perspective };

  Projection projection = Projection::Perspective;
};

struct GpuCameraComponent {
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
  Camera,
};

/// Handle to Render Buffers to upload data
struct RenderBuffersResource {

  std::unordered_map<BufferType, ResourceHandle> data = {};
};

struct RenderPlugin : public IPlugin {

  void build(game::Game &game) override;
};