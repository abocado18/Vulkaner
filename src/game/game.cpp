#include "game.h"
#include "ecs/vox_ecs.h"
#include <chrono>

game::Game::Game() : world() {

  time_data.delta_time = 0.0f;
  time_data.total_time = 0.0f;
  time_data.total_ticks = 0;

  world.insertResource<Time>(time_data);

  GameData game_data = {true};

  world.insertResource<GameData>(game_data);
}

game::Game::~Game() {
  world.runSchedule(OnClose);

  world.executeCommands();
}

void game::Game::runStartup() {
  world.runSchedule(Startup);
  world.executeCommands();
}

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

  auto runAndExecute = [](vecs::Ecs &world, vecs::Schedule schedule) {
    world.runSchedule(schedule);
    world.executeCommands();
  };

  runAndExecute(world, PreUpdate);
  runAndExecute(world, Update);
  runAndExecute(world, PostUpdate);

  runAndExecute(world, PreRender);
  runAndExecute(world, Render);
  runAndExecute(world, PostRender);

  world.update();
}

const bool game::Game::shouldRun() {

  GameData *game_data = world.getResource<GameData>();

  return game_data->should_run;
}