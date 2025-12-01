#include "game.h"
#include "ecs/vox_ecs.h"
#include <chrono>

game::Game::Game() : world() {

  time_data.delta_time = 0.0f;
  time_data.total_time = 0.0f;
  time_data.total_ticks = 0;

  world.insertResource<Time>(time_data);
}

game::Game::~Game() {
  world.runSchedule(OnClose);

  world.executeCommands();
}

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
  world.executeCommands();

  world.runSchedule(Update);
  world.executeCommands();

  world.runSchedule(PostUpdate);
  world.executeCommands();

  world.update();
}