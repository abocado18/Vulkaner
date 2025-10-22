#pragma once

#include "volk.h"

#include "allocator/vk_mem_alloc.h"

#include "GLFW/glfw3.h"

#include "resource_handler.h"

#include<vector>

#define VK_ERROR(result)                              \
    {                                                 \
        if (result != VK_SUCCESS)                     \
        {                                             \
            std::cerr << "Error: " << result << "\n"; \
            abort();                                  \
        }                                             \
    }

namespace render
{


    struct Swapchain
    {
        VkSwapchainKHR swapchain;
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        VkExtent2D extent;
    };

    /// Virtual class that contains all functions used for rendering
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
        VkInstance instance;
        VkPhysicalDevice physical_device;
        VkDevice device;
        VkSurfaceKHR surface;
        Swapchain swapchain;
        
        VmaAllocator vma_allocator;

        VkQueue graphics_queue;
        uint32_t graphics_queue_index;

        VkCommandBuffer primary_command_buffer;
        VkCommandPool command_pool;

        VkSemaphore aquire_image_semaphore;
        std::vector<VkSemaphore> rendering_finished_semaphores;

        VkFence fence;

        GLFWwindow *window;

        void recreateSwapchain(uint32_t width, uint32_t height);

        void render();

        void createSwapchain(bool has_old_swapchain = false, Swapchain old_swapchain = {});
    };
}