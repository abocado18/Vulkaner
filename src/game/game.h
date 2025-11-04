#pragma once

#include "ecs/vox_ecs.h"

#include "platform/renderer/renderer.h"

#include "platform/math/math.h"

namespace game {

class Game {
public:
  Game(render::IRenderContext &render_ctx);
  ~Game();


  void tick();

  void runStartup();


private:
  vecs::Ecs world;

  render::IRenderContext &render_ctx;


  vecs::Schedule Startup = {};

  vecs::Schedule PreUpdate = {};
  vecs::Schedule Update = {};
  vecs::Schedule PostUpdate = {};

};
} // namespace game