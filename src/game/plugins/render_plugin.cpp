#include "render_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"

#include "game/plugins/asset_plugin.h"
#include "platform/render/renderer.h"
#include "platform/render/resources.h"
#include <vulkan/vulkan_core.h>

void RenderPlugin::build(game::Game &game) {

  using namespace vecs;

  std::cout << "Initialize Render Plugin\n";

  IRenderer *r = new VulkanRenderer(1280, 720);

  game.world.insertResource<IRenderer *>(r);

  game.world.insertResource<Assets<AssetMesh>>({});

  game.world.insertResource<Assets<AssetMaterial>>({});

  game.world.insertResource<Assets<AssetImage>>({});

  game.world.insertResource<ExtractedRendererResources>({});

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

#pragma endregion

  game.world.addSystem<ResMut<IRenderer *>, ResMut<ExtractedRendererResources>>(
      game.Render, [](auto view, IRenderer *renderer,
                      ExtractedRendererResources &render_resources) {
        renderer->draw(render_resources.camera, render_resources.meshes,
                       render_resources.lights);
      });
}
