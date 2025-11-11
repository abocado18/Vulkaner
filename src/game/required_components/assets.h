#pragma once

#include <cinttypes>
#include <cstdint>
#include <unordered_map>

#include "game/plugin.h"

template <typename T> struct Assets : IPlugin {
  std::unordered_map<size_t, T> values;

  void build(game::Game &game) override { 

    



  }



  size_t addAsset(T &asset)
  {
    size_t id = getNextId();
    values[id] = asset;

    return id;
  }

  size_t getNextId() {
    static size_t id = 0;
    return id++;
  }

  T &operator[](size_t idx) { return values[idx]; }
};
