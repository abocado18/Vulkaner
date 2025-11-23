#include <iostream>

#include "main.h"

#include "game/game.h"
#include "platform/render/renderer.h"

int main() {

  Renderer renderer(1280, 720);

  game::Game gameplay = {};

  gameplay.world.insertResource<Renderer *>(&renderer);

  while (renderer.shouldUpdate()) {

    renderer.draw();
  }

  return 0;
}
