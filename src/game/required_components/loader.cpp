#include "loader.h"

#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/required_components/standard_material.h"
#include "platform/renderer/renderer.h"
#include <cstdint>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

#include "platform/loader/tinygltf/tiny_gltf.h"
#include "platform/renderer/resource_handler.h"

void SceneLoaderPlugin::build(game::Game &game) {

  game.world.addSystem<vecs::Added<vecs::Read<SceneBundle>>>(
      game.Update, [](auto view, vecs::Entity e, const SceneBundle &bundle) {

      });

  game.world.addSystem<vecs::Res<vecs::Removed<SceneBundle>>>(
      game.Update,
      [](auto &view, vecs::Entity e, const vecs::Removed<SceneBundle> &bundle) {

      });

  game.world.addSystem<vecs::Added<vecs::Read<SceneLoader>>,
                       vecs::ResMut<render::RenderContext *>,
                       vecs::ResMut<vecs::Commands>>(
      game.Update,
      [](auto view, vecs::Entity e, const SceneLoader &scene_loader,
         render::RenderContext *render_ctx, vecs::Commands &commands) {
        const std::string &scene_path = scene_loader.scene_path;

        std::unordered_map<uint32_t, vecs::Entity> node_to_entity = {};

        using namespace tinygltf;

        commands.push([render_ctx, scene_path](vecs::Ecs *world) {
          Model model;
          TinyGLTF loader;
          std::string err;
          std::string warn;

          bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, scene_path);
          // bool ret = loader.LoadBinaryFromFile(&model, &err, &warn,
          // filename);
          // // for binary glTF(.glb)

          if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
          }

          if (!err.empty()) {
            printf("Err: %s\n", err.c_str());
          }

          if (!ret) {
            printf("Failed to parse glTF: %s\n", scene_path.c_str());
          }

          std::unordered_map<int, vecs::Entity> texture_entities;
          std::unordered_map<int32_t, std::string> image_usage;

          for (auto &mat : model.materials) {
            if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0)
              image_usage[mat.pbrMetallicRoughness.baseColorTexture.index] =
                  "albedo";

            if (mat.normalTexture.index >= 0)
              image_usage[mat.normalTexture.index] = "normal";

            if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
              image_usage[mat.pbrMetallicRoughness.metallicRoughnessTexture
                              .index] = "metallicRoughness";

            if (mat.occlusionTexture.index >= 0)
              image_usage[mat.occlusionTexture.index] = "occlusion";

            if (mat.emissiveTexture.index >= 0)
              image_usage[mat.emissiveTexture.index] = "emissive";
          }

          for (auto &tex : model.textures) {
            vecs::Entity texture_entity = world->createEntity();

            auto &img = model.images[tex.source];

            VkFormat image_format;

            if (image_usage.at(tex.source) == "albedo" ||
                image_usage.at(tex.source) == "emissive") {
              image_format = VK_FORMAT_R8G8B8A8_SRGB;
            } else {
              image_format = VK_FORMAT_R8G8B8A8_UNORM;
            }

            resource_handler::ResourceHandle tex_handle =
                render_ctx->createImage(img.width, img.height,
                                        VK_IMAGE_USAGE_SAMPLED_BIT,
                                        image_format);

            const uint8_t *pixels =
                reinterpret_cast<const uint8_t *>(img.image.data());

            render_ctx->writeImage(tex_handle, pixels, img.width, img.height);

            world->addComponent(texture_entity, TextureIndex{tex.source});
            world->addComponent(texture_entity, tex_handle);

            texture_entities[tex.source] = texture_entity;
          }

          for (auto &mat : model.materials) {

            // Assume Standard Material for now
            vecs::Entity material_entity = world->createEntity();

            if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0)
              world->addComponent(
                  material_entity,
                  TextureHandle{texture_entities[mat.pbrMetallicRoughness
                                                     .baseColorTexture.index]});

            StandardMaterial material = {};
            material.albedo_color =
                Vec3<float>(mat.pbrMetallicRoughness.baseColorFactor[0],
                            mat.pbrMetallicRoughness.baseColorFactor[1],
                            mat.pbrMetallicRoughness.baseColorFactor[2]);

            material.metallic = mat.pbrMetallicRoughness.metallicFactor;
            material.roughness = mat.pbrMetallicRoughness.roughnessFactor;

            if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0)
              material.albedo_texture =
                  texture_entities[mat.pbrMetallicRoughness.baseColorTexture
                                       .index];

            world->addComponent(material_entity, material);
          }

          for (auto &node : model.nodes) {
          }
        });
      });
}