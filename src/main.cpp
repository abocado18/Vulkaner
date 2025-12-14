#include <iostream>
#include <vector>

#include "main.h"

#include "game/game.h"
#include "game/plugins/default_components_plugin.h"
#include "game/plugins/registry_plugin.h"
#include "game/plugins/render_plugin.h"
#include "game/plugins/scene_plugin.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"

int main() {

  game::Game gameplay = {};

  { // Add Plugins


    

    RegistryPlugin registry_plugin {};
    gameplay.addPlugin(registry_plugin);

    DefaultComponentsPlugin default_components_plugin {};
    gameplay.addPlugin(default_components_plugin);

    RenderPlugin render_plugin{};
    gameplay.addPlugin(render_plugin);

    ScenePlugin scene_plugin{};
    gameplay.addPlugin(scene_plugin);

    
  }

  gameplay.runStartup();

  while (gameplay.shouldRun()) {

    gameplay.tick();
  }

  

  return 0;
}
