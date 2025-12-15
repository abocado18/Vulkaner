#include "scene_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/asset_plugin.h"
#include "game/plugins/default_components_plugin.h"
#include "game/plugins/registry_plugin.h"
#include "game/plugins/render_plugin.h"

#include "platform/render/renderer.h"

#include <X11/Xmd.h>

#include <cstdint>
#include <fstream>

#include <iostream>

#include <string>

#include <unordered_map>

#include <vector>
#include <vulkan/vulkan_core.h>

using namespace vecs;

void ScenePlugin::build(game::Game &game) {

  std::cout << "Initialize Scene Plugin\n";

  Assets<LoadSceneName> scene_name_assets{};

  game.world.insertResource<Assets<LoadSceneName>>(scene_name_assets);

  game.world.addSystem<ResMut<Commands>>(
      game.Startup, [](auto view, Commands &cmd) {
        std::cout << "Load Test Scene\n";
        cmd.push([](Ecs *world) {
          Entity e = world->createEntity();
          LoadSceneName n = {ASSET_PATH "/scene/"};
          world->addComponent<LoadSceneName>(e, n);
        });
      });

  game.world.addSystem<ResMut<Commands>, ResMut<IRenderer *>,
                       ResMut<Assets<LoadSceneName>>,
                       Res<RenderBuffersResource>, Added<Read<LoadSceneName>>>(
      game.Update, [](auto view, Commands &cmd, IRenderer *renderer,
                      Assets<LoadSceneName> &asset_scene_names,
                      const RenderBuffersResource &render_buffers) {
        //

        view.forEach([](auto view, Entity e, Commands &cmd, IRenderer *renderer,
                        Assets<LoadSceneName> &asset_scene_names,
                        const RenderBuffersResource &render_buffers,
                        const LoadSceneName &scene_name) {
          const std::string scene_path = scene_name.scene_path;

          std::cout << "Load Scene: " << scene_path << "\n";

          // Cache Scene Name as asset
          AssetHandle<LoadSceneName> file_handle = asset_scene_names.registerAsset(LoadSceneName{scene_name});

          cmd.push(
              [e](Ecs *world) { world->removeComponent<LoadSceneName>(e); });

          json scene_json;
          {
            std::ifstream file(scene_path + "/scene.json");

            if (!file.is_open()) {
              std::cout << "Could not open Scene " << scene_path + "/scene.json"
                        << "\n";
              return;
            }

            scene_json = json::parse(file);

            if (scene_json.is_discarded()) {
              std::cout << "Scene " << scene_path << " corrupted\n";
              return;
            }
          }
          // Starts here

          cmd.push([scene_json, file_handle](Ecs *world) mutable {
            std::unordered_map<int32_t, Entity> file_index_to_node_index{};
            {
              for (auto &entity : scene_json) {

                int32_t scene_id = entity["id"].get<int32_t>();

                file_index_to_node_index[scene_id] = world->createEntity();
              };
            }

            for (auto &entity : scene_json) {
              int32_t entity_scene_id = entity["id"].get<int32_t>();
              int32_t parent_scene_id = entity["parent"].get<int32_t>();

              Entity entity_id = file_index_to_node_index.at(entity_scene_id);

              if (parent_scene_id >= 0) {

                Entity parent_id = file_index_to_node_index.at(parent_scene_id);

                world->addComponent<Parent>(entity_id, Parent{parent_id});
              }

              const auto &components = entity["components"];

              ComponentRegistry *registry =
                  world->getResource<ComponentRegistry>();

              if (registry == nullptr) {
                std::cerr << "Compoennt Registry must be addded\n";
                std::abort();
              }

              for (auto it = components.begin(); it != components.end(); it++) {

                const std::string &name = it.key();
                const json &json_data = it.value();


                

                registry->getRegistryFunc(name)(world, json_data, entity_id, &file_handle);
              }
            }
          });
        });
      });
}