#pragma once

#include "game/plugin.h"
#include <unordered_map>

template <typename T> struct AssetHandle {
  size_t id;
};

template <typename T> struct Assets {
  std::unordered_map<size_t, T> data_map{};

  static inline size_t next_id = 0;

  AssetHandle<T> registerAsset(T&& data) {
    size_t id = next_id++;
    data_map.emplace(id, std::forward<T>(data));
    return { id };
}
};

class AssetPlugin : IPlugin {

  void build(game::Game &game) override;
};