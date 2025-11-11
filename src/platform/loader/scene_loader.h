#pragma once

#include "platform/renderer/gpu_structs.h"
#include "platform/renderer/resource_handler.h"
#include "platform/renderer/vertex.h"
#include "tinygltf/tiny_gltf.h"

#include "game/ecs/vox_ecs.h"
#include "platform/renderer/material.h"
#include "platform/renderer/renderer.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "game/required_components/assets.h"

namespace gltf_load {

static void loadScene(const std::string &path, vecs::Ecs &world,
                      render::RenderContext &render_ctx) {
  using namespace tinygltf;

  Model model;
  TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
  // bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename); // for
  // binary glTF(.glb)

  if (!warn.empty()) {
    printf("Warn: %s\n", warn.c_str());
  }

  if (!err.empty()) {
    printf("Err: %s\n", err.c_str());
  }

  if (!ret) {
    printf("Failed to parse glTF: %s\n", path.c_str());
  }

#pragma region Load Textures

  std::unordered_map<int32_t, std::string> image_usage = {};

  for (auto &mat : model.materials) {
    if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0)
      image_usage[mat.pbrMetallicRoughness.baseColorTexture.index] = "albedo";

    if (mat.normalTexture.index >= 0)
      image_usage[mat.normalTexture.index] = "normal";

    if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
      image_usage[mat.pbrMetallicRoughness.metallicRoughnessTexture.index] =
          "metallicRoughness";

    if (mat.occlusionTexture.index >= 0)
      image_usage[mat.occlusionTexture.index] = "occlusion";

    if (mat.emissiveTexture.index >= 0)
      image_usage[mat.emissiveTexture.index] = "emissive";
  }

  auto *it = world.getResource<Assets<resource_handler::ResourceHandle>>();

  if (it == nullptr) {

    world.insertResource<Assets<resource_handler::ResourceHandle>>({});

    it = world.getResource<Assets<resource_handler::ResourceHandle>>();
  }

  std::unordered_map<size_t, size_t> source_to_handle = {};

  for (auto &t : model.textures) {
    const auto &img = model.images[t.source];

    const uint8_t *pixels = reinterpret_cast<const uint8_t *>(img.image.data());

    const uint32_t width = static_cast<uint32_t>(img.width);
    const uint32_t height = static_cast<uint32_t>(img.height);

    VkFormat image_format;

    if (image_usage.at(t.source) == "albedo" ||
        image_usage.at(t.source) == "emissive") {
      image_format = VK_FORMAT_R8G8B8A8_SRGB;
    } else {
      image_format = VK_FORMAT_R8G8B8A8_UNORM;
    }

    resource_handler::ResourceHandle handle = render_ctx.createImage(
        width, height, VK_IMAGE_USAGE_SAMPLED_BIT, image_format);

    source_to_handle[t.source] = it->addAsset(handle);
  }

#pragma endregion
}

} // namespace gltf_load
