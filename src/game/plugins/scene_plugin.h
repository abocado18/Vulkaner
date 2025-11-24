#pragma once
#include "game/plugin.h"

#include <memory>
#include <string>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>



struct LoadScenePlugin {
  std::string load_scene;
};

struct ScenePlugin;

struct LoadedScene {

};

class ScenePlugin : public IPlugin {

  void build(game::Game &game) override;
};