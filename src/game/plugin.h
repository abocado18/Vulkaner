#pragma once

// Forward Declaration
namespace game {
struct Game;
};

namespace plugin {

struct IPlugin {

  virtual void build(game::Game &game) = 0;
};

} // namespace plugin