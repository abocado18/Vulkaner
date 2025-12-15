#pragma once

#include "game/game.h"
#include "game/plugin.h"
#include "game/plugins/asset_plugin.h"
#include "game/plugins/scene_plugin.h"
#include "nlohmann/json.hpp"
#include "platform/render/resources.h"
#include <unordered_map>

// Keep track on loaded meshes
struct MeshCpuData {
  bool loaded = false;
};

struct MeshGpuData {
  BufferHandle vertex_index_buffer_handle{};
  size_t index_number;
  size_t index_byte_offset;
};

struct LoadedMeshesResource {
  std::unordered_map<std::string, MeshCpuData> data_map{};
};

struct Mesh {
  size_t id;
  AssetHandle<LoadSceneName> scene_handle;
};

struct GpuMesh {
  AssetHandle<MeshGpuData> mesh_gpu_data{};
};

struct GpuTransform {
  BufferHandle transform_handle;
};

struct GpuMaterial {
  BufferHandle material_handle{};
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