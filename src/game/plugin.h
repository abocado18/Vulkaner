#pragma once

namespace game {
class Game;
}

struct IPlugin {

  virtual void build(game::Game &game) = 0;
};
