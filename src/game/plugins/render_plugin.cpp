#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/scene_plugin.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"
#include "platform/render/resources.h"
#include <vector>
#include <vulkan/vulkan_core.h>

void RenderPlugin::build(game::Game &game) {

  using namespace vecs;

  std::cout << "Initialize Render Plugin\n";

  IRenderer *r = new Renderer(1280, 720);

  game.world.insertResource<IRenderer *>(r);
  game.world.insertResource<RenderObjectsList>({});

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
      game.PostUpdate,
      [](auto view, vecs::Entity e, IRenderer *r, game::GameData &game_data) {
     

        if (r->shouldRun() == false) {
          game_data.should_run = false;
        }
      });

  game.world.addSystem<vecs::ResMut<Renderer *>, vecs::ResMut<vecs::Commands>>(
      game.OnClose,
      [](auto view, vecs::Entity e, Renderer *r, vecs::Commands &cmd) {
        // Gets only called once

        cmd.push([r](vecs::Ecs *world) { delete r; });
      });

  game.world.addSystem<Read<SceneAssetStructs::MeshRenderer>, ResMut<IRenderer *>, ResMut<RenderObjectsList>>(
      game.PostUpdate,
      [](auto view, Entity e, const SceneAssetStructs::MeshRenderer &mesh_renderer, IRenderer *renderer, RenderObjectsList &objects_list) {

        objects_list.render_objects.clear();

        renderer->draw(objects_list.render_objects);

      });
}
