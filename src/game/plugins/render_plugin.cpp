#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/asset_plugin.h"
#include "game/plugins/camera_plugin.h"
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

  game.world.insertResource<Assets<MeshGpuData>>({});

  game.world.insertResource<ExtractedRendererResources>({});

  auto *reg = game.world.getResource<ComponentRegistry>();

  reg->registerComponent<Mesh>("Mesh", [](vecs::Ecs *world, const json &j,
                                          vecs::Entity e, void *custom_data) {
    AssetHandle<LoadSceneName> *scene_name_handle =
        static_cast<AssetHandle<LoadSceneName> *>(custom_data);

    Mesh m{};
    m.id = j.get<size_t>();

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
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
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

  game.world.addSystem<vecs::ResMut<IRenderer *>, vecs::ResMut<vecs::Commands>>(
      game.OnClose, [](auto view, IRenderer *r, vecs::Commands &cmd) {
        // Gets only called once

        cmd.push([r](vecs::Ecs *world) { delete r; });
      });

  game.world.addSystem<Read<GpuCamera>, ResMut<ExtractedRendererResources>>(
      game.Extract,
      [](auto view, ExtractedRendererResources &render_resources) {
        view.forEach([](auto view, Entity e, const GpuCamera &gpu_cam,
                        ExtractedRendererResources &render_res) {
          render_res.camera.camera_data.id =
              gpu_cam.camera_mat_handle.getBufferIndex();
          render_res.camera.camera_data.offset =
              gpu_cam.camera_mat_handle.getBufferSpace()[0];
        });
      });

  game.world
      .addSystem<Read<GpuTransform>, Read<GpuMesh>, Res<Assets<MeshGpuData>>,
                 ResMut<ExtractedRendererResources>>(
          game.Extract,
          [](auto view, const Assets<MeshGpuData> &mesh_gpu_data_asset,
             ExtractedRendererResources &resources) {
            resources.meshes.clear();

            view.forEach([](auto view, Entity e,
                            const GpuTransform &gpu_transform,
                            const GpuMesh &gpu_mesh,
                            const Assets<MeshGpuData> &mesh_gpu_data_asset,
                            ExtractedRendererResources &resources) {
              const MeshGpuData *gpu_data =
                  mesh_gpu_data_asset.getConstAsset(gpu_mesh.mesh_gpu_data);

              uint32_t vertex_index =
                  gpu_data->vertex_index_buffer_handle.getBufferIndex();
              const auto vertex_space =
                  gpu_data->vertex_index_buffer_handle.getBufferSpace();

              RenderMesh render_mesh{};
              render_mesh.index_count = gpu_data->index_number;
              render_mesh.index_offset = gpu_data->index_byte_offset;
              render_mesh.vertex.id = vertex_index;
              render_mesh.vertex.offset = vertex_space[0];
              render_mesh.pipeline_id = 0;
              render_mesh.transform.id =
                  gpu_transform.transform_handle.getBufferIndex();
              render_mesh.transform.offset =
                  gpu_transform.transform_handle.getBufferSpace()[0];

              resources.meshes.push_back(render_mesh);
            });
          });

  game.world.addSystem<Res<IRenderer *>, ResMut<ExtractedRendererResources>>(
      game.Render,
      [](auto view, IRenderer *r, ExtractedRendererResources &resources) {
        r->draw(resources.camera, resources.meshes, resources.lights);
      });

#pragma region Add Gpu Components

  game.world.addSystem<Added<Read<Camera>>, Read<Transform>,
                       Res<RenderBuffersResource>, ResMut<IRenderer *>,
                       ResMut<Commands>>(
      game.PreRender,
      [](auto view, const RenderBuffersResource &buffers_resource,
         IRenderer *renderer, Commands &cmd) {
        view.forEach([](auto view, Entity e, const Camera &cam,
                        const Transform &transform,
                        const RenderBuffersResource &buffers_resource,
                        IRenderer *renderer, Commands &cmd) {
          GpuCamera gpu_cam{};

          auto transform_buffer_handle =
              buffers_resource.data.at(BufferType::Transform);

          GpuCameraData camera_data{};

          switch (cam.type) {

          case CameraType::PERSPECTIVE:

            camera_data.proj_matrix = Mat4<float>::perspective(
                cam.fov, cam.aspct_ratio, cam.z_near, cam.z_far);

              

            break;
          case CameraType::ORTHOGRAPHIC:

            std::cerr << "Orthographic camera not supported yet\n";
            return;

            break;
          }

          Vec3<float> forward_vector =
              Vec3<float>::forward(); //(0, 0, -1)
          forward_vector = transform.rotation * forward_vector;


              

          camera_data.view_matrix = Mat4<float>::lookAt(
              transform.translation, Vec3<float>(0.0f, 1.0f, 0.0f),
              transform.translation + forward_vector);

            

          gpu_cam.camera_mat_handle = renderer->writeBuffer(
              transform_buffer_handle, &camera_data, sizeof(camera_data),
              UINT32_MAX, VK_ACCESS_SHADER_READ_BIT);

          cmd.push([e, gpu_cam](Ecs *world) {
            world->addComponent<GpuCamera>(e, gpu_cam);
          });
        });
      });

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

  game.world.addSystem<Added<Write<Mesh>>, Res<RenderBuffersResource>,
                       ResMut<IRenderer *>, ResMut<Commands>,
                       Res<Assets<LoadSceneName>>, ResMut<Assets<MeshGpuData>>>(
      game.PreRender, [](auto view, const RenderBuffersResource &buffers_res,
                         IRenderer *renderer, Commands &cmd,
                         const Assets<LoadSceneName> &scene_names,
                         Assets<MeshGpuData> &mesh_gpu_data) {
        //
        view.forEach([](auto view, Entity e, Mesh &m,
                        const RenderBuffersResource &buffers_res,
                        IRenderer *renderer, Commands &cmd,
                        const Assets<LoadSceneName> &scene_names,
                        Assets<MeshGpuData> &mesh_gpu_data_assets) {
          auto transform_buffer = buffers_res.data.at(BufferType::Vertex);

          const auto *load_scene_name =
              scene_names.getConstAsset(m.scene_handle);

          const std::string &scene_path = load_scene_name->scene_path;

          const std::string mesh_data_path =
              scene_path + "meshes/" + std::to_string(m.id) + ".json";
          const std::string mesh_bin_path =
              scene_path + "meshes/" + std::to_string(m.id) + ".bin";

          bool already_registered =
              mesh_gpu_data_assets.isPathRegistered(mesh_data_path);

          GpuMesh gpu_mesh{};

          if (already_registered) {

            std::cout << "Mesh already loaded: " << mesh_data_path << "\n";

            gpu_mesh.mesh_gpu_data =
                mesh_gpu_data_assets.getAssetHandle(mesh_data_path);

          } else {
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

            MeshGpuData gpu_data{};
            gpu_data.index_byte_offset = index_byte_offset;
            gpu_data.index_number = index_number;

            gpu_data.vertex_index_buffer_handle = renderer->writeBuffer(
                transform_buffer, mesh_bin_data.data(),
                mesh_bin_data.size() * sizeof(unsigned char), UINT32_MAX,
                VK_ACCESS_SHADER_READ_BIT);

            gpu_mesh.mesh_gpu_data =
                mesh_gpu_data_assets.registerAsset(gpu_data, mesh_data_path);
          }

          cmd.push([gpu_mesh, e](Ecs *world) {
            world->addComponent<GpuMesh>(e, gpu_mesh);
          });
        });
      });

#pragma endregion
}
