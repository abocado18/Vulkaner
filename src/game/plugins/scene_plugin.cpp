#include "scene_plugin.h"

#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/render_plugin.h"
#include "game/required_components/materials.h"
#include "game/required_components/name.h"
#include "game/required_components/transform.h"
#include "platform/math/math.h"
#include "platform/render/renderer.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace vecs;

void ScenePlugin::build(game::Game &game) {

  std::cout << "Initialize Scene Plugin\n";

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

                       Res<RenderBuffersResource>, Added<Read<LoadSceneName>>>(
      game.Update, [](auto view, Commands &cmd, IRenderer *renderer,
                      const RenderBuffersResource &render_buffers) {
        //

        view.forEach([](auto view, Entity e, Commands &cmd, IRenderer *renderer,
                        const RenderBuffersResource &render_buffers,
                        const LoadSceneName &scene_name) {
          const std::string scene_path = scene_name.scene_path;

          std::cout << "Load Scene: " << scene_path << "\n";

          cmd.push(
              [e](Ecs *world) { world->removeComponent<LoadSceneName>(e); });

          json scene_json;
          {
            std::ifstream file(scene_path + "/scene.json");

            if (!file.is_open()) {
              std::cout << "Could not open Scene " << scene_path << "\n";
              return;
            }

            scene_json = json::parse(file);

            if (scene_json.is_discarded()) {
              std::cout << "Scene " << scene_path << " corrupted\n";
              return;
            }
          }

          std::shared_ptr<std::unordered_map<int32_t, int32_t>>
              scene_index_to_entity_index =
                  std::make_shared<std::unordered_map<int32_t, int32_t>>();

          for (auto &e : scene_json) {

            int32_t e_scene_id = e["id"].get<int32_t>();
            int32_t parent_e_scene_id = e["parent"].get<int32_t>();


            for (auto &c : e["components"]) {
            }
          }
        });
      });
}