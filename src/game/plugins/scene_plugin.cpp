#include "scene_plugin.h"
#include "fastgltf/core.hpp"
#include "fastgltf/types.hpp"
#include "game/ecs/vox_ecs.h"
#include "game/game.h"
#include "platform/render/renderer.h"
#include <filesystem>

using namespace vecs;

void ScenePlugin::build(game::Game &game) {

  game.world.addSystem<ResMut<Renderer *>, Added<Read<LoadScenePlugin>>>(
      game.Update,
      [](auto view, Entity e, Renderer *renderer, LoadScenePlugin &load) {
        LoadedScene loaded_scene;

        fastgltf::Parser parser{};

        constexpr auto gltfOptions =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::AllowDouble |
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::LoadExternalImages;

        std::filesystem::path path = load.load_scene;

        fastgltf::GltfDataBuffer data;
        data.FromPath(path);

        fastgltf::Expected<fastgltf::Asset> res =
            parser.loadGltf(data, path.parent_path(), gltfOptions);

        fastgltf::Asset &gltf = res.get();
      });
}