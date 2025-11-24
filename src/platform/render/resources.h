#pragma once

#include "allocator/vk_mem_alloc.h"
#include "platform/render/deletion_queue.h"
#include "volk.h"
#include <memory>
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

  Resource(std::variant<Image, Buffer> v, ResourceManager &manager);
  ~Resource();

  std::variant<Image, Buffer> value;

private:
  ResourceManager &resource_manager;
};

struct ResourceHandle {
  uint64_t idx;

private:
  RefCounted<Resource> ref;
};

class ResourceManager {

public:
  ResourceManager(VkDevice &device, VmaAllocator &allocator);
  ~ResourceManager();

  DeletionQueue<ResourceManager> _deletion_queue;

  const VkDevice &_device;
  const  VmaAllocator &_allocator;


  void runDeletionQueue();

private:
  


};
