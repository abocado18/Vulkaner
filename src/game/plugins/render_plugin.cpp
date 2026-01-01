#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"

#include "game/plugins/asset_plugin.h"
#include "platform/math/math.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"
#include "platform/render/resources.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <regex.h>
#include <vulkan/vulkan_core.h>

void RenderPlugin::build(game::Game &game) {

  using namespace vecs;

  std::cout << "Initialize Render Plugin\n";

  IRenderer *r = new VulkanRenderer(1920, 1080);

  game.world.insertResource<IRenderer *>(r);

  game.world.insertResource<Assets<AssetMesh>>({});

  game.world.insertResource<Assets<AssetMaterial>>({});

  game.world.insertResource<Assets<AssetImage>>({});

  game.world.insertResource<ExtractedRendererResources>({});

  RenderBuffersResource render_buffers{};

  ResourceHandle vertex_handle =
      r->createBuffer(800'000'000, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
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

  ResourceHandle camera_handle =
      r->createBuffer(5'000, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  render_buffers.data[BufferType::Vertex] = vertex_handle;
  render_buffers.data[BufferType::Transform] = transform_handle;
  render_buffers.data[BufferType::Material] = material_handle;
  render_buffers.data[BufferType::Camera] = camera_handle;

  game.world.insertResource<RenderBuffersResource>(render_buffers);

  // Systems

  game.world.addSystem<vecs::ResMut<IRenderer *>, vecs::ResMut<vecs::Commands>>(
      game.OnClose, [](auto view, IRenderer *r, vecs::Commands &cmd) {
        // Gets only called once

        cmd.push([r](vecs::Ecs *world) { delete r; });
      });

  game.world.addSystem<vecs::ResMut<IRenderer *>, vecs::ResMut<game::GameData>>(
      game.PostRender, [](auto view, IRenderer *r, game::GameData &game_data) {
        if (r->shouldRun() == false) {
          game_data.should_run = false;
        }
      });

#pragma region Add Gpu Components

  game.world.addSystem<Added<Read<CameraComponent>>, ResMut<Commands>,
                       ResMut<IRenderer *>, Res<RenderBuffersResource>>(
      game.PostUpdate, [](auto view, Commands &cmd, IRenderer *renderer,
                          const RenderBuffersResource &render_buffers) {
        view.forEach([](auto view, Entity e,
                        const CameraComponent camera_component, Commands &cmd,
                        IRenderer *renderer,
                        const RenderBuffersResource &render_buffers) {
          GpuCameraComponent gpu_cam{};

          ResourceHandle camera_buffer_handle =
              render_buffers.data.at(BufferType::Camera);

          // Empty, gets updated later
          GpuCameraData gpu_cam_data{};

          gpu_cam.buffer = renderer->writeBuffer(
              camera_buffer_handle, &gpu_cam_data, sizeof(gpu_cam_data),
              UINT32_MAX, VK_ACCESS_SHADER_READ_BIT);

          cmd.push([gpu_cam, e](Ecs *world) {
            world->addComponent<GpuCameraComponent>(e, gpu_cam);
          });
        });
      });

  game.world.addSystem<Added<Read<TransformComponent>>, ResMut<Commands>,
                       ResMut<IRenderer *>, Res<RenderBuffersResource>>(
      game.PostUpdate, [](auto view, Commands &cmd, IRenderer *renderer,
                          const RenderBuffersResource &render_buffers) {
        view.forEach([](auto view, Entity e,
                        const TransformComponent &transform, Commands &cmd,
                        IRenderer *renderer,
                        const RenderBuffersResource &render_buffers) {
          GpuTransformComponent gpu_transform{};

          Mat4<float> transform_matrix = Mat4<float>::createTransformMatrix(
              transform.translation, transform.scale, transform.rotation);

          RenderModelMatrix gpu_model = createRenderModelMatrix(transform_matrix);

          ResourceHandle transform_buffer_handle =
              render_buffers.data.at(BufferType::Transform);

              

          gpu_transform.buffer = renderer->writeBuffer(
              transform_buffer_handle, &gpu_model,
              sizeof(gpu_model), UINT32_MAX, VK_ACCESS_SHADER_READ_BIT);

          cmd.push([e, gpu_transform](Ecs *world) {
            world->addComponent<GpuTransformComponent>(e, gpu_transform);
          });
        });
      });

#pragma endregion

#pragma region Pre render updated

  game.world.addSystem<Write<GpuCameraComponent>, Read<CameraComponent>,
                       Read<TransformComponent>, ResMut<IRenderer *>,
                       Res<RenderBuffersResource>>(
      game.PreRender,
      [](auto view, IRenderer *r, const RenderBuffersResource &res) {
        view.forEach([](auto view, Entity e, GpuCameraComponent &gpu_cam,
                        const CameraComponent &cam,
                        const TransformComponent &transform, IRenderer *r,
                        const RenderBuffersResource &resource) {
          GpuCameraData cam_data{};

          ResourceHandle camera_buffer_handle =
              resource.data.at(BufferType::Camera);

          Vec3<float> forward = transform.rotation * Vec3<float>::forward();
          Vec3<float> up = transform.rotation * Vec3<float>::up();

          cam_data.view_matrix = Mat4<float>::lookAt(
              transform.translation, up, transform.translation + forward);

          cam_data.inv_view_matrix = cam_data.view_matrix.inverse();

          if (cam.projection == CameraComponent::Projection::Perspective) {
            cam_data.proj_matrix = Mat4<float>::perspective(
                cam.y_fov, cam.aspect, cam.near_plane, cam.far_plane);
          } else {

            cam_data.proj_matrix = Mat4<float>::orthographic(
                -cam.x_mag, cam.x_mag, -cam.y_mag, cam.y_mag, cam.near_plane,
                cam.far_plane);
          }

          uint32_t offset = gpu_cam.buffer.getBufferSpace()[0];
          gpu_cam.buffer =
              r->writeBuffer(camera_buffer_handle, &cam_data, sizeof(cam_data),
                             offset, VK_ACCESS_SHADER_READ_BIT);
        });
      });

#pragma endregion

#pragma region Extract

  game.world.addSystem<Read<RenderComponent>, Read<GpuTransformComponent>,
                       ResMut<ExtractedRendererResources>,
                       Res<Assets<AssetMesh>>, Res<Assets<AssetMaterial>>,
                       Res<Assets<AssetImage>>>(
      game.Extract, [](auto view, ExtractedRendererResources &resources,
                       const Assets<AssetMesh> &asset_meshes,
                       const Assets<AssetMaterial> &asset_mats,
                       const Assets<AssetImage> &asset_images) {
        view.forEach([](auto view, Entity e, const RenderComponent &render_comp,
                        const GpuTransformComponent &gpu_transform,
                        ExtractedRendererResources &resources,
                        const Assets<AssetMesh> &asset_meshes,
                        const Assets<AssetMaterial> &asset_mats,
                        const Assets<AssetImage> &asset_images) {
          for (size_t i = 0; i < render_comp.meshes.size(); i++) {

            auto &render_i = render_comp.meshes[i];

            const AssetMesh *mesh = asset_meshes.getConstAsset(render_i.mesh);
            const AssetMaterial *material =
                asset_mats.getConstAsset(render_i.material);

            RenderMesh render_mesh{};
            render_mesh.index_count = mesh->index_count;
            render_mesh.vertex.id = mesh->vertex.getBufferIndex();
            render_mesh.material.id = material->buffer_handle.getBufferIndex();
            render_mesh.material.offset =
                material->buffer_handle.getBufferSpace()[0];
            render_mesh.vertex.offset = mesh->vertex.getBufferSpace()[0];
            render_mesh.index_offset = mesh->index.getBufferSpace()[0];
            render_mesh.pipeline_id = 0;

            for (size_t img_index = 0; img_index < material->images.size();
                 img_index++) {

              const auto &img_handle = material->images[img_index];
              const AssetImage *img = asset_images.getConstAsset(img_handle);

              assert(img);

              render_mesh.images.push_back(
                  static_cast<uint32_t>(img->image_handle.idx));
            }

            render_mesh.transform.id = gpu_transform.buffer.getBufferIndex();
            render_mesh.transform.offset =
                gpu_transform.buffer.getBufferSpace()[0];

            resources.meshes.push_back(render_mesh);
          }
        });
      });

  game.world
      .addSystem<Read<GpuCameraComponent>, ResMut<ExtractedRendererResources>>(
          game.Extract, [](auto view, ExtractedRendererResources &resources) {
            view.forEach([](auto view, Entity e,
                            const GpuCameraComponent &gpu_cam,
                            ExtractedRendererResources &resources) {
              resources.camera.camera_data.id = gpu_cam.buffer.getBufferIndex();
              resources.camera.camera_data.offset =
                  gpu_cam.buffer.getBufferSpace()[0];
            });
          });

#pragma endregion

  game.world.addSystem<ResMut<IRenderer *>, ResMut<ExtractedRendererResources>>(
      game.Render, [](auto view, IRenderer *renderer,
                      ExtractedRendererResources &render_resources) {
        renderer->draw(render_resources.camera, render_resources.meshes,
                       render_resources.lights);
      });

  game.world.addSystem<ResMut<ExtractedRendererResources>>(
      game.PostRender, [](auto view, ExtractedRendererResources &resources) {
        resources.meshes.clear();
        resources.lights.clear();
      });
}
