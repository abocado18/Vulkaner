#pragma once
#include "fastgltf/types.hpp"
#include "game/plugin.h"

#include <array>
#include <optional>
#include <vector>

#include <memory>
#include <string>
#include <unordered_map>

#include "ktx.h"
#include "platform/math/math.h"
#include "platform/render/resources.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct LoadSceneName {
  std::string scene_path;
};

class ScenePlugin : public IPlugin {

  void build(game::Game &game) override;
};


