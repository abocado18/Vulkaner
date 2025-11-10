#include "game.h"
#include "ecs/vox_ecs.h"
#include "platform/renderer/renderer.h"
#include <chrono>

#include "platform/loader/scene_loader.h"

game::Game::Game(render::RenderContext &render_ctx)
    : render_ctx(render_ctx), world() {

  time_data.delta_time = 0.0f;
  time_data.total_time = 0.0f;
  time_data.total_ticks = 0;

  world.insertResource<Time>(time_data);

  gltf_load::loadScene(ASSET_PATH "/monke.glb", world, render_ctx);
}

game::Game::~Game() {}

void game::Game::runStartup() { world.runSchedule(Startup); }

void game::Game::tick() {

  auto now = std::chrono::high_resolution_clock::now();

  time_data.delta_time = (std::chrono::duration_cast<std::chrono::microseconds>(
                              now - last_frame_start)
                              .count()) /
                         1'000'000.f;

  last_frame_start = std::chrono::high_resolution_clock::now();

  time_data.total_time += time_data.delta_time;
  time_data.total_ticks += 1;

  world.insertResource<Time>(time_data);

  world.runSchedule(PreUpdate);
  world.runSchedule(Update);
  world.runSchedule(PostUpdate);

  world.updateWorldTick();
}