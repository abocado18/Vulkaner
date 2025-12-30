#pragma once

#include "game/ecs/vox_ecs.h"
#include "game/plugin.h"

#include "nlohmann/detail/macro_scope.hpp"
#include "nlohmann/json.hpp"
#include "platform/math/math.h"

using json = nlohmann::json;




struct ParentComponent {
  vecs::Entity id;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ParentComponent, id)



struct NameComponent {

  std::string name;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NameComponent, name)

/// Contains default compoents and plugins
class DefaultComponentsPlugin : public IPlugin {

  void build(game::Game &game) override;
};