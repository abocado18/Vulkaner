#pragma once

#include "game/plugin.h"

struct SceneBundle {};

class SceneLoader : public IPlugin {
public:
  SceneLoader() = default;
  ~SceneLoader() = default;

private:
  void build(game::Game &game) override;
};