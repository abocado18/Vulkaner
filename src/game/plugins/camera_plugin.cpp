#include "camera_plugin.h"
#include "game/game.h"
#include "game/plugins/registry_plugin.h"
#include <cstdlib>
#include <iostream>

void CameraPlugin::build(game::Game &game) {

  std::cout << "Camera Plugin\n";

  auto *reg = game.world.getResource<ComponentRegistry>();

  if (!reg) {
    std::abort();
  }

  reg->registerComponent<Camera>("Camera");
}