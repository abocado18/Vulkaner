#pragma once

#include "vulkan/vulkan.h"
#include "VkBootstrap.h"

#include "allocator/vk_mem_alloc.h"

#include "GLFW/glfw3.h"

namespace render
{
    class RenderContext
    {
    public:
        RenderContext(uint32_t width, uint32_t height);
        ~RenderContext();

    private:

    vkb::Instance instance;
    vkb::PhysicalDevice physical_device;
    vkb::Device device;
    VkSurfaceKHR surface;
    vkb::Swapchain swapchain;

    VkQueue graphics_queue;
    uint32_t graphics_queue_index;


    GLFWwindow *window;



    };
}