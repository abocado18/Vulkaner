#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/asset_plugin.h"
#include "game/plugins/default_components_plugin.h"
#include "game/plugins/registry_plugin.h"
#include "game/plugins/scene_plugin.h"
#include "platform/math/math.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"
#include "platform/render/resources.h"
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

void RenderPlugin::build(game::Game &game) {

  using namespace vecs;

  std::cout << "Initialize Render Plugin\n";

  IRenderer *r = new VulkanRenderer(1280, 720);

  game.world.insertResource<IRenderer *>(r);

  game.world.insertResource<LoadedMeshesResource>({});

  game.world.insertResource<Assets<MeshGpuData>>({});

  auto *reg = game.world.getResource<ComponentRegistry>();

  reg->registerComponent<Mesh>("Mesh", [](vecs::Ecs *world, const json &j,
                                          vecs::Entity e, void *custom_data) {
    AssetHandle<LoadSceneName> *scene_name_handle =
        static_cast<AssetHandle<LoadSceneName> *>(custom_data);

    Mesh m{};
    m.id = j.get<size_t>();

    auto *res = world->getResource<Assets<MeshCpuData>>();

    m.scene_handle = *scene_name_handle;

    world->addComponent<Mesh>(e, m);
  });

  RenderBuffersResource render_buffers = {};

  ResourceHandle vertex_handle =
      r->createBuffer(500'000'000, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  ResourceHandle transform_handle =
      r->createBuffer(1'000'000, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  ResourceHandle material_handle =
      r->createBuffer(1'000'000, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  render_buffers.data[BufferType::Vertex] = vertex_handle;
  render_buffers.data[BufferType::Transform] = transform_handle;
  render_buffers.data[BufferType::Material] = material_handle;

  game.world.insertResource<RenderBuffersResource>(render_buffers);

  game.world.addSystem<vecs::ResMut<IRenderer *>, vecs::ResMut<game::GameData>>(
      game.PostRender, [](auto view, IRenderer *r, game::GameData &game_data) {
        if (r->shouldRun() == false) {
          game_data.should_run = false;
        }
      });

  game.world
      .addSystem<vecs::ResMut<VulkanRenderer *>, vecs::ResMut<vecs::Commands>>(
          game.OnClose, [](auto view, VulkanRenderer *r, vecs::Commands &cmd) {
            // Gets only called once

            cmd.push([r](vecs::Ecs *world) { delete r; });
          });

  game.world.addSystem<Read<GpuTransform>, ResMut<IRenderer *>,
                       Res<RenderBuffersResource>>(
      game.Render, [](auto view, IRenderer *renderer,
                      const RenderBuffersResource &render_buffers) {
        std::vector<RenderObject> render_objects{};

        renderer->draw(render_objects);
      });

#pragma region Add Gpu Components

  game.world.addSystem<Added<Read<Transform>>, Res<RenderBuffersResource>,
                       ResMut<IRenderer *>, ResMut<Commands>>(
      game.PreRender, [](auto view, const RenderBuffersResource &buffers_res,
                         IRenderer *renderer, Commands &cmd) {
        view.forEach([](auto view, Entity e, const Transform &t,
                        const RenderBuffersResource &buffers_res,
                        IRenderer *renderer, Commands &cmd) {
          auto transform_buffer = buffers_res.data.at(BufferType::Transform);

          GpuTransform gpu_transform{};

          Mat4<float> transform_matrix = Mat4<float>::createTransformMatrix(
              t.translation, t.scale, t.rotation);

          gpu_transform.transform_handle = renderer->writeBuffer(
              transform_buffer, &transform_matrix, sizeof(transform_matrix),
              UINT32_MAX, VK_ACCESS_SHADER_READ_BIT);

          cmd.push([e, gpu_transform](Ecs *world) {
            world->addComponent<GpuTransform>(e, gpu_transform);
          });
        });
      });

  game.world
      .addSystem<Added<Write<Mesh>>, Res<RenderBuffersResource>,
                 ResMut<IRenderer *>, ResMut<Commands>,
                 Res<Assets<LoadSceneName>>, ResMut<LoadedMeshesResource>>(
          game.PreRender,
          [](auto view, const RenderBuffersResource &buffers_res,
             IRenderer *renderer, Commands &cmd,
             const Assets<LoadSceneName> &scene_names,
             LoadedMeshesResource &load_meshes_res) {
            //
            view.forEach([](auto view, Entity e, Mesh &m,
                            const RenderBuffersResource &buffers_res,
                            IRenderer *renderer, Commands &cmd,
                            const Assets<LoadSceneName> &scene_names,
                            LoadedMeshesResource &load_meshes_res) {
              auto transform_buffer = buffers_res.data.at(BufferType::Vertex);

              const auto *load_scene_name =
                  scene_names.getConstAsset(m.scene_handle);

              const std::string &scene_path = load_scene_name->scene_path;

              const std::string mesh_data_path =
                  scene_path + "meshes/" + std::to_string(m.id) + ".json";
              const std::string mesh_bin_path =
                  scene_path + "meshes/" + std::to_string(m.id) + ".bin";

              auto it = load_meshes_res.data_map.find(mesh_data_path);

              if (it != load_meshes_res.data_map.end()) {

                if (it->second.loaded == true) {

                  // Mesh already loaded
                }
              }

              std::cout << "Mesh Path: " << mesh_data_path << "\n";

              json mesh_json;
              {
                std::ifstream file(mesh_data_path);

                if (!file.is_open()) {
                  return;
                }

                mesh_json = json::parse(file);

                file.close();

                if (mesh_json.is_discarded()) {
                  return;
                }
              }

              size_t index_byte_offset =
                  mesh_json["Index Byte Offset"].get<size_t>();
              size_t index_number = mesh_json["Index Number"].get<size_t>();
              size_t vertex_number = mesh_json["Vertex Number"].get<size_t>();

              auto loadBinaryData =
                  [](const std::string &path) -> std::vector<unsigned char> {
                std::ifstream file(path, std::ios::binary);

                if (!file.is_open())
                  return {};

                std::vector<unsigned char> data(
                    std::istreambuf_iterator<char>(file), {});

                return data;
              };

              std::vector<unsigned char> mesh_bin_data =
                  loadBinaryData(mesh_bin_path);

              GpuMesh gpu_mesh{};

              gpu_mesh.vertex_index_buffer_handle = renderer->writeBuffer(
                  transform_buffer, mesh_bin_data.data(),
                  mesh_bin_data.size() * sizeof(unsigned char), UINT32_MAX,
                  VK_ACCESS_SHADER_READ_BIT);

              gpu_mesh.index_number = index_number;
              gpu_mesh.index_byte_offset = index_byte_offset;

              // mesh_data->loaded = true;

              cmd.push([gpu_mesh, e](Ecs *world) {
                world->addComponent<GpuMesh>(e, gpu_mesh);
              });
            });
          });

#pragma endregion
}
