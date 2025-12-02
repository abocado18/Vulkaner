#pragma once
#include "game/plugin.h"
#include "nlohmann/json_fwd.hpp"

#include <array>
#include <vector>

#include <memory>
#include <string>
#include <unordered_map>

#include "platform/math/math.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;


namespace SceneFormatStructs
{


  using uuid = std::string;

  struct MeshRenderer
  {
    uuid mesh;
    uuid material;
  };


  struct Entity
  {
    uuid id;
    std::string name;
    
    Vec3<float> position;
    Quat<float> rotation;
    Vec3<float> scale;

    std::vector<MeshRenderer> meshes;
  };

  struct Material 
  {
    uuid id;
    std::string name;
    uuid albedo_texture;
    uuid normal_texture;
    uuid metallic_roughness_texture;
    uuid emissive_texture;

    std::array<float, 3> albedo_color;
    float metallic;
    float roughness;
    std::array<float, 3> emissive_color;

    std::string custom_material_data;
  };
}

namespace SceneAssetStructs {


  struct Mesh {
    uint32_t vertex_offset;
    uint32_t index_offset;
    uint32_t index_number;
  };

}



struct LoadSceneName {
  std::string load_scene;
};

struct ScenePlugin;

struct LoadedScene {

};

class ScenePlugin : public IPlugin {

  void build(game::Game &game) override;
};