#pragma once

#include "game/game.h"
#include "game/plugin.h"
#include "platform/render/resources.h"
#include <string>
#include <unordered_map>

struct RenderBuffers {

  std::unordered_map<std::string, ResourceHandle> data = {};
};

struct RenderPlugin : public IPlugin {

  void build(game::Game &game) override;
};