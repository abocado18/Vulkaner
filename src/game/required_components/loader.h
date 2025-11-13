#pragma once

#include "game/ecs/vox_ecs.h"
#include "game/plugin.h"
#include <string>

struct SceneBundle {};


//Contains a path to a scene and loads it once added
struct SceneLoader {  
  std::string scene_path;
};


struct TextureIndex
{
  int32_t index;
};


//Reference an Entity that has a texture
struct TextureHandle
{
  vecs::Entity texture_entity;
};

struct MaterialHandle
{
  vecs::Entity material_entitiy;
};

class SceneLoaderPlugin : public IPlugin {
public:
  SceneLoaderPlugin() = default;
  ~SceneLoaderPlugin() = default;

private:
  void build(game::Game &game) override;
};