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


struct RenderBuffers {

  std::unordered_map<BufferType, ResourceHandle> data = {};
};

struct RenderObjectsList
{
  std::vector<RenderObjects> render_objects {};
};

struct RenderPlugin : public IPlugin {

  void build(game::Game &game) override;
};