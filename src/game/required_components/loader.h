#pragma once

#include "game/plugin.h"
#include <string>

struct SceneBundle {};

struct SceneLoader {
  std::string scene_path;
};

class SceneLoaderPlugin : public IPlugin {
public:
  SceneLoaderPlugin() = default;
  ~SceneLoaderPlugin() = default;

private:
  void build(game::Game &game) override;
};