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

  struct MeshRenderer
  {
    std::string mesh;
    std::string material;
  };


  struct Entity
  {
    std::string id;
    std::string name;
    
    Vec3<float> position;
    Quat<float> rotation;
    Vec3<float> scale;

    std::vector<MeshRenderer> meshes;
  };

  struct Material 
  {
    std::string id;
    std::string name;
    std::string albedo_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
    std::string emissive_texture;

    std::array<float, 3> albedo_color;
    float metallic;
    float roughness;
    std::array<float, 3> emissive_color;

    std::string custom_material_data;
  };
}



struct LoadScenePlugin {
  std::string load_scene;
};

struct ScenePlugin;

struct LoadedScene {

};

class ScenePlugin : public IPlugin {

  void build(game::Game &game) override;
};