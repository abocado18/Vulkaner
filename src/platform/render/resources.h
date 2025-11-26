#pragma once

#include "allocator/vk_mem_alloc.h"
#include "platform/render/deletion_queue.h"
#include "volk.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vulkan/vulkan_core.h>

template <typename T> using RefCounted = std::shared_ptr<T>;

struct Image {
  VkImage image;
  VkImageView view;
  VmaAllocation allocation;
  VkFormat format;

  VkImageLayout current_layout;

  VkExtent3D extent;
};

struct Buffer {

  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
};

class ResourceManager;

struct Resource {

  Resource(std::variant<Image, Buffer> v, ResourceManager &manager, size_t idx);
  ~Resource();

  std::variant<Image, Buffer> value;

private:
  ResourceManager &resource_manager;
  size_t idx;
};

struct ResourceHandle {

  ResourceHandle(uint64_t idx, RefCounted<Resource> _ref)
      : idx(idx), ref(_ref) {}

  uint64_t idx;

private:
  RefCounted<Resource> ref;
};

class ResourceManager {

public:
  ResourceManager(VkDevice &device, VmaAllocator &allocator);
  ~ResourceManager();

  DeletionQueue<ResourceManager> _deletion_queue;

  ResourceHandle createBuffer(size_t size, VkBufferUsageFlagBits usage_flags, std::optional<std::string> name = std::nullopt);

  void writeBuffer(size_t size, void *data, );

  const VkDevice &_device;
  const VmaAllocator &_allocator;

  void runDeletionQueue();

  void removeResource(size_t idx) {
    resources.erase(idx);
  }

private:
  std::unordered_map<size_t, Resource> resources;
  std::unordered_map<std::string, size_t> resource_names;

  inline size_t getNextId() const {
    static size_t id = 0;
    return id++;
  }
};
