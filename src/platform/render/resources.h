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
#include <vector>
#include <vulkan/vulkan_core.h>

template <typename T> using RefCounted = std::shared_ptr<T>;

template <typename T> struct vectorHash {
  size_t operator()(const std::vector<T> &vec) const noexcept {
    size_t h = 0;
    for (const auto &x : vec) {

      h ^= std::hash<T>{}(x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
  }
};

struct DescriptorSetLayoutBuilder {
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  void addBinding(uint32_t binding, VkDescriptorType type);
  void clear();

  VkDescriptorSetLayout
  build(VkDevice device, VkShaderStageFlags shader_stages,
        VkDescriptorSetLayoutCreateFlags create_flags = 0);
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

struct Descriptor {
  VkDescriptorSetLayout layout;
  VkDescriptorSet set;
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
  uint32_t current_offset = 0;
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

  const uint64_t idx;

private:
  const RefCounted<Resource> ref;
};

struct CombinedResourceIndexAndDescriptorType {

  size_t idx;
  VkDescriptorType type;

  bool operator==(const CombinedResourceIndexAndDescriptorType &other) const {

    return (idx == other.idx && type == other.type);
  }
};

namespace std {
template <> struct hash<CombinedResourceIndexAndDescriptorType> {
  size_t
  operator()(const CombinedResourceIndexAndDescriptorType &x) const noexcept {
    size_t h1 = std::hash<size_t>{}(x.idx);
    size_t h2 = std::hash<int>{}(static_cast<int>(x.type));
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
  }
};
} // namespace std

class ResourceManager {

public:
  ResourceManager(VkDevice &device, VmaAllocator &allocator);
  ~ResourceManager();

  DeletionQueue<ResourceManager> _deletion_queue;

  ResourceHandle createBuffer(size_t size, VkBufferUsageFlagBits usage_flags,
                              std::optional<std::string> name = std::nullopt);

  // Writes to buffer, returns realignt Offset, if offset is uint32_max, uses current offset of buffer
  uint32_t writeBuffer(ResourceHandle handle, void *data, uint32_t size,
                       uint32_t offset = UINT32_MAX);

  const VkDevice &_device;
  const VmaAllocator &_allocator;

  void runDeletionQueue();

  void removeResource(size_t idx) { resources.erase(idx); }

  Descriptor bindResources(
      std::vector<CombinedResourceIndexAndDescriptorType> &resources_to_bind);

private:
  std::unordered_map<size_t, std::weak_ptr<Resource>> resources;
  std::unordered_map<std::string, size_t> resource_names;

  DescriptorAllocatorGrowable _dynamic_allocator;

  DescriptorWriter writer;

  std::unordered_map<std::vector<CombinedResourceIndexAndDescriptorType>,
                     Descriptor,
                     vectorHash<CombinedResourceIndexAndDescriptorType>>
      bound_descriptor_sets = {};
  std::unordered_map<std::vector<VkDescriptorType>, std::vector<Descriptor>,
                     vectorHash<VkDescriptorType>>
      free_descriptor_sets = {};

  inline size_t getNextId() const {
    static size_t id = 0;
    return id++;
  }
};
