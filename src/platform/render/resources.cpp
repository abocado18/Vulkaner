#include "resources.h"
#include <variant>
#include <vulkan/vulkan_core.h>

Resource::Resource(std::variant<Image, Buffer> v, ResourceManager &manager)
    : resource_manager(manager), value(v) {}

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
}

ResourceManager::ResourceManager(VkDevice &device, VmaAllocator &allocator)
    : _device(device), _allocator(allocator) {}

ResourceManager::~ResourceManager() {};

void ResourceManager::runDeletionQueue() { _deletion_queue.flush(this); }