#include "scene_plugin.h"
#include "fastgltf/math.hpp"
#include "fastgltf/types.hpp"
#include "fastgltf/util.hpp"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "game/plugins/asset_plugin.h"
#include "game/plugins/default_components_plugin.h"
#include "game/plugins/render_plugin.h"

#include "ktxvulkan.h"
#include "platform/math/math.h"
#include "platform/render/render_object.h"
#include "platform/render/renderer.h"

#include <X11/Xmd.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <iostream>

#include <stdexcept>
#include <string>

#include <type_traits>
#include <unordered_map>

#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "platform/render/resources.h"
#include "platform/render/vertex.h"

#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"

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

  game.world.addSystem<
      ResMut<Commands>, ResMut<IRenderer *>, ResMut<Assets<AssetMesh>>,
      ResMut<Assets<AssetMaterial>>, ResMut<Assets<AssetImage>>,
      Res<RenderBuffersResource>,
      Added<Read<LoadSceneName>>>(game.Update, [](auto view, Commands &cmd,
                                                  IRenderer *renderer,
                                                  Assets<AssetMesh>
                                                      &asset_mesh_data_assets,
                                                  Assets<AssetMaterial>
                                                      &asset_mat_data_assets,
                                                  Assets<AssetImage>
                                                      &asset_image_data_assets,
                                                  const RenderBuffersResource
                                                      &render_buffers) {
    //

    view.forEach([](auto view, Entity e, Commands &cmd, IRenderer *renderer,
                    Assets<AssetMesh> &asset_mesh_data_assets,
                    Assets<AssetMaterial> &asset_mat_data_assets,
                    Assets<AssetImage> &asset_image_data_assets,
                    const RenderBuffersResource &render_buffers,
                    const LoadSceneName &scene_name) {
      const std::string scene_path = scene_name.scene_path;

      std::cout << "Load Scene: " << scene_path << "\n";

      cmd.push([e](Ecs *world) { world->removeEntity(e); });

      const auto *render_buffers_ptr = &render_buffers;
      auto *asset_mesh_data_assets_ptr = &asset_mesh_data_assets;
      auto *asset_mat_data_assets_ptr = &asset_mat_data_assets;
      auto *asset_image_data_assets_ptr = &asset_image_data_assets;

      // Loading starts here
      cmd.push([scene_path, render_buffers_ptr, renderer,
                asset_mesh_data_assets_ptr, asset_mat_data_assets_ptr,
                asset_image_data_assets_ptr](Ecs *world) {
        std::filesystem::path file_path(scene_path);

        if (!std::filesystem::exists(file_path)) {
          std::cout << "Failed to find " << file_path << '\n';
        }

        std::cout << "Loading " << file_path << '\n';

        const fastgltf::Extensions extensions =
            fastgltf::Extensions::KHR_texture_basisu |
            fastgltf::Extensions::KHR_lights_punctual |
            fastgltf::Extensions::EXT_mesh_gpu_instancing |
            fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::KHR_texture_transform;

        const fastgltf::Options options =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::LoadExternalImages |
            fastgltf::Options::GenerateMeshIndices;

        fastgltf::Parser parser(extensions);

        auto gltf_file = fastgltf::MappedGltfFile::FromPath(file_path);

        if (!gltf_file) {
          std::cerr << "Failed to open glTF file: "
                    << fastgltf::getErrorMessage(gltf_file.error()) << '\n';
          return;
        }

        fastgltf::Expected<fastgltf::Asset> asset =
            parser.loadGltf(gltf_file.get(), file_path.parent_path(), options);

        if (asset.error() != fastgltf::Error::None) {
          std::cerr << "Failed to load glTF: "
                    << fastgltf::getErrorMessage(asset.error()) << '\n';
          return;
        }

        std::unordered_map<size_t, std::vector<AssetHandle<AssetMesh>>>
            index_to_mesh{}; // Submeshes
        std::unordered_map<size_t, AssetHandle<AssetMaterial>>
            index_to_mat{}; // submaterials

        std::unordered_map<size_t, AssetHandle<AssetImage>> index_to_image{};

        std::unordered_map<size_t, Entity> index_to_entity{};

        std::unordered_map<size_t, size_t> child_index_to_parent_index{};

        for (size_t node_index = 0; node_index < asset->nodes.size();
             node_index++) {

          Entity node_entity = world->createEntity();
          index_to_entity[node_index] = node_entity;

          fastgltf::Node &n = asset->nodes[node_index];

          for (auto &child : n.children) {

            child_index_to_parent_index[child] = node_index;
          }
        }

#pragma region Material and Images

        auto material_buffer_handle =
            render_buffers_ptr->data.at(BufferType::Material);

        for (size_t mat_index = 0; mat_index < asset->materials.size();
             mat_index++) {

          auto &gltf_m = asset->materials[mat_index];

          AssetMaterial asset_material{};
          asset_material.material_parameters.albedo =
              std::array<float, 4>{gltf_m.pbrData.baseColorFactor.data()[0],
                                   gltf_m.pbrData.baseColorFactor.data()[1],
                                   gltf_m.pbrData.baseColorFactor.data()[2],
                                   gltf_m.pbrData.baseColorFactor.data()[3]};

          asset_material.material_parameters.metallic =
              gltf_m.pbrData.metallicFactor;

          asset_material.material_parameters.roughness =
              gltf_m.pbrData.roughnessFactor;

          auto writeNewImageHandle = [asset_image_data_assets_ptr, renderer](
                                         ktxTexture2 *tex,
                                         AssetImage &new_image,
                                         const std::string &asset_path,
                                         AssetHandle<AssetImage> &new_handle) {
            if (ktxTexture2_NeedsTranscoding(tex)) {

              ktxTexture2_TranscodeBasis(tex, KTX_TTF_BC7_RGBA,
                                         KTX_TF_HIGH_QUALITY);
            }

            new_image.width = tex->baseWidth;
            new_image.height = tex->baseHeight;
            new_image.number_mipmaps = tex->numLevels;

            ktx_uint8_t *texture_data = tex->pData;

            std::vector<MipMapData> mipmap_data{};
            for (uint32_t i = 0; i < new_image.number_mipmaps; i++) {
              ktx_size_t offset = SIZE_MAX;

              auto mip_offset_result =
                  ktxTexture2_GetImageOffset(tex, i, 0, 0, &offset);

              if (mip_offset_result != KTX_SUCCESS) {

                throw std::runtime_error("failed to get mip map offset!");
              }

              ktx_size_t img_size = ktxTexture_GetImageSize(
                  reinterpret_cast<ktxTexture *>(tex), i);

              mipmap_data.push_back({img_size, offset});
            }

            VkFormat new_format = ktxTexture2_GetVkFormat(tex);

            size_t total_size = 0;

            for (auto &m_data : mipmap_data) {

              size_t end = m_data.size + m_data.offset;
              total_size = std::max(total_size, end);
            }

            new_image.image_handle = renderer->createImage(
                {new_image.width, new_image.height, 1}, VK_IMAGE_TYPE_2D,
                new_format,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT,
                new_image.number_mipmaps, 1);

            renderer->writeImage(new_image.image_handle, texture_data,
                                 total_size, {0, 0, 0}, mipmap_data,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            new_handle = asset_image_data_assets_ptr->registerAsset(new_image,
                                                                    asset_path);
          };

          auto loadImage =
              [&](fastgltf::Asset &asset, fastgltf::Image &image,
                  const std::string &image_path) -> AssetHandle<AssetImage> {
            if (asset_image_data_assets_ptr->isPathRegistered(image_path)) {
              return asset_image_data_assets_ptr->getAssetHandle(image_path);
            }

            AssetImage new_image{};
            AssetHandle<AssetImage> new_handle{};

            std::visit(
                fastgltf::visitor{
                    [](auto &) {},

                    [&](fastgltf::sources::URI &filePath) {
                      assert(filePath.fileByteOffset == 0);
                      assert(filePath.uri.isLocalPath());

                      const std::string path(filePath.uri.path().begin(),
                                             filePath.uri.path().end());

                      ktxTexture2 *tex = nullptr;

                      auto result = ktxTexture2_CreateFromNamedFile(
                          path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                          &tex);

                      if (result != KTX_SUCCESS) {
                        const char *err = ktxErrorString(result);
                        std::cerr << err << std::endl;

                        throw std::runtime_error(
                            "failed to load ktx texture image");
                      }

                      writeNewImageHandle(tex, new_image, image_path,
                                          new_handle);

                      ktxTexture2_Destroy(tex);
                    },

                    [&](fastgltf::sources::Vector &vector) {
                      ktxTexture2 *tex = nullptr;

                      auto result = ktxTexture2_CreateFromMemory(
                          reinterpret_cast<const uint8_t *>(
                              vector.bytes.data()),
                          vector.bytes.size(),
                          KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &tex);

                      if (result != KTX_SUCCESS) {

                        const char *err = ktxErrorString(result);
                        std::cerr << err << std::endl;

                        throw std::runtime_error(
                            "failed to load ktx texture image" +
                            std::string(err));
                      }

                      writeNewImageHandle(tex, new_image, image_path,
                                          new_handle);

                      ktxTexture2_Destroy(tex);
                    },

                    [&](fastgltf::sources::BufferView &view) {
                      auto &bufferView =
                          asset.bufferViews[view.bufferViewIndex];
                      auto &buffer = asset.buffers[bufferView.bufferIndex];

                      std::visit(
                          fastgltf::visitor{
                              [](auto &) {},
                              [&](fastgltf::sources::Vector &vector) {
                                auto &buffer_view =
                                    asset.bufferViews[view.bufferViewIndex];
                                auto &buffer =
                                    asset.buffers[bufferView.bufferIndex];

                                ktxTexture2 *tex = nullptr;

                                auto result = ktxTexture2_CreateFromMemory(
                                    reinterpret_cast<const uint8_t *>(
                                        vector.bytes.data()) +
                                        bufferView.byteOffset,
                                    bufferView.byteLength,
                                    KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                    &tex);

                                if (result != KTX_SUCCESS) {

                                  const char *err = ktxErrorString(result);
                                  std::cerr << err << std::endl;

                                  throw std::runtime_error(
                                      "failed to load ktx texture image");
                                }

                                writeNewImageHandle(tex, new_image, image_path,
                                                    new_handle);

                                ktxTexture2_Destroy(tex);
                              },
                              [&](fastgltf::sources::Array &array) {
                                ktxTexture2 *tex = nullptr;

                                auto result = ktxTexture2_CreateFromMemory(
                                    reinterpret_cast<const uint8_t *>(
                                        array.bytes.data()) +
                                        bufferView.byteOffset,
                                    bufferView.byteLength,
                                    KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                    &tex);

                                if (result != KTX_SUCCESS) {

                                  const char *err = ktxErrorString(result);
                                  std::cerr << err << std::endl;

                                  throw std::runtime_error(
                                      "failed to load ktx texture image");
                                }

                                writeNewImageHandle(tex, new_image, image_path,
                                                    new_handle);

                                ktxTexture2_Destroy(tex);
                              }},
                          buffer.data);
                    }},
                image.data);

            if (new_handle.id == SIZE_MAX) {
              throw std::runtime_error("No Texture loaded\n");
            }

            return new_handle;
          };

          ResourceHandle placeholder_handle =
              renderer->getPlaceholderImageHandle();

          AssetImage placeholder_asset_image{};
          placeholder_asset_image.image_handle = placeholder_handle;
          placeholder_asset_image.width = 1;
          placeholder_asset_image.height = 1;
          placeholder_asset_image.number_mipmaps = 1;

          auto placeholder_asset_handle =
              asset_image_data_assets_ptr->registerAsset(
                  placeholder_asset_image, "Placeholder");

          if (gltf_m.pbrData.baseColorTexture.has_value()) {

            asset_material.material_parameters.use_textures[0] = 1;

            auto &tex = asset->textures[gltf_m.pbrData.baseColorTexture.value()
                                            .textureIndex];

            // Assume ktx2 texture
            assert(tex.basisuImageIndex.has_value());
            auto &img = asset->images[tex.basisuImageIndex.value()];

            AssetHandle<AssetImage> new_handle = loadImage(
                asset.get(), img,
                scene_path +
                    std::to_string(
                        gltf_m.pbrData.baseColorTexture.value().textureIndex));

            asset_material.images.push_back(new_handle);
          } else {

            asset_material.images.push_back(placeholder_asset_handle);
            asset_material.material_parameters.use_textures[0] = 0;
          }

          if (gltf_m.normalTexture.has_value()) {

            asset_material.material_parameters.use_textures[1] = 1;
            auto &tex =
                asset->textures[gltf_m.normalTexture.value().textureIndex];
            assert(tex.basisuImageIndex.has_value());
            auto &img = asset->images[tex.basisuImageIndex.value()];

            AssetHandle<AssetImage> new_handle = loadImage(
                asset.get(), img,
                scene_path + std::to_string(tex.basisuImageIndex.value()));

            asset_material.images.push_back(new_handle);
          } else {
            asset_material.material_parameters.use_textures[1] = 0;
            asset_material.images.push_back(placeholder_asset_handle);
          }

          if (gltf_m.pbrData.metallicRoughnessTexture.has_value()) {

            asset_material.material_parameters.use_textures[2] = 1;

            auto &tex =
                asset->textures[gltf_m.pbrData.metallicRoughnessTexture.value()
                                    .textureIndex];

            // Assume ktx2 texture
            assert(tex.basisuImageIndex.has_value());
            auto &img = asset->images[tex.basisuImageIndex.value()];

            AssetHandle<AssetImage> new_handle = loadImage(
                asset.get(), img,
                scene_path + std::to_string(tex.basisuImageIndex.value()));

            asset_material.images.push_back(new_handle);
          } else {
            asset_material.material_parameters.use_textures[2] = 0;
            asset_material.images.push_back(placeholder_asset_handle);
          }

          asset_material.buffer_handle = renderer->writeBuffer(
              material_buffer_handle, &asset_material.material_parameters,
              sizeof(asset_material.material_parameters), UINT32_MAX,
              VK_ACCESS_SHADER_READ_BIT);

          index_to_mat[mat_index] = asset_mat_data_assets_ptr->registerAsset(
              asset_material, std::to_string(mat_index));
        }

#pragma endregion

#pragma region Meshes

        const auto vertex_buffer_handle =
            render_buffers_ptr->data.at(BufferType::Vertex);

        for (size_t mesh_index = 0; mesh_index < asset->meshes.size();
             mesh_index++) {
          auto &m = asset->meshes[mesh_index];

          for (auto &p : m.primitives) {

            std::vector<uint32_t> indices{};

            std::vector<vertex::Vertex> vertices{};

            if (p.indicesAccessor.has_value()) {

              auto &accessor = asset->accessors[p.indicesAccessor.value()];
              indices.resize(accessor.count);

              switch (accessor.componentType) {

              case fastgltf::ComponentType::UnsignedByte: {

                size_t idx = 0;
                fastgltf::iterateAccessor<uint8_t>(
                    asset.get(), accessor, [&](uint8_t index) {
                      indices[idx++] = static_cast<uint32_t>(index);
                    });

                break;
              }

              case fastgltf::ComponentType::UnsignedShort: {

                size_t idx = 0;
                fastgltf::iterateAccessor<uint16_t>(
                    asset.get(), accessor, [&](uint16_t index) {
                      indices[idx++] = static_cast<uint32_t>(index);
                    });

                break;
              }

              case fastgltf::ComponentType::UnsignedInt: {

                size_t idx = 0;
                fastgltf::iterateAccessor<uint32_t>(
                    asset.get(), accessor,
                    [&](uint32_t index) { indices[idx++] = index; });

                break;
              }

              default:
                break;
              }
            }

            {

              auto *it = p.findAttribute(GltfAttrib::Position);

              if (it && it->accessorIndex < asset->accessors.size()) {
                auto &accessor = asset->accessors[it->accessorIndex];

                if (accessor.bufferViewIndex.has_value()) {
                  vertices.resize(accessor.count);

                  fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                      asset.get(), accessor,
                      [&](fastgltf::math::fvec3 pos, size_t idx) {
                        vertices[idx].position = std::array<float, 3>{
                            pos.data()[0], pos.data()[1], pos.data()[2]};
                      });
                }
              }
            }

            if (vertices.size() == 0 || indices.size() == 0)
              continue;

            {
              auto *it = p.findAttribute(GltfAttrib::Normal);

              if (it && it->accessorIndex < asset->accessors.size()) {
                auto &accessor = asset->accessors[it->accessorIndex];

                if (accessor.bufferViewIndex.has_value()) {

                  fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                      asset.get(), accessor,
                      [&](fastgltf::math::fvec3 n, size_t idx) {
                        vertices[idx].normals = std::array<float, 3>{
                            n.data()[0], n.data()[1], n.data()[2]};
                      });
                }
              }
            }

            {
              auto *it = p.findAttribute(GltfAttrib::Tangent);

              if (it && it->accessorIndex < asset->accessors.size()) {
                auto &accessor = asset->accessors[it->accessorIndex];

                if (accessor.bufferViewIndex.has_value()) {

                  fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                      asset.get(), accessor,
                      [&](fastgltf::math::fvec4 t, size_t idx) {
                        vertices[idx].tangent = std::array<float, 4>{
                            t.data()[0], t.data()[1], t.data()[2], t.data()[3]};
                      });
                }
              }
            }

            {
              auto *it = p.findAttribute(GltfAttrib::Color0);

              if (it && it->accessorIndex < asset->accessors.size()) {
                auto &accessor = asset->accessors[it->accessorIndex];

                if (accessor.bufferViewIndex.has_value()) {

                  fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                      asset.get(), accessor,
                      [&](fastgltf::math::fvec3 c, size_t idx) {
                        vertices[idx].color = std::array<float, 3>{
                            c.data()[0], c.data()[1], c.data()[2]};
                      });
                }
              }
            }

            {
              auto *it = p.findAttribute(GltfAttrib::TexCoord0);

              if (it && it->accessorIndex < asset->accessors.size()) {
                auto &accessor = asset->accessors[it->accessorIndex];

                if (accessor.bufferViewIndex.has_value()) {

                  fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                      asset.get(), accessor,
                      [&](fastgltf::math::fvec2 uv, size_t idx) {
                        vertices[idx].tex_coords_0 =
                            std::array<float, 2>{uv.data()[0], uv.data()[1]};
                      });
                }
              }
            }

            AssetMesh sub_mesh{};

            sub_mesh.index_count = indices.size();
            sub_mesh.vertex = renderer->writeBuffer(
                vertex_buffer_handle, vertices.data(),
                vertices.size() * sizeof(vertex::Vertex), UINT32_MAX,
                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);

            sub_mesh.index =
                renderer->writeBuffer(vertex_buffer_handle, indices.data(),
                                      indices.size() * sizeof(uint32_t),
                                      UINT32_MAX, VK_ACCESS_INDEX_READ_BIT);

            index_to_mesh[mesh_index].push_back(
                asset_mesh_data_assets_ptr->registerAsset(
                    sub_mesh, scene_path + std::to_string(mesh_index)));
          }
        }

#pragma endregion

        for (size_t node_index = 0; node_index < asset->nodes.size();
             node_index++) {

          Entity &node_entity = index_to_entity[node_index];
          fastgltf::Node &node = asset->nodes[node_index];

          std::cout << "Load Node: " << node.name << "\n";

          {
            auto it = child_index_to_parent_index.find(node_index);

            if (it != child_index_to_parent_index.end()) {
              // Has Parent

              ParentComponent parent{index_to_entity[it->second]};

              world->addComponent<ParentComponent>(node_entity, parent);
            }
          }

          {
            TransformComponent transform{};

            std::visit(
                [&](auto &value) {
                  using T = std::decay_t<decltype(value)>;

                  if constexpr (std::is_same_v<T, fastgltf::TRS>) {

                    fastgltf::TRS a = value;

                    transform.translation = {value.translation[0],
                                             value.translation[1],
                                             value.translation[2]};

                    transform.rotation = {
                        value.rotation.x(), value.rotation.y(),
                        value.rotation.z(), value.rotation.w()};

                    transform.scale = {value.scale[0], value.scale[1],
                                       value.scale[2]};

                  } else if constexpr (std::is_same_v<
                                           T, fastgltf::math::fmat4x4>) {

                    transform.translation = {value.col(3).x(), value.col(3).y(),
                                             value.col(3).z()};

                    transform.scale = {length(value.col(0)),
                                       length(value.col(1)),
                                       length(value.col(2))};

                    fastgltf::math::fmat3x3 r;

                    r.col(0) = value.col(0) / transform.scale.x;
                    r.col(1) = value.col(1) / transform.scale.y;
                    r.col(2) = value.col(2) / transform.scale.z;

                    Vec3<float> euler{};

                    if (std::abs(r[2][1]) < 0.9999f) {
                      euler.x = std::asin(-r[2][1]);
                      euler.y = std::atan2(r[2][0], r[2][2]);
                      euler.z = std::atan2(r[0][1], r[1][1]);
                    } else {

                      euler.x = std::asin(-r[2][1]);
                      euler.y = std::atan2(-r[1][0], r[0][0]);
                      euler.z = 0.0f;
                    }

                    transform.rotation = Quat<float>::fromEuler(euler);
                  }
                },
                node.transform);

            world->addComponent<TransformComponent>(node_entity, transform);
          }

          {
            if (node.cameraIndex.has_value()) {

              fastgltf::Camera &gltf_cam =
                  asset->cameras[node.cameraIndex.value()];

              CameraComponent cam_comp{};

              std::visit(
                  [&](auto &value) {
                    using T = std::decay_t<decltype(value)>;

                    if constexpr (std::is_same_v<
                                      T, fastgltf::Camera::Perspective>) {

                      fastgltf::Camera::Perspective &perspective_cam =
                          std::get<fastgltf::Camera::Perspective>(
                              gltf_cam.camera);

                      cam_comp.projection =
                          CameraComponent::Projection::Perspective;

                      cam_comp.aspect = perspective_cam.aspectRatio.value();
                      cam_comp.near_plane = perspective_cam.znear;
                      cam_comp.far_plane = perspective_cam.zfar.value();
                      cam_comp.y_fov = perspective_cam.yfov;

                    }

                    else if constexpr (std::is_same_v<
                                           T, fastgltf::Camera::Orthographic>) {

                      fastgltf::Camera::Orthographic &ortho_cam =
                          std::get<fastgltf::Camera::Orthographic>(
                              gltf_cam.camera);

                      cam_comp.projection = CameraComponent::Projection::Ortho;
                      cam_comp.x_mag = ortho_cam.xmag;
                      cam_comp.y_mag = ortho_cam.ymag;

                      cam_comp.near_plane = ortho_cam.znear;
                      cam_comp.far_plane = ortho_cam.zfar;
                    }
                  },
                  gltf_cam.camera);

              world->addComponent<CameraComponent>(node_entity, cam_comp);
            }
          }

          {

            if (node.meshIndex.has_value()) {

              RenderComponent render_component{};

              std::vector<AssetHandle<AssetMesh>> &sub_meshes =
                  index_to_mesh[node.meshIndex.value()];

              fastgltf::Mesh &gltf_mesh = asset->meshes[node.meshIndex.value()];

              size_t current_mesh_index = 0;
              for (auto &sub_mesh_handle : sub_meshes) {

                RenderInstance current_instance{};

                AssetMesh *current_sub_mesh =
                    asset_mesh_data_assets_ptr->getAsset(sub_mesh_handle);

                assert(current_sub_mesh != nullptr);

                current_instance.mesh = sub_mesh_handle;

                size_t mat_index = gltf_mesh.primitives[current_mesh_index]
                                       .materialIndex.value();

                current_instance.material = index_to_mat[mat_index];

                current_mesh_index++;

                render_component.meshes.push_back(current_instance);
              }

              world->addComponent<RenderComponent>(node_entity,
                                                   render_component);
            }
          }
        }
      });
    });
  });
}