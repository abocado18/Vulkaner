#include <iostream>
#include <vector>

#include "main.h"

#include "game/game.h"
#include "game/plugins/render_plugin.h"
#include "game/plugins/scene_plugin.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"

int main() {

  game::Game gameplay = {};

  { // Add Plugins

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
