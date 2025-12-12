#pragma once
#include "game/plugin.h"
#include "game/required_components/materials.h"
#include "nlohmann/json_fwd.hpp"

#include <array>
#include <optional>
#include <vector>

#include <memory>
#include <string>
#include <unordered_map>

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



//Registry for all serializable Components
#define COMP_LIST(T) \
X(Transform) \
X(GlobalTransform) \