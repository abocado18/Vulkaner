#pragma once

#include "vulkan/vulkan.h"
#include "VkBootstrap.h"

#include "allocator/vk_mem_alloc.h"

#include "GLFW/glfw3.h"

namespace render
{


    class IRenderContext
    {
        public:
        IRenderContext() = default;
        ~IRenderContext() = default;


        virtual inline bool windowShouldClose() const = 0;

        virtual void update() = 0;

    };




    class RenderContext : public IRenderContext
    {
    public:
        RenderContext(uint32_t width, uint32_t height);
        ~RenderContext();



        inline bool windowShouldClose() const override;

        inline void update() override;

    private:

    vkb::Instance instance;
    vkb::PhysicalDevice physical_device;
    vkb::Device device;
    VkSurfaceKHR surface;
    vkb::Swapchain swapchain;

    VkQueue graphics_queue;
    uint32_t graphics_queue_index;


    GLFWwindow *window;



    void recreateSwapchain(uint32_t width, uint32_t height);



    };
}