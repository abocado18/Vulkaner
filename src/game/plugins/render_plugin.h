#pragma once

#include "game/game.h"
#include "game/plugin.h"
#include "platform/render/resources.h"
#include <unordered_map>



enum class BufferType 
{
  Vertex,
  Material,
  Transform,
};


///Handle to Render Buffers to upload data
struct RenderBuffersResource {

  std::unordered_map<BufferType, ResourceHandle> data = {};
};



struct RenderPlugin : public IPlugin {

  void build(game::Game &game) override;
};