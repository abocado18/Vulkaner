#include "renderer.h"

#include <iostream>
#include <vector>

render::RenderContext::RenderContext(uint32_t width, uint32_t height)
{

    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "Could not init GLFW\n";
        abort();
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(width, height, "Renderer", nullptr, nullptr);

    if (window == nullptr)
    {
        std::cerr << "Could not create window\n";
    }

    uint32_t glfw_instance_extension_count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_instance_extension_count);

    std::vector<const char *> glfw_extensions_list(glfw_extensions, glfw_extensions + glfw_instance_extension_count);

    vkb::InstanceBuilder builder;

    auto inst_ret = builder
                        .set_app_name("Renderer")
                        .request_validation_layers()
                        .use_default_debug_messenger()
                        .enable_extensions(glfw_extensions_list)
                        .build();

    if (!inst_ret)
    {
        std::cerr << "Could not create Instance\n";
        abort();
    }

    instance = inst_ret.value();

    if (glfwCreateWindowSurface(instance.instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        std::cerr << "Could not create surface\n";
        abort();
    }

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_features = {};
    dynamic_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_features.dynamicRendering = VK_TRUE;

    vkb::PhysicalDeviceSelector selector(instance);
    auto phys_ret = selector
                        .set_surface(surface)
                        .set_minimum_version(1, 2)
                        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                        .add_required_extension_features(dynamic_features)
                        .add_required_extension(VK_KHR_MAINTENANCE2_EXTENSION_NAME)
                        .add_required_extension(VK_KHR_MULTIVIEW_EXTENSION_NAME)
                        .add_required_extension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)
                        .add_required_extension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)
                        .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
                        .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)
                        .select();

    if (!phys_ret)
    {
        std::cerr << "No physical device found\n";
        abort();
    }

    physical_device = phys_ret.value();

    vkb::DeviceBuilder device_builder(physical_device);

    auto dev_ret = device_builder.build();

    if (!dev_ret)
    {
        std::cerr << "Could not create logical device\n";
        abort();
    }

    device = dev_ret.value();

    auto graphics_queue_ret = device.get_queue(vkb::QueueType::graphics);

    if (!graphics_queue_ret)
    {
        std::cerr << "No graphics queue\n";
    }

    graphics_queue = graphics_queue_ret.value();
    graphics_queue_index = device.get_queue_index(vkb::QueueType::graphics).value();

    vkb::SwapchainBuilder swapchain_builder(device);

    auto swapchain_ret = swapchain_builder
                             .set_desired_min_image_count(3)
                             .set_desired_format(VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                             .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                             .set_desired_extent(width, height)
                             .build();

    if(!swapchain_ret)
    {
        std::cerr << "Could not create Swapchain\n";
        abort();
    }

    swapchain = swapchain_ret.value();
}

render::RenderContext::~RenderContext()
{
    vkb::destroy_swapchain(swapchain);
    vkb::destroy_device(device);
    vkb::destroy_surface(instance, surface);
    vkb::destroy_instance(instance);

    glfwDestroyWindow(window);

    glfwTerminate();
}