#pragma once

#include "volk.h"

#include <unordered_map>
#include <vector>

namespace resource_handler
{

    enum ImageLayouts : uint32_t
    {
        UNDEFINED = 1 << 0,
        GENERAL = 1 << 1,
        COLOR_ATTACHMENT_OPTIMAL = 1 << 2,
        DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 1 << 3,
        DEPTH_STENCIL_READ_ONLY_OPTIMAL = 1 << 4,
        SHADER_READ_ONLY_OPTIMAL = 1 << 5,
        TRANSFER_SRC_OPTIMAL = 1 << 6,
        TRANSFER_DST_OPTIMAL = 1 << 7,
        PREINITIALIZED = 1 << 8,
        DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1 << 9,
        DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1 << 10,
        PRESENT_SRC_KHR = 1 << 11,
        SHARED_PRESENT_KHR = 1 << 12,
    };

    enum BufferAccessMasks
    {
        INDIRECT_COMMAND_READ_BIT = 1 << 0,
        INDEX_READ_BIT = 1 << 1,
        VERTEX_ATTRIBUTE_READ_BIT = 1 << 2,
        UNIFORM_READ_BIT = 1 << 3,
        SHADER_READ_BIT = 1 << 4,
        SHADER_WRITE_BIT = 1 << 5,
        COLOR_ATTACHMENT_READ_BIT = 1 << 6,
        COLOR_ATTACHMENT_WRITE_BIT = 1 << 7,
        DEPTH_STENCIL_ATTACHMENT_READ_BIT = 1 << 8,
        DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 1 << 9,
        TRANSFER_READ_BIT = 1 << 10,
        TRANSFER_WRITE_BIT = 1 << 11,
        HOST_READ_BIT = 1 << 12,
        HOST_WRITE_BIT = 1 << 13,
        MEMORY_READ_BIT = 1 << 14,
        MEMORY_WRITE_BIT = 1 << 15,

    };

    enum class ResourceType
    {
        BUFFER,
        IMAGE
    };

    struct TransistionData
    {
        ResourceType type;

        union
        {
            BufferAccessMasks access_masks;
            ImageLayouts image_layout;
        } data;
    };

    struct Image
    {
        VkImage image;
        VkImageLayout current_layout;
    };

    struct Resource
    {
        ResourceType type;
    };

    class ResourceHandler
    {
    public:
        ResourceHandler();
        ~ResourceHandler();

        void updateTransistionForLayouts();

    private:
        std::unordered_map<uint32_t, Resource> resources;
    };

}