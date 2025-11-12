#include "loader.h"

#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "platform/renderer/renderer.h"

void SceneLoaderPlugin::build(game::Game &game) {

  game.world.addSystem<vecs::Added<vecs::Read<SceneBundle>>>(
      game.Update, [](auto view, vecs::Entity e, const SceneBundle &bundle) {

      });

  game.world.addSystem<vecs::Res<vecs::Removed<SceneBundle>>>(
      game.Update,
      [](auto &view, vecs::Entity e, const vecs::Removed<SceneBundle> &bundle) {

      });

  game.world.addSystem<vecs::Added<vecs::Read<SceneLoader>>,
                       vecs::ResMut<render::RenderContext *>,
                       vecs::ResMut<vecs::Commands>>(
      game.Update,
      [](auto view, vecs::Entity e, const SceneLoader &loader,
         render::RenderContext *render_ctx, vecs::Commands &commands) {
        commands.push([](vecs::Ecs *world) -> void {
            
        });
      });
}