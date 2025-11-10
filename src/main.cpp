#include <iostream>

#include "main.h"

#include "game/game.h"


#include "game/game.h"
#include "game/required_components/assets.h"
#include "platform/renderer/renderer.h"

int main()
{
    

    render::RenderContext render_ctx(1280, 720, SHADER_PATH);

    game::Game gameplay(render_ctx);

    Assets<float> av;

    gameplay.addPlugin(av);

    gameplay.runStartup();

    while (render_ctx.windowShouldClose() == false)
    {


        gameplay.tick();


        render_ctx.update();

        
    }
    

    return 0;
}
