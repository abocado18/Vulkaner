

#include "default_components_plugin.h"
#include "game/game.h"
#include "game/plugins/camera_plugin.h"
#include "game/plugins/registry_plugin.h"
#include <cstdlib>
#include <iostream>

using namespace vecs;

void DefaultComponentsPlugin::build(game::Game &game) {

  std::cout << "Initialized Default Components\n";

  CameraPlugin camera_plugin{};

  game.addPlugin(camera_plugin);

  auto *reg = game.world.getResource<ComponentRegistry>();

  if (!reg) {
    std::cerr << "No Registry\n";
    std::abort();
  }

  reg->registerComponent<Name>("Name");
  reg->registerComponent<Transform>("Transform");
  reg->registerComponent<Parent>("Parent");
}