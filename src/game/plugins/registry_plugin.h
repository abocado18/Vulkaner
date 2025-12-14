#pragma once

#include <unordered_map>

#include "game/ecs/vox_ecs.h"
#include "game/plugin.h"

#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

using json = nlohmann::json;

struct ComponentRegistry {

public:
  using RegistryFunc = void (*)(vecs::Ecs *world, const json &j, vecs::Entity e);

  template <typename T> void registerComponent(const std::string &name) {

    registry[name] = [](vecs::Ecs *world, const json &j, vecs::Entity e) {
      T comp = j.get<T>();

      world->addComponent<T>(e, comp);
    };
  }

  RegistryFunc getRegistryFunc(const std::string &name) {
    return registry.at(name);
  }

private:
  std::unordered_map<std::string, RegistryFunc> registry{};
};

class RegistryPlugin : public IPlugin {

  void build(game::Game &game) override;
};