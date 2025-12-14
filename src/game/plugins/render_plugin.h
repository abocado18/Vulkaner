#pragma once

#include "game/game.h"
#include "game/plugin.h"
#include "nlohmann/json.hpp"
#include "platform/render/resources.h"
#include <unordered_map>

struct Mesh {

  size_t id;
};

inline void to_json(nlohmann::json &j, const Mesh &m) { j = m.id; }

inline void from_json(const nlohmann::json &j, Mesh &m) {
  m.id = j.get<size_t>();
}

struct GpuMesh {
  BufferHandle vertex_index_buffer_handle{};
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