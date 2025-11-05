#pragma once

#include "ecs/vox_ecs.h"

#include "platform/renderer/renderer.h"

#include "platform/math/math.h"
#include <bits/types/timer_t.h>

#include <chrono>

namespace game {



struct Time
{
  float delta_time;
  float total_time;
  uint64_t total_ticks;
};


class Game {
public:
  Game(render::RenderContext &render_ctx);
  ~Game();


  void tick();

  void runStartup();


private:
  vecs::Ecs world;

  Time time_data = {};
  

  std::chrono::time_point<std::chrono::high_resolution_clock> last_frame_start;

  render::RenderContext &render_ctx;


  vecs::Schedule Startup = {};

  vecs::Schedule PreUpdate = {};
  vecs::Schedule Update = {};
  vecs::Schedule PostUpdate = {};

};
} // namespace game