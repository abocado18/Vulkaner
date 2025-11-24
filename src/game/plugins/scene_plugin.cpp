#include "scene_plugin.h"

#include "assimp/Importer.hpp"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/required_components/name.h"
#include "game/required_components/transform.h"
#include "platform/render/renderer.h"
#include <filesystem>

#include <iostream>

using namespace vecs;

void ScenePlugin::build(game::Game &game) {

  std::cout << "Initialize Scene Plugin\n";

  game.world.addSystem<ResMut<Commands>, ResMut<Renderer *>,
                       Added<Read<LoadScenePlugin>>>(
      game.Update, [](auto view, Entity e, Commands &cmd, Renderer *renderer,
                      LoadScenePlugin &load) {
        Assimp::Importer importer;

        const aiScene *scene =
            importer.ReadFile(load.load_scene.c_str(),
                              aiProcess_Triangulate | aiProcess_GenNormals |
                                  aiProcess_OptimizeMeshes);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
            !scene->mRootNode) {

          std::cout << "Could not load Scene\n";

          cmd.push(

              [e](Ecs *world) {
              

                world->removeComponent<LoadScenePlugin>(e);
              });

          return;
        }

        // Load Textures
        {
          if (scene->HasTextures()) {
            for (size_t i = 0; i < scene->mNumTextures; i++) {

              auto *tex = scene->mTextures[i];

              uint32_t width = static_cast<uint32_t>(tex->mWidth);
              uint32_t height = static_cast<uint32_t>(tex->mHeight);

              //Load Texture to GPU here
            }
          }
        }
      });
}