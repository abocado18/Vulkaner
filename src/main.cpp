#include <iostream>

#include "main.h"

#define VULKAN_USE_VALIDATION_LAYERS
#include "platform/renderer/renderer.h"

int main()
{

    render::RenderContext render_ctx(1280, 720);

    while (render_ctx.windowShouldClose() == false)
    {
        render_ctx.update();
    }
    

    return 0;
}
