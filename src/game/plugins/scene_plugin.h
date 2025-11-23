#pragma once
#include "fastgltf/types.hpp"
#include "game/plugin.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "fastgltf/core.hpp"

struct LoadScenePlugin {
  std::string load_scene;
};

struct ScenePlugin;

struct LoadedScene {
  std::unordered_map<std::string, std::shared_ptr<fastgltf::Mesh>> meshes;
  std::unordered_map<std::string, std::shared_ptr<fastgltf::Node>> nodes;
  std::unordered_map<std::string, std::shared_ptr<fastgltf::Image>> images;
  std::unordered_map<std::string, std::shared_ptr<fastgltf::Material>>
      materials;

  std::vector<std::shared_ptr<fastgltf::Node>> top_nodes;
};

class ScenePlugin : public IPlugin {

  void build(game::Game &game) override;
};