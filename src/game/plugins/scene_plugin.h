#pragma once
#include "game/plugin.h"
#include "game/required_components/materials.h"
#include "nlohmann/json_fwd.hpp"

#include <array>
#include <optional>
#include <vector>

#include <memory>
#include <string>
#include <unordered_map>

#include "platform/math/math.h"
#include "platform/render/resources.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace SceneFormatStructs {

using uuid = std::string;

struct MeshRenderer {
  uuid mesh;
  uuid material;
};

struct Entity {
  uuid id;
  std::string name;

  Vec3<float> position;
  Quat<float> rotation;
  Vec3<float> scale;

  std::vector<MeshRenderer> meshes;
};

} // namespace SceneFormatStructs

// Used as components
namespace SceneAssetStructs {

enum class TextureType : int8_t {
  Albedo,
  Normal,
  Metallic_Roughness,
  Emissive,
};

// Used In Material
struct Texture {
  TextureType type;
  ResourceHandle handle;
};

// Component
struct Material {

  std::array<Texture, 5> textures{};
  size_t number_textures = 0;

  BufferHandle gpu_material_handle;

  std::shared_ptr<StandardMaterial> cpu_material;
};

// Mesh Component, bas both Material and texture handles
struct Mesh {
  uint32_t vertex_offset;
  uint32_t index_offset;
  uint32_t index_number;

  BufferHandle mesh_gpu_handle;

  Material material;

  bool visible = true;
};

struct MeshRenderer {
  std::vector<Mesh> meshes{};
};

} // namespace SceneAssetStructs

struct LoadSceneName {
  std::string load_scene;
};

struct ScenePlugin;

struct LoadedScene {};

class ScenePlugin : public IPlugin {

  void build(game::Game &game) override;
};