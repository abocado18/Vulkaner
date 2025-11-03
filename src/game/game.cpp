#include "game.h"
#include "ecs/vox_ecs.h"
#include "platform/renderer/renderer.h"

game::Game::Game(render::IRenderContext &render_ctx)
    : render_ctx(render_ctx), world() {}

game::Game::~Game() {}


void game::Game::runStartup()
{
    world.runSchedule(Startup);
}

void game::Game::tick() {
  world.runSchedule(PreUpdate);
  world.runSchedule(Update);
  world.runSchedule(PostUpdate);
}