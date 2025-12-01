#include <iostream>
#include <vector>

#include "main.h"

#include "game/game.h"
#include "game/plugins/scene_plugin.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"

int main() {

  Renderer renderer(1280, 720);

  game::Game gameplay = {};

  { //Add Plugins
    ScenePlugin scene_plugin {};
    gameplay.addPlugin(scene_plugin);
  }

  gameplay.world.insertResource<Renderer *>(&renderer);

  while (renderer.shouldUpdate()) {

    std::vector<RenderObject> draws {};
    renderer.draw(draws); 
  }

  return 0;
}
