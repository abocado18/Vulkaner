#pragma once

#include <cinttypes>
#include <unordered_map>

#include "game/plugin.h"

template <typename T> struct Assets : plugin::IPlugin {
  std::unordered_map<size_t, T> values;


  void build(game::Game &game) override
  {
    game.runStartup();
  }

  T &operator[](size_t idx) { return values[idx]; }
};

#include "game/game.h"