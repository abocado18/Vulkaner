#include "loader.h"

#include "game/ecs/vox_ecs.h"
#include "game/game.h"

void SceneLoader::build(game::Game &game) {

  game.world.addSystem<vecs::Added<vecs::Read<SceneBundle>>>(
      game.Update, [](auto view, vecs::Entity e, const SceneBundle &bundle) {

      });

  game.world.addSystem<vecs::Res<vecs::Removed<SceneBundle>>>(
      game.Update,
      [](auto &view, vecs::Entity e, const vecs::Removed<SceneBundle> &bundle) {


        

      });
}