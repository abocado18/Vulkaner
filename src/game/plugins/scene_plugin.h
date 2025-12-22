#pragma once
#include "game/plugin.h"

#include <array>
#include <optional>
#include <vector>

#include <memory>
#include <string>
#include <unordered_map>

#include "platform/math/math.h"
#include "platform/render/resources.h"

#include <nlohmann/json.hpp>

#include "platform/importer/tinygltf/tiny_gltf.h"

using json = nlohmann::json;

struct LoadSceneName {
  std::string scene_path;
};

class ScenePlugin : public IPlugin {

  void build(game::Game &game) override;
};

template <typename T, size_t N>
std::vector<std::array<T, N>>
loadVertexAttributes(const tinygltf::Model &model,
                     const tinygltf::Primitive &primitive,
                     const std::string &attr_name) {

  auto it = primitive.attributes.find(attr_name);

  if (it == primitive.attributes.end()) {
    return {};
  }

  const tinygltf::Accessor &accessor = model.accessors[it->second];

  const tinygltf::BufferView &buffer_view =
      model.bufferViews[accessor.bufferView];
  const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];

  std::vector<std::array<T, N>> return_data_vector(accessor.count);

  int byte_stride = accessor.ByteStride(buffer_view);

  if (byte_stride == -1) {
    throw std::runtime_error("Invalid glTF bufferView/accessor stride");
  }

  for (size_t i = 0; i < accessor.count; i++) {

    const T *data = reinterpret_cast<const T *>(
        &buffer.data[buffer_view.byteOffset + accessor.byteOffset +
                     i * byte_stride]);

    std::array<T, N> return_data{};

    for (size_t j = 0; j < N; j++) {

      return_data[j] = data[j];
    }

    return_data_vector[i] = return_data;
  };

  return return_data_vector;
}
