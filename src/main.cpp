#include <iostream>

#include "main.h"

#include "platform/render/renderer.h"

int main() {

  Renderer renderer(1280, 720);

  while (renderer.shouldUpdate()) {

    renderer.draw();
  }

  return 0;
}
