#include "scene_plugin.h"

#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/render_plugin.h"
#include "game/required_components/materials.h"
#include "game/required_components/name.h"
#include "game/required_components/transform.h"
#include "platform/math/math.h"
#include "platform/render/renderer.h"

#include <cstdint>
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

  game.world.addSystem<ResMut<Commands>, ResMut<IRenderer *>,
                       Res<RenderBuffers>, Added<Read<LoadSceneName>>>(
      game.Update,
      [](auto view, Entity e, Commands &cmd, IRenderer *renderer,
         const RenderBuffers &render_buffers, const LoadSceneName &load) {
        const std::string &file_path = load.load_scene;

        std::cout << "Load Scene " << file_path << "\n";

        cmd.push([e](Ecs *world) { world->removeComponent<LoadSceneName>(e); });

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

            return;
          }

          entities_json = json::parse(file, nullptr, false);

          if (entities_json.is_discarded()) {
            std::cerr << "Entities Json File corrupted: "
                      << file_path + "entities/entities.json\n";

            file.close();

            return;
          }

          file.close();
        }

        json materials_json;
        {
          std::ifstream file(file_path + "materials/materials.json");

          if (!file.is_open()) {
            std::cerr << "Could not open Material Json File with path"
                      << file_path + "materials/materials.json\n";
            return;
          }

          materials_json = json::parse(file, nullptr, false);

          if (materials_json.is_discarded()) {
            std::cerr << "Materials file corrupted " << file_path
                      << "materials/materials.json\n";

            file.close();
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

        std::unordered_map<SceneFormatStructs::uuid, SceneAssetStructs::Texture>
            loaded_images = {};
        std::unordered_map<SceneFormatStructs::uuid, SceneAssetStructs::Mesh>
            loaded_meshes = {};
        std::unordered_map<SceneFormatStructs::uuid,
                           SceneAssetStructs::Material<StandardMaterial>>
            loaded_materials; // Assume StandardMaterila for now

        for (auto &mat : materials_json) {

          // Assume StandardMaterial for now
          SceneAssetStructs::Material<StandardMaterial> gpu_mat = {};

          for (auto &tex : mat["textures"]) {

            auto key = tex.begin().key();
            auto value = tex.begin().value();

            auto getTextureType =
                [](const std::string &name) -> SceneAssetStructs::TextureType {
              if (name == "Albedo")
                return SceneAssetStructs::TextureType::Albedo;

              else if (name == "MetallicRoughness")
                return SceneAssetStructs::TextureType::Metallic_Roughness;

              else if (name == "Emissive")
                return SceneAssetStructs::TextureType::Emissive;

              else if (name == "Normal")
                return SceneAssetStructs::TextureType::Normal;

              else {
                std::cerr << "Not valid Texture Type\n";
                std::abort();
              }
            };

            if (!value.empty()) {

              const SceneFormatStructs::uuid id =
                  value.get<SceneFormatStructs::uuid>();

              auto it = loaded_images.find(id);

              if (it == loaded_images.end()) {

                json image_json;
                {
                  std::ifstream file(file_path + "images/" + id + ".img.json");

                  if (!file.is_open()) {
                    std::cerr << "Could not open Image file: "
                              << file_path + "images/" + id + ".img.json\n";
                    std::abort();
                  }

                  image_json = json::parse(file, nullptr, false);

                  if (image_json.is_discarded()) {
                    std::cerr << "File corrupted: "
                              << file_path + "images/" + id + ".img.json\n";
                    file.close();
                    std::abort();
                  }

                  file.close();
                }

                int width = image_json["extent"][0];
                int height = image_json["extent"][1];

                SceneAssetStructs::TextureType tex_type = getTextureType(key);

                VkFormat format;

                if (tex_type == SceneAssetStructs::TextureType::Albedo ||
                    tex_type == SceneAssetStructs::TextureType::Emissive) {
                  format = VK_FORMAT_R8G8B8A8_SRGB;
                }

                else {
                  format = VK_FORMAT_R8G8B8A8_UNORM;
                }

                ResourceHandle tex_handle = renderer->createImage(
                    {(uint32_t)width, (uint32_t)height, 1}, VK_IMAGE_TYPE_2D,
                    format,
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, true, 1);

                std::vector<uint8_t> img_data = loadBinaryFile(
                    file_path + "images/" + id + ".img_data.bin");

                size_t img_size = img_data.size() * sizeof(uint8_t);

                assert(img_size == 4 * width * height);

                renderer->writeImage(tex_handle, img_data.data(),
                                     4 * width * height, {0},
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                gpu_mat.textures[gpu_mat.number_textures++] = {tex_type,
                                                               tex_handle};
              } else {
                gpu_mat.textures[gpu_mat.number_textures++] = it->second;
              }

            } else {
              // No Texture
            }
          }

          gpu_mat.material_data.albedo[0] = mat["albedo_color"][0].get<float>();
          gpu_mat.material_data.albedo[1] = mat["albedo_color"][1].get<float>();
          gpu_mat.material_data.albedo[2] = mat["albedo_color"][2].get<float>();
          gpu_mat.material_data.albedo[3] = mat["albedo_color"][3].get<float>();

          gpu_mat.material_data.metallic = mat["metallic"].get<float>();
          gpu_mat.material_data.roughness = mat["roughness"].get<float>();

          gpu_mat.material_data.emissive[0] =
              mat["emissive_color"][0].get<float>();
          gpu_mat.material_data.emissive[1] =
              mat["emissive_color"][1].get<float>();
          gpu_mat.material_data.emissive[2] =
              mat["emissive_color"][2].get<float>();

          auto material_buffer_handle =
              render_buffers.data.at(BufferType::Material);

          gpu_mat.gpu_material_handle = renderer->writeBuffer(
              material_buffer_handle, &gpu_mat.material_data,
              sizeof(gpu_mat.material_data), UINT32_MAX,
              VK_ACCESS_SHADER_READ_BIT);

          loaded_materials.insert_or_assign(
              mat["id"].get<SceneFormatStructs::uuid>(), gpu_mat);
        }

        // Materials and Images loaded and cached..

        // Loop over all Entites and load Components, Meshes etc..
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

          SceneAssetStructs::MeshRenderer mesh_renderer = {};

          for (auto &m : e.meshes) {

            {

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
                    mesh_json["vertex_offset"].get<size_t>(); // Is always 0
                size_t file_index_offset =
                    mesh_json["index_offset"].get<size_t>();
                size_t file_vertex_size =
                    mesh_json["vertex_size"].get<size_t>();
                size_t file_index_size = mesh_json["index_size"].get<size_t>();

                std::vector<uint8_t> bin_data = loadBinaryFile(
                    file_path + "meshes/" + m.mesh + ".mesh_data.bin");

                if (bin_data.size() == 0) {
                  std::cerr << "Corrupt Mesh Data\n";
                  continue;
                }

                size_t data_size = bin_data.size() * sizeof(uint8_t);

                // Upload GPU data here
                auto vertex_handle = render_buffers.data.at(BufferType::Vertex);
                BufferHandle gpu_vertex_handle = renderer->writeBuffer(
                    vertex_handle, bin_data.data(), data_size, UINT32_MAX,
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                        VK_ACCESS_INDEX_READ_BIT);

                SceneAssetStructs::Mesh mesh = {
                    .vertex_offset = gpu_vertex_handle.getBufferSpace()[0],
                    .index_offset = gpu_vertex_handle.getBufferSpace()[0] +
                                    (uint32_t)file_index_offset,
                    .index_number = (uint32_t)file_index_size,
                    .mesh_gpu_handle = gpu_vertex_handle,
                    .visible = true,
                    .material_gpu_handle = loaded_materials.at(m.material),
                };

                loaded_meshes.insert_or_assign(m.mesh, mesh);
              }

              SceneAssetStructs::Mesh gpu_mesh = loaded_meshes.at(m.mesh);
              mesh_renderer.meshes.push_back(gpu_mesh);
            }

            // Havwe all meshes
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