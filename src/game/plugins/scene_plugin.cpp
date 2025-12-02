#include "scene_plugin.h"

#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/render_plugin.h"
#include "game/required_components/name.h"
#include "game/required_components/transform.h"
#include "platform/math/math.h"
#include "platform/render/renderer.h"

#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace vecs;

void ScenePlugin::build(game::Game &game) {

  std::cout << "Initialize Scene Plugin\n";

  game.world.addSystem<ResMut<Commands>, ResMut<Renderer *>, Res<RenderBuffers>,
                       Added<Read<LoadSceneName>>>(
      game.Update, [](auto view, Entity e, Commands &cmd, Renderer *renderer,
                      RenderBuffers &render_buffers, const LoadSceneName &load) {
        const std::string &file_path = load.load_scene;

        std::cout << "Load Scene " << file_path << "\n";

        auto loadBinaryFile =
            [](const std::string &path) -> std::vector<uint8_t> {
          std::ifstream file(path, std::ios::binary | std::ios::ate);

          if (!file.is_open()) {
            std::cerr << "Could not open binary\n";
            return {};
          }

          size_t size = file.tellg();
          file.seekg(0, std::ios::beg);

          std::vector<uint8_t> data(size);

          if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
            std::cerr << "Failed to read full binary file\n";
            return {};
          }

          return data;
        };

        json entities_json;
        {
          std::ifstream file(file_path + "entities/entities.json");

          if (!file.is_open()) {
            std::cerr << "Could not open Entity json file with path "
                      << file_path + "entities/entities.json\n";

            cmd.push(
                [e](Ecs *world) { world->removeComponent<LoadSceneName>(e); });

            return;
          }

          entities_json = json::parse(file, nullptr, false);

          if (entities_json.is_discarded()) {
            std::cerr << "Entities Json File corrupted: "
                      << file_path + "entities/entities.json\n";

            cmd.push(
                [e](Ecs *world) { world->removeComponent<LoadSceneName>(e); });

            return;
          }

          file.close();
        }

        #pragma region Entities

        std::vector<SceneFormatStructs::Entity> entities = {};

        for (auto &e : entities_json) {
          SceneFormatStructs::Entity entity = {};
          entity.id = e["id"].get<std::string>();
          entity.name = e["name"].get<std::string>();

          auto &transform = e["transform"];

          entity.position = {transform["translation"][0],
                             transform["translation"][1],
                             transform["translation"][2]};

          entity.rotation = {transform["rotation"][0], transform["rotation"][1],
                             transform["rotation"][2],
                             transform["rotation"][3]};

          entity.scale = {transform["scale"][0], transform["scale"][1],
                          transform["scale"][2]};

          for (auto &m : e["mesh_renderers"]) {

            SceneFormatStructs::MeshRenderer mesh_renderer = {};
            mesh_renderer.mesh = m["mesh"].get<std::string>();
            mesh_renderer.material = m["material"].get<std::string>();

            entity.meshes.push_back(mesh_renderer);
          }

          entities.push_back(entity);
        }

        #pragma endregion

        std::unordered_map<std::string, uint64_t> loaded_images = {};
        std::unordered_map<std::string, SceneFormatStructs::Material>
            loaded_materials = {};
        std::unordered_map<SceneFormatStructs::uuid, uint64_t> loaded_meshes = {};

        for (auto &e : entities) {

          auto quatToEuler = [](Quat<float> quat) -> Vec3<float> {
            const float w = quat.w;
            const float x = quat.x;
            const float y = quat.y;
            const float z = quat.z;

            Vec3<float> euler = {};

            float sinr_cosp = 2.0f * (w * x + y * z);
            float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
            euler.x = std::atan2(sinr_cosp, cosr_cosp);

            float sinp = 2.0f * (w * y - z * x);
            if (std::abs(sinp) >= 1.0f)
              euler.y = std::copysign(M_PI / 2.0f, sinp); // clamp
            else
              euler.y = std::asin(sinp);

            float siny_cosp = 2.0f * (w * z + x * y);
            float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
            euler.z = std::atan2(siny_cosp, cosy_cosp);

            return euler;
          };

          Transform transform = {};
          transform.translation = e.position;
          transform.rotation = quatToEuler(e.rotation);
          transform.scale = e.scale;

          Name name = {};
          name.name = e.name;

          for (auto &m : e.meshes) {

            auto it = loaded_meshes.find(m.mesh);

            // Check if already loaded
            if (it == loaded_meshes.end()) {

              const std::string mesh_path =
                  file_path + "meshes/" + m.mesh + ".mesh.json";

              json mesh_json;

              std::ifstream mesh_file(mesh_path);

              if (!mesh_file.is_open()) {
                std::cerr << "Could not open Mesh File " << mesh_path << "\n";
                continue;
              }

              mesh_json = json::parse(mesh_file);

              if (mesh_json.is_discarded()) {
                std::cerr << "json is discarded: " << mesh_path << "\n";
                continue;
              }

              size_t file_vertex_offset =
                  mesh_json["vertex_offset"].get<size_t>(); //Is always 0
              size_t file_index_offset =
                  mesh_json["index_offset"].get<size_t>();
              size_t file_vertex_size = mesh_json["vertex_size"].get<size_t>();
              size_t file_index_size = mesh_json["index_size"].get<size_t>();

              std::vector<uint8_t> bin_data = loadBinaryFile(
                  file_path + "meshes/" + m.mesh + ".mesh_data.bin");

              if (bin_data.size() == 0) {
                std::cerr << "Corrupt Mesh Data\n";
                continue;
              }

              // Upload GPU data here
              auto vertex_handle = render_buffers.data.at(BufferType::Vertex);
              uint32_t gpu_vertex_offset =
                  renderer->writeBuffer(vertex_handle, bin_data.data(),
                                        sizeof(uint8_t) * bin_data.size(),
                                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                                            VK_ACCESS_INDEX_READ_BIT);

              

              cmd.push([gpu_vertex_offset, file_index_offset, file_index_size](Ecs *world) {



                Entity e = world->createEntity();


                SceneAssetStructs::Mesh mesh = {};
                mesh.vertex_offset = gpu_vertex_offset;
                mesh.index_offset = gpu_vertex_offset + file_index_offset;
                mesh.index_number = file_index_size;

                world->addComponent<SceneAssetStructs::Mesh>(e, mesh);

              });

            }
          }
        }
      });

  game.world.addSystem<ResMut<Commands>>(
      game.Startup, [](auto view, Entity e, Commands &cmd) {
        std::cout << "Load Test Scene\n";
        cmd.push([](Ecs *world) {
          Entity e = world->createEntity();
          LoadSceneName n = {ASSET_PATH "/scene/"};
          world->addComponent<LoadSceneName>(e, n);
        });
      });
}