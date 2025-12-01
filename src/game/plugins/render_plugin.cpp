#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"
#include <vector>

void RenderPlugin::build(game::Game &game) {

  Renderer *r = new Renderer(1280, 720);

  

  game.world.insertResource<Renderer *>(r);

  game.world.addSystem<vecs::ResMut<Renderer *>, vecs::ResMut<game::GameData>>(
      game.PostUpdate, [](auto view, vecs::Entity e, Renderer *r, game::GameData &game_data) {
        std::vector<RenderObject> render_objects = {};

        
        if(r->shouldUpdate() == false) {
          game_data.should_run = false;
        }
        
        
        r->draw(render_objects);





      });

  game.world.addSystem<vecs::ResMut<Renderer *>, vecs::ResMut<vecs::Commands>>(
      game.OnClose,
      [](auto view, vecs::Entity e, Renderer *r, vecs::Commands &cmd) {
        // Gets only called once

        cmd.push([r](vecs::Ecs *world) { delete r; });
      });
}