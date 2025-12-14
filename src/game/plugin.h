#pragma once

namespace game {
class Game;
}

struct IPlugin {

  virtual ~IPlugin() = default;

  virtual void build(game::Game &game) = 0;
};
