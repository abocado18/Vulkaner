#include <iostream>

#include "main.h"


#include "platform/renderer/renderer.h"

int main()
{
    

    render::RenderContext render_ctx(1280, 720, SHADER_PATH);

    while (render_ctx.windowShouldClose() == false)
    {
        render_ctx.update();

        
    }
    

    return 0;
}
