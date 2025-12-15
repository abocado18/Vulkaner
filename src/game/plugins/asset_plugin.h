#pragma once

#include "game/plugin.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

template <typename T> struct AssetHandle {

  AssetHandle<T>() : id(SIZE_MAX), ptr(nullptr) {}

  AssetHandle<T>(size_t id, std::shared_ptr<T> ptr) : id(id), ptr(ptr) {}

  size_t id;

private:
  std::shared_ptr<T> ptr;
};

template <typename T> struct Assets {

public:
  AssetHandle<T> registerAsset(T &&data) {
    size_t id = next_id++;
    std::shared_ptr<T> ptr = std::make_shared<T>(std::forward<T>(data));

    AssetHandle<T> handle(id, ptr);

    data_map[id] = ptr;

    return handle;
  }

  T *getAsset(AssetHandle<T> handle) {

    auto it = data_map.find(handle.id);

    if (it == data_map.end())
      return nullptr;

    std::weak_ptr<T> ref = it->second;

    if (ref.expired())
      return nullptr;

    return ref.lock().get();
  }

  const T *getConstAsset(AssetHandle<T> handle) const {
    auto it = data_map.find(handle.id);

    if (it == data_map.end())
      return nullptr;

    std::weak_ptr<T> ref = it->second;

    if (ref.expired())
      return nullptr;

    return ref.lock().get();
  }

  size_t pathToAssetId(const std::string &path) {

    auto it = path_to_id.find(path);

    if (it != path_to_id.end()) {

      return it->second;
    }

    size_t id = next_id++;
    path_to_id[path] = id;
    return id;
  };

private:
  std::unordered_map<size_t, std::weak_ptr<T>> data_map{};

  std::unordered_map<std::string, size_t> path_to_id{};

  static inline size_t next_id = 0;
};

class AssetPlugin : IPlugin {

  void build(game::Game &game) override;
};