#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/default_components_plugin.h"
#include "game/plugins/registry_plugin.h"
#include "platform/math/math.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"
#include "platform/render/resources.h"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

void RenderPlugin::build(game::Game &game) {

  using namespace vecs;

  std::cout << "Initialize Render Plugin\n";

  IRenderer *r = new VulkanRenderer(1280, 720);

  game.world.insertResource<IRenderer *>(r);

  auto *reg = game.world.getResource<ComponentRegistry>();

  reg->registerComponent<Mesh>("Mesh");

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

  game.world.addSystem<Added<Read<Mesh>>, Res<RenderBuffersResource>,
                       ResMut<IRenderer *>, ResMut<Commands>>(
      game.PreRender, [](auto view, const RenderBuffersResource &buffers_res,
                         IRenderer *renderer, Commands &cmd) {
        view.forEach([](auto view, Entity e, const Mesh &m,
                        const RenderBuffersResource &buffers_res,
                        IRenderer *renderer, Commands &cmd) {
          auto transform_buffer = buffers_res.data.at(BufferType::Vertex);
        });
      });

#pragma endregion
}
