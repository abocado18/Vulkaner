#pragma once

#include "allocator/vk_mem_alloc.h"
#include "platform/render/deletion_queue.h"
#include "volk.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <variant>
#include <vulkan/vulkan_core.h>

template <typename T> using RefCounted = std::shared_ptr<T>;

enum class UpdateFrequency {

  FRAME,
  OBJECT,
  INSTANCE,



};

struct DescriptorSetLayoutBuilder {
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  void addBinding(uint32_t binding, VkDescriptorType type);
  void clear();

  VkDescriptorSetLayout
  build(VkDevice device, VkShaderStageFlags shader_stages,
        VkDescriptorSetLayoutCreateFlags create_flags = 0);
};

struct DescriptorAllocator {

  struct PoolSizeRatio {
    VkDescriptorType type;
    float ratio;
  };

  VkDescriptorPool pool;

  void initPool(VkDevice device, uint32_t max_sets,
                std::span<PoolSizeRatio> pool_ratios);

  void clearDescriptors(VkDevice device);

  void destroyPool(VkDevice device);

  VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {
public:
  struct PoolSizeRatio {
    VkDescriptorType type;
    float ratio;
  };

  void init(VkDevice device, uint32_t initial_sets,
            std::span<PoolSizeRatio> pool_ratios);

  void clearPools(VkDevice device);
  void destroyPools(VkDevice device);

  VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,
                           void *pNext = nullptr);

private:
  VkDescriptorPool getPool(VkDevice device);
  VkDescriptorPool createPool(VkDevice device, uint32_t set_count,
                              std::span<PoolSizeRatio> pool_ratios);

  std::vector<PoolSizeRatio> ratios;
  std::vector<VkDescriptorPool> full_pools;
  std::vector<VkDescriptorPool> ready_pools;
  uint32_t sets_per_pool;
};

struct DescriptorWriter {
  std::deque<VkDescriptorImageInfo> image_infos;
  std::deque<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkWriteDescriptorSet> writes;

  void writeImage(int binding, VkImageView image, VkSampler sampler,
                  VkImageLayout layout, VkDescriptorType type);
  void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                   VkDescriptorType type);

  void clear();
  void updateSet(VkDevice device, VkDescriptorSet set);
};

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
  uint32_t size;
  std::vector<std::array<uint32_t, 2>> free_spaces = {};
};

class ResourceManager;

struct Resource {

  Resource(std::variant<Image, Buffer> v, ResourceManager *manager, size_t idx);
  ~Resource();

  std::variant<Image, Buffer> value;

private:
  ResourceManager *resource_manager;
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

  ResourceHandle createBuffer(size_t size, VkBufferUsageFlagBits usage_flags,
                              std::optional<std::string> name = std::nullopt);

  const VkDevice &_device;
  const VmaAllocator &_allocator;

  void runDeletionQueue();

  void removeResource(size_t idx) { resources.erase(idx); }

private:
  std::unordered_map<size_t, Resource> resources;
  std::unordered_map<std::string, size_t> resource_names;

  DescriptorAllocatorGrowable _dynamic_allocator;

  VkDescriptorSetLayout _global_descriptor_set_layout;
  VkDescriptorSet _global_descriptor_set;

  DescriptorWriter writer;

  inline size_t getNextId() const {
    static size_t id = 0;
    return id++;
  }
};
