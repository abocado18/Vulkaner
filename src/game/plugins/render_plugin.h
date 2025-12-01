#pragma once


#include "game/game.h"
#include "game/plugin.h"



struct RenderPlugin : public IPlugin {


    void build(game::Game &game) override;


};