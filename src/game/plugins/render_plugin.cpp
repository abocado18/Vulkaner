#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/scene_plugin.h"
#include "game/required_components/transform.h"
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

  IRenderer *r = new Renderer(1280, 720);

  game.world.insertResource<IRenderer *>(r);

  RenderBuffers render_buffers = {};

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

  game.world.insertResource<RenderBuffers>(render_buffers);

  game.world.addSystem<vecs::ResMut<IRenderer *>, vecs::ResMut<game::GameData>>(
      game.PostUpdate, [](auto view, IRenderer *r, game::GameData &game_data) {
        if (r->shouldRun() == false) {
          game_data.should_run = false;
        }
      });

  game.world.addSystem<vecs::ResMut<Renderer *>, vecs::ResMut<vecs::Commands>>(
      game.OnClose, [](auto view, Renderer *r, vecs::Commands &cmd) {
        // Gets only called once

        cmd.push([r](vecs::Ecs *world) { delete r; });
      });

  game.world.addSystem<Read<SceneAssetStructs::MeshRenderer>,
                       Read<GpuTransform>, ResMut<IRenderer *>,
                       Res<RenderBuffers>>(
      game.PostUpdate,
      [](auto view, IRenderer *renderer, const RenderBuffers &render_buffers) {
        std::vector<RenderObject> render_objects{};

        view.forEach([&](auto view, Entity e,
                         const SceneAssetStructs::MeshRenderer &mesh_renderer,
                         const GpuTransform &transform, IRenderer *renderer,
                         const RenderBuffers &render_buffers) {
          for (const auto &m : mesh_renderer.meshes) {

            RenderObject obj = {};
            obj.index_count = m.index_number;
            obj.index_offset = m.index_offset;
            obj.instance_count = 1;
            obj.vertex_offset = m.vertex_offset;
            obj.vertex_buffer_id = m.mesh_gpu_handle.getBufferIndex();

            obj.transform_buffer_id =
                transform.transform_handle.getBufferIndex();
            obj.transform_offset =
                transform.transform_handle.getBufferSpace()[0];

            obj.material_offset =
                m.material.gpu_material_handle.getBufferSpace()[0];
            obj.material_buffer_id =
                m.material.gpu_material_handle.getBufferIndex();

            obj.pipeline_id = 0; // Only one standard pipeline for now
          }
        });

        renderer->draw(render_objects);
      });
}
