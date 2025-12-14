#include "registry_plugin.h"
#include "game/game.h"
#include <iostream>

void RegistryPlugin::build(game::Game &game) {

  std::cout << "Initialize Registry Plugin\n";

  ComponentRegistry reg{};
  game.world.insertResource<ComponentRegistry>(reg);
}