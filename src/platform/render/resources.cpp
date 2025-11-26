#include "resources.h"
#include "vulkan_macros.h"
#include <memory>
#include <optional>
#include <variant>
#include <vulkan/vulkan_core.h>

Resource::Resource(std::variant<Image, Buffer> v, ResourceManager &manager,
                   size_t idx)
    : resource_manager(manager), value(v), idx(idx) {}

Resource::~Resource() {

  if (std::holds_alternative<Image>(value)) {

    Image img = std::get<Image>(value);

    resource_manager._deletion_queue.pushFunction(
        [img](ResourceManager *manager) {
          vkDestroyImageView(manager->_device, img.view, nullptr);

          vmaDestroyImage(manager->_allocator, img.image, img.allocation);
        });
  } else {

    Buffer buffer = std::get<Buffer>(value);
    resource_manager._deletion_queue.pushFunction(
        [buffer](ResourceManager *manager) {
          vmaDestroyBuffer(manager->_allocator, buffer.buffer,
                           buffer.allocation);
        });
  }

  resource_manager.removeResource(idx);
}

ResourceManager::ResourceManager(VkDevice &device, VmaAllocator &allocator)
    : _device(device), _allocator(allocator) {}

ResourceManager::~ResourceManager() {};

void ResourceManager::runDeletionQueue() { _deletion_queue.flush(this); }

ResourceHandle
ResourceManager::createBuffer(size_t size, VkBufferUsageFlagBits usage_flags, std::optional<std::string> name) {

  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.size = size;
  create_info.usage = usage_flags;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  Buffer new_buffer;

  VK_ERROR(vmaCreateBuffer(_allocator, &create_info, &alloc_info,
                           &new_buffer.buffer, &new_buffer.allocation,
                           &new_buffer.allocation_info),
           "Could not create Buffer");

  size_t id = getNextId();

  Resource new_resource(new_buffer, *this, id);

  resources.insert_or_assign(id, new_resource);

  ResourceHandle new_handle(id, std::make_shared<Resource>(new_resource));

  if(name != std::nullopt)
  {
    resource_names.insert_or_assign(name.value(), id);
  }

  return new_handle;
}