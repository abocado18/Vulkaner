

#include "default_components_plugin.h"
#include "game/game.h"
#include "game/plugins/registry_plugin.h"
#include <cstdlib>
#include <iostream>

using namespace vecs;

void DefaultComponentsPlugin::build(game::Game &game) {

  std::cout << "Initialized Default Components\n";



  auto *reg = game.world.getResource<ComponentRegistry>();

  if (!reg) {
    std::cerr << "No Registry\n";
    std::abort();
  }

  reg->registerComponent<NameComponent>("Name");

  reg->registerComponent<ParentComponent>("Parent");
}