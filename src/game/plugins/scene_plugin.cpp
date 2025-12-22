#include "scene_plugin.h"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/asset_plugin.h"
#include "game/plugins/default_components_plugin.h"
#include "game/plugins/render_plugin.h"

#include "platform/render/renderer.h"

#include <X11/Xmd.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <iostream>

#include <stdexcept>
#include <string>

#include <unordered_map>

#include <vector>
#include <vulkan/vulkan_core.h>

#include "platform/importer/tinygltf/tiny_gltf.h"
#include "platform/render/resources.h"
#include "platform/render/vertex.h"

#include "ktx.h"

using namespace vecs;

namespace GltfAttrib {
inline constexpr const char *Position = "POSITION";
inline constexpr const char *Normal = "NORMAL";
inline constexpr const char *Tangent = "TANGENT";
inline constexpr const char *TexCoord0 = "TEXCOORD_0";
inline constexpr const char *TexCoord1 = "TEXCOORD_1";
inline constexpr const char *Color0 = "COLOR_0";
inline constexpr const char *Joints0 = "JOINTS_0";
inline constexpr const char *Weights0 = "WEIGHTS_0";
} // namespace GltfAttrib

void ScenePlugin::build(game::Game &game) {

  std::cout << "Initialize Scene Plugin\n";

  Assets<LoadSceneName> scene_name_assets{};

  game.world.insertResource<Assets<LoadSceneName>>(scene_name_assets);

  game.world.addSystem<ResMut<Commands>>(
      game.Startup, [](auto view, Commands &cmd) {
        std::cout << "Load Test Scene\n";
        cmd.push([](Ecs *world) {
          Entity e = world->createEntity();
          LoadSceneName n = {ASSET_PATH "/Opt_Untitled.glb"};
          world->addComponent<LoadSceneName>(e, n);
        });
      });

  game.world.addSystem<ResMut<Commands>, ResMut<IRenderer *>,
                       ResMut<Assets<AssetMesh>>, ResMut<Assets<AssetMaterial>>,
                       ResMut<Assets<AssetImage>>, Res<RenderBuffersResource>,
                       Added<Read<LoadSceneName>>>(
      game.Update, [](auto view, Commands &cmd, IRenderer *renderer,
                      Assets<AssetMesh> &asset_mesh_data_assets,
                      Assets<AssetMaterial> &asset_mat_data_assets,
                      Assets<AssetImage> &asset_image_data_assets,
                      const RenderBuffersResource &render_buffers) {
        //

        view.forEach([](auto view, Entity e, Commands &cmd, IRenderer *renderer,
                        Assets<AssetMesh> &asset_mesh_data_assets,
                        Assets<AssetMaterial> &asset_mat_data_assets,
                        Assets<AssetImage> &asset_image_data_assets,
                        const RenderBuffersResource &render_buffers,
                        const LoadSceneName &scene_name) {
          const std::string scene_path = scene_name.scene_path;

          std::cout << "Load Scene: " << scene_path << "\n";

          cmd.push(
              [e](Ecs *world) { world->removeComponent<LoadSceneName>(e); });

          const auto *render_buffers_ptr = &render_buffers;
          auto *asset_mesh_data_assets_ptr = &asset_mesh_data_assets;
          auto *asset_mat_data_assets_ptr = &asset_mat_data_assets;
          auto *asset_image_data_assets_ptr = &asset_image_data_assets;

          cmd.push([scene_path, render_buffers_ptr, renderer,
                    asset_mesh_data_assets_ptr, asset_mat_data_assets_ptr,
                    asset_image_data_assets_ptr](Ecs *world) {
            tinygltf::Model model;
            tinygltf::TinyGLTF loader;
            std::string err;
            std::string warn;

            bool is_glb = scene_path.find(".glb") != std::string::npos;

            bool ret =
                is_glb
                    ? loader.LoadBinaryFromFile(&model, &err, &warn, scene_path)
                    : loader.LoadASCIIFromFile(&model, &err, &warn, scene_path);

            if (!warn.empty()) {
              std::cout << "glTF warning: " << warn << std::endl;
            }

            if (!err.empty()) {
              std::cout << "glTF error: " << err << std::endl;
            }

            if (!ret) {
              throw std::runtime_error("Failed to load glTF model with path: " +
                                       scene_path);
            }

            std::unordered_map<size_t, AssetHandle<AssetMaterial>>
                index_to_material{};

            std::unordered_map<size_t, AssetHandle<AssetMesh>> index_to_mesh{};

            std::unordered_map<size_t, ResourceHandle> albedo_images{};
            std::unordered_map<size_t, ResourceHandle> normal_images{};

            auto mat_buffer_handle =
                render_buffers_ptr->data.at(BufferType::Material);

            // Cpu Mateirals
            for (size_t i = 0; i < model.materials.size(); i++) {

              const auto &gltf_mat = model.materials[i];

              AssetMaterial material_data{};

              material_data.material_parameters.albedo = {
                  static_cast<float>(
                      gltf_mat.pbrMetallicRoughness.baseColorFactor[0]),
                  static_cast<float>(
                      gltf_mat.pbrMetallicRoughness.baseColorFactor[1]),
                  static_cast<float>(
                      gltf_mat.pbrMetallicRoughness.baseColorFactor[2]),
                  static_cast<float>(
                      gltf_mat.pbrMetallicRoughness.baseColorFactor[3])};

              material_data.material_parameters.metallic =
                  gltf_mat.pbrMetallicRoughness.metallicFactor;
              material_data.material_parameters.roughness =
                  gltf_mat.pbrMetallicRoughness.roughnessFactor;

              material_data.material_parameters.emissive = {
                  static_cast<float>(gltf_mat.emissiveFactor[0]),
                  static_cast<float>(gltf_mat.emissiveFactor[1]),
                  static_cast<float>(gltf_mat.emissiveFactor[2])};

              material_data.buffer_handle = renderer->writeBuffer(
                  mat_buffer_handle, &material_data.material_parameters,
                  sizeof(material_data.material_parameters), UINT32_MAX,
                  VK_ACCESS_SHADER_READ_BIT);

              auto getImageHandle =
                  [&](size_t index, const std::string &scene_path,
                      bool is_normal_map) -> AssetHandle<AssetImage> {
                auto &gltf_image = model.images[index];

                ktxTexture *tex;

                KTX_error_code result = ktxTexture_CreateFromMemory(
                    gltf_image.image.data(), gltf_image.image.size(),
                    KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT |
                        KTX_TEXTURE_CREATE_CHECK_GLTF_BASISU_BIT,
                    &tex);

                if (result != KTX_SUCCESS) {
                  throw std::runtime_error("Invalide Image\n");
                }

                uint32_t width = tex->baseWidth;
                uint32_t height = tex->baseHeight;

                ktx_size_t image_size = ktxTexture_GetImageSize(tex, 0);
                ktx_uint8_t *image_data = ktxTexture_GetData(tex);

                const bool already_loaded =
                    asset_image_data_assets_ptr->isPathRegistered(
                        scene_path + gltf_image.name);

                if (already_loaded) {
                  return asset_image_data_assets_ptr->getAssetHandle(
                      scene_path + gltf_image.name);
                }

                VkFormat desired_format = is_normal_map
                                              ? VK_FORMAT_R8G8B8A8_UNORM
                                              : VK_FORMAT_R8G8B8A8_SRGB;

                std::vector<size_t> mip_lvl_offsets{};

                for (size_t mip_lv_index = 0; mip_lv_index < tex->numLevels;
                     mip_lv_index++) {

                  ktx_size_t offset;
                  KTX_error_code result = ktxTexture_GetImageOffset(
                      tex, mip_lv_index, 0, 0, &offset);

                  mip_lvl_offsets[mip_lv_index] = static_cast<size_t>(offset);
                }

                ResourceHandle img_handle = renderer->createImage(
                    {width, height, 1}, VK_IMAGE_TYPE_2D, desired_format,
                    VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_VIEW_TYPE_2D,
                    VK_IMAGE_ASPECT_COLOR_BIT, tex->numLevels, 1);

                renderer->writeImage(img_handle, image_data, image_size,
                                     {0, 0, 0}, mip_lvl_offsets,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                AssetImage asset_image{};

                asset_image.image_handle = img_handle;
                asset_image.width = width;
                asset_image.height = height;
                asset_image.channels = 0;

                return asset_image_data_assets_ptr->registerAsset(
                    asset_image, scene_path + gltf_image.name);
              };
            }

            // Load Meshes
            for (size_t mesh_index = 0; mesh_index < model.meshes.size();
                 mesh_index++) {
              auto &m = model.meshes[mesh_index];

              const auto &vertex_buffer_handle =
                  render_buffers_ptr->data.at(BufferType::Vertex);

              AssetMesh asset_mesh{};
              asset_mesh.sub_meshes.resize(m.primitives.size());

              for (size_t primitive_index = 0;
                   primitive_index < m.primitives.size(); primitive_index++) {

                auto &p = m.primitives[primitive_index];

                tinygltf::Accessor index_accessor = model.accessors[p.indices];

                tinygltf::BufferView index_buffer_view =
                    model.bufferViews[index_accessor.bufferView];

                tinygltf::Buffer index_buffer =
                    model.buffers[index_buffer_view.buffer];

                const unsigned char *index_data =
                    &index_buffer.data[index_buffer_view.byteOffset +
                                       index_accessor.byteOffset];

                std::vector<uint32_t> indices(index_accessor.count);

                switch (index_accessor.componentType) {

                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {

                  const uint16_t *indices16 =
                      reinterpret_cast<const uint16_t *>(index_data);

                  for (size_t i = 0; i < index_accessor.count; i++) {

                    indices[i] = static_cast<uint32_t>(indices16[i]);
                  }

                  break;
                }

                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {

                  const uint32_t *indices32 =
                      reinterpret_cast<const uint32_t *>(index_data);

                  for (size_t i = 0; i < index_accessor.count; i++) {

                    indices[i] = static_cast<uint32_t>(indices32[i]);
                  }
                  break;
                }

                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {

                  const uint8_t *indices8 =
                      reinterpret_cast<const uint8_t *>(index_data);

                  for (size_t i = 0; i < index_accessor.count; i++) {

                    indices[i] = static_cast<uint32_t>(indices8[i]);
                  }

                  break;
                };
                }

                auto vertex_positions = loadVertexAttributes<float, 3>(
                    model, p, GltfAttrib::Position);

                auto normal_positions = loadVertexAttributes<float, 3>(
                    model, p, GltfAttrib::Normal);

                auto colors = loadVertexAttributes<float, 3>(
                    model, p, GltfAttrib::Color0);

                auto tangents = loadVertexAttributes<float, 4>(
                    model, p, GltfAttrib::Tangent);

                auto uv0s = loadVertexAttributes<float, 2>(
                    model, p, GltfAttrib::TexCoord0);

                std::vector<vertex::Vertex> vertices(vertex_positions.size());

                const bool has_normals = normal_positions.size() != 0;
                const bool has_colors = colors.size() != 0;
                const bool has_tangents = tangents.size() != 0;
                const bool has_uv0 = uv0s.size() != 0;

                for (size_t i = 0; i < vertices.size(); i++) {

                  auto &v = vertices[i];
                  v.position = vertex_positions[i];

                  v.normals = has_normals
                                  ? normal_positions[i]
                                  : std::array<float, 3>{0.0f, 0.0f, 0.0f};

                  v.color = has_colors ? colors[i]
                                       : std::array<float, 3>{1.0f, 1.0f, 1.0f};

                  v.tangent = has_tangents ? tangents[i]
                                           : std::array<float, 4>{0.0f, 0.0f,
                                                                  0.0f, 0.0f};

                  v.tex_coords_0 =
                      has_uv0 ? uv0s[i] : std::array<float, 2>{0.0f, 0.0f};
                }

                asset_mesh.sub_meshes[primitive_index].vertex =
                    renderer->writeBuffer(
                        vertex_buffer_handle, vertices.data(),
                        vertices.size() * sizeof(vertex::Vertex), UINT32_MAX,
                        VK_ACCESS_SHADER_READ_BIT);

                asset_mesh.sub_meshes[primitive_index].index =
                    renderer->writeBuffer(vertex_buffer_handle, indices.data(),
                                          indices.size() * sizeof(uint32_t),
                                          UINT32_MAX,
                                          VK_ACCESS_SHADER_READ_BIT);

                asset_mesh.sub_meshes[primitive_index].index_count =
                    static_cast<uint32_t>(indices.size());
              }

              index_to_mesh[mesh_index] =
                  asset_mesh_data_assets_ptr->registerAsset(
                      asset_mesh, scene_path + m.name);
            }

            for (auto &scene : model.scenes) {
              for (auto &node_index : scene.nodes) {

                auto &node = model.nodes[node_index];

                Entity new_entity = world->createEntity();

                if (!node.name.empty()) {

                  Name name{};
                  name.name = node.name;

                  world->addComponent(new_entity, name);
                }

                if (node.translation.size() != 0 && node.rotation.size() != 0 &&
                    node.scale.size() != 0) {

                  Transform transform{};
                  transform.translation = {(float)node.translation[0],
                                           (float)node.translation[1],
                                           (float)node.translation[2]};

                  transform.rotation = {
                      (float)node.rotation[0], (float)node.rotation[1],
                      (float)node.rotation[2], (float)node.rotation[3]};

                  transform.scale = {(float)node.scale[0], (float)node.scale[1],
                                     (float)node.scale[2]};

                  world->addComponent(new_entity, transform);
                }
              }
            }
          });
        });
      });
}