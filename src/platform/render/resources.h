#pragma once

#include "allocator/vk_mem_alloc.h"
#include "platform/render/deletion_queue.h"
#include "platform/render/render_object.h"
#include "platform/render/vulkan_macros.h"
#include "volk.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

// MAx Number of  Bindings
constexpr size_t MAX_BINDINGS_PER_SET = 16;

template <typename T> using RefCounted = std::shared_ptr<T>;

template <typename T, size_t N> struct arrayHash {
  size_t operator()(const std::array<T, N> &arr) const noexcept {
    size_t h = 0;
    for (const auto &x : arr) {

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
  VmaAllocationInfo allocation_info;
  VkFormat format;

  VkImageAspectFlags aspect_mask;

  VkImageLayout current_layout;

  VkImageUsageFlags image_usage;

  VkExtent3D extent;

  uint32_t mip_map_number;
  uint32_t array_layers;
};

struct Buffer {

  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
  uint32_t size;
  uint32_t current_offset = 0;
  std::vector<std::array<uint32_t, 2>> free_spaces{};

  VkBufferUsageFlags usage_flags;
};

struct SamplerKey {

  SamplerKey() : create_info({VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO}) {}

  VkSamplerCreateInfo create_info{};

  bool operator==(const SamplerKey &other) const noexcept {
    const VkSamplerCreateInfo &t = this->create_info;
    const VkSamplerCreateInfo &o = other.create_info;

    auto float_equal = [](float a, float b) {
      // Compare bitwise to match hashing
      uint32_t bits_a, bits_b;
      std::memcpy(&bits_a, &a, sizeof(a));
      std::memcpy(&bits_b, &b, sizeof(b));
      return bits_a == bits_b;
    };

    return t.addressModeU == o.addressModeU &&
           t.addressModeV == o.addressModeV &&
           t.addressModeW == o.addressModeW && t.magFilter == o.magFilter &&
           t.minFilter == o.minFilter && t.mipmapMode == o.mipmapMode &&
           t.borderColor == o.borderColor && t.compareOp == o.compareOp &&
           t.flags == o.flags && t.anisotropyEnable == o.anisotropyEnable &&
           t.compareEnable == o.compareEnable &&
           t.unnormalizedCoordinates == o.unnormalizedCoordinates &&
           float_equal(t.mipLodBias, o.mipLodBias) &&
           float_equal(t.minLod, o.minLod) && float_equal(t.maxLod, o.maxLod) &&
           float_equal(t.maxAnisotropy, o.maxAnisotropy);
  }
};

namespace std {
template <> struct hash<SamplerKey> {
  size_t operator()(const SamplerKey &key) const noexcept {
    size_t h = 0;

    auto hash_combine = [&h](size_t v) {
      h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
    };

    hash_combine(std::hash<int>{}(static_cast<int>(key.create_info.magFilter)));
    hash_combine(std::hash<int>{}(static_cast<int>(key.create_info.minFilter)));
    hash_combine(
        std::hash<int>{}(static_cast<int>(key.create_info.mipmapMode)));
    hash_combine(
        std::hash<int>{}(static_cast<int>(key.create_info.addressModeU)));
    hash_combine(
        std::hash<int>{}(static_cast<int>(key.create_info.addressModeV)));
    hash_combine(
        std::hash<int>{}(static_cast<int>(key.create_info.addressModeW)));
    hash_combine(
        std::hash<int>{}(static_cast<int>(key.create_info.borderColor)));
    hash_combine(std::hash<int>{}(static_cast<int>(key.create_info.compareOp)));
    hash_combine(std::hash<int>{}(static_cast<int>(key.create_info.flags)));

    hash_combine(std::hash<bool>{}(key.create_info.anisotropyEnable != 0));
    hash_combine(std::hash<bool>{}(key.create_info.compareEnable != 0));
    hash_combine(
        std::hash<bool>{}(key.create_info.unnormalizedCoordinates != 0));

    auto float_hash = [](float f) {
      uint32_t bits;
      static_assert(sizeof(bits) == sizeof(f), "Size mismatch");
      std::memcpy(&bits, &f, sizeof(f));
      return std::hash<uint32_t>{}(bits);
    };

    hash_combine(float_hash(key.create_info.mipLodBias));
    hash_combine(float_hash(key.create_info.minLod));
    hash_combine(float_hash(key.create_info.maxLod));
    hash_combine(float_hash(key.create_info.maxAnisotropy));

    return h;
  }
};
} // namespace std

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

  ResourceHandle() : idx(UINT64_MAX), ref(nullptr) {}

  ResourceHandle(uint64_t idx, RefCounted<Resource> _ref)
      : idx(idx), ref(_ref) {}

  uint64_t idx;

private:
  RefCounted<Resource> ref;
};

struct BufferSpace {

  BufferSpace(std::array<uint32_t, 2> space, ResourceHandle buffer_handle,
              ResourceManager *manager);
  ~BufferSpace();

  std::array<uint32_t, 2> values;

  // Contains Handle to prevent buffer from deletion as long as space in the
  // buffer is used
  ResourceHandle buffer_handle;

private:
  ResourceManager *manager;
};

// Handle that points to a used space in a buffer
struct BufferHandle {

  BufferHandle() = default;
  BufferHandle(size_t buffer_idx, RefCounted<BufferSpace> space)
      : buffer_idx(buffer_idx), buffer_space(space) {}

  std::array<uint32_t, 2> getBufferSpace() const {
    return {buffer_space->values[0], buffer_space->values[1]};
  }

  size_t getBufferIndex() const { return buffer_idx; }

private:
  size_t buffer_idx;
  RefCounted<BufferSpace> buffer_space;
};

struct CombinedResourceIndexAndDescriptorType {

  CombinedResourceIndexAndDescriptorType(size_t idx, VkDescriptorType type,
                                         size_t size, SamplerKey sampler)
      : idx(idx), type(type), size(size), sampler(sampler) {}

  CombinedResourceIndexAndDescriptorType()
      : idx(SIZE_MAX), type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), size(0),
        sampler() {}

  size_t idx;
  VkDescriptorType type;
  size_t size;

  SamplerKey sampler;

  bool operator==(const CombinedResourceIndexAndDescriptorType &other) const {

    return (idx == other.idx && type == other.type) &&
           (sampler == other.sampler);
  }
};

namespace std {
template <> struct hash<CombinedResourceIndexAndDescriptorType> {
  size_t
  operator()(const CombinedResourceIndexAndDescriptorType &x) const noexcept {
    size_t seed = 0;

    auto hash_combine = [&seed](size_t h) {
      seed ^= h + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    };

    hash_combine(std::hash<size_t>{}(x.idx));
    hash_combine(std::hash<int>{}(static_cast<int>(x.type)));
    hash_combine(std::hash<size_t>{}(x.size));

    return seed;
  }
};
} // namespace std

struct ResourceWriteInfo {

  ResourceWriteInfo(uint32_t _target, const std::array<uint32_t, 3> _offset,
                    const Buffer _source_buffer)
      : target_index(_target), target_offset(_offset),
        source_buffer(_source_buffer),
        image_write_data({VK_IMAGE_LAYOUT_UNDEFINED, {}}) {}

  uint32_t target_index{};
  const std::array<uint32_t, 3> target_offset;

  const Buffer source_buffer;

  struct ImageWriteData {
    VkImageLayout new_layout;
    std::vector<MipMapData> mip_lvl_data{};
  } image_write_data;

  struct BufferWriteData {
    VkAccessFlags new_access;
    uint32_t write_size;
    uint32_t write_offset;
  } buffer_write_data;
};

struct TransientBufferKey {
  uint32_t size;
  VkBufferUsageFlags usage_flags;

  VmaAllocationCreateFlags allocation_flags;

  uint32_t queue_family;

  const bool operator==(const TransientBufferKey &other) const {
    return (size == other.size) && (usage_flags == other.usage_flags) &&
           (allocation_flags == other.allocation_flags) &&
           (queue_family == other.queue_family);
  }
};

namespace std {
template <> struct hash<TransientBufferKey> {
  size_t operator()(const TransientBufferKey &key) const noexcept {
    size_t h = 0;
    h ^= std::hash<uint32_t>{}(key.size) + 0x9e3779b9 + (h << 6) + (h >> 2);

    h ^= std::hash<VkBufferUsageFlags>{}(key.usage_flags) + 0x9e3779b9 +
         (h << 6) + (h >> 2);

    h ^= std::hash<VmaAllocationCreateFlags>{}(key.allocation_flags) +
         0x9e3779b9 + (h << 6) + (h >> 2);

    h ^= std::hash<uint32_t>{}(key.queue_family) + 0x9e3779b9 + (h << 6) +
         (h >> 2);

    return h;
  }
};
} // namespace std

struct TransientImageKey {

  VkFormat format{};
  VkExtent3D extent{};
  VkImageType image_type{};
  VkImageUsageFlags image_usage{};
  VkImageAspectFlags aspect_mask{};
  VkImageViewType view_type{};
  uint32_t queue_family{};
  uint32_t mip_levels{};
  uint32_t array_layers{};

  const bool operator==(const TransientImageKey &other) const {
    return format == other.format && extent.width == other.extent.width &&
           extent.height == other.extent.height &&
           extent.depth == other.extent.depth &&
           image_type == other.image_type && image_usage == other.image_usage &&
           view_type == other.view_type && aspect_mask == other.aspect_mask &&
           mip_levels == other.mip_levels &&
           array_layers == other.array_layers &&
           queue_family == other.queue_family;
  };
};

namespace std {
template <> struct hash<TransientImageKey> {
  size_t operator()(const TransientImageKey &key) const noexcept {
    size_t h = 0;
    h ^= std::hash<int>{}(key.format) + 0x9e3779b9 + (h << 6) + (h >> 2);
    //   h ^= std::hash<int>{}(key.image_type) + 0x9e3779b9 + (h << 6) + (h >>
    //   2);
    h ^= std::hash<int>{}(key.image_usage) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(key.view_type) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(key.aspect_mask) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint32_t>{}(key.mip_levels) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    h ^= std::hash<uint32_t>{}(key.array_layers) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    h ^= std::hash<uint32_t>{}(key.extent.width) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    h ^= std::hash<uint32_t>{}(key.extent.height) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    h ^= std::hash<uint32_t>{}(key.extent.depth) + 0x9e3779b9 + (h << 6) +
         (h >> 2);

    h ^= std::hash<uint32_t>{}(key.queue_family) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    return h;
  }
};
} // namespace std

// enu,to see if Transient Res is Buffer or Image due to being stores seperated
enum class TransientKind : uint8_t {
  Buffer,
  Image,
  Undefined,
};

struct CombinedTransientNameAndDescriptorType {

  CombinedTransientNameAndDescriptorType(const std::string &name,
                                         VkDescriptorType type, size_t size,
                                         TransientKind kind, SamplerKey sampler)
      : name(name), type(type), size(size), kind(kind), sampler(sampler) {}

  CombinedTransientNameAndDescriptorType()
      : name(""), type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), size(0),
        kind(TransientKind::Undefined) {}

  std::string name;
  VkDescriptorType type;
  size_t size;

  TransientKind kind{};

  SamplerKey sampler{};

  bool operator==(const CombinedTransientNameAndDescriptorType &other) const {

    return (name == other.name && type == other.type &&
            sampler == other.sampler);
  }
};

namespace std {
template <> struct hash<CombinedTransientNameAndDescriptorType> {
  size_t
  operator()(const CombinedTransientNameAndDescriptorType &x) const noexcept {
    size_t seed = 0;

    auto hash_combine = [&seed](size_t h) {
      seed ^= h + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    };

    hash_combine(std::hash<std::string_view>{}(x.name));
    hash_combine(std::hash<int>{}(static_cast<int>(x.type)));
    hash_combine(std::hash<size_t>{}(x.size));

    return seed;
  }
};
} // namespace std

struct ResourceDescriptorSetKey {
  std::array<CombinedResourceIndexAndDescriptorType, MAX_BINDINGS_PER_SET>
      bindings{};
  uint8_t count_bindings;

  bool operator==(const ResourceDescriptorSetKey &other) const noexcept {

    if (count_bindings != other.count_bindings) {
      return false;
    }

    for (size_t i = 0; i < count_bindings; i++) {

      if (bindings[i] != other.bindings[i]) {
        return false;
      }
    }

    return true;
  }
};

namespace std {
template <> struct hash<ResourceDescriptorSetKey> {
  size_t operator()(const ResourceDescriptorSetKey &key) const noexcept {
    size_t h = 0;

    for (size_t i = 0; i < key.count_bindings; i++) {

      const CombinedResourceIndexAndDescriptorType &v = key.bindings[i];

      h ^= std::hash<size_t>{}(v.idx) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= std::hash<size_t>{}(v.size) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= std::hash<size_t>{}(static_cast<size_t>(v.type)) + 0x9e3779b9 +
           (h << 6) + (h >> 2);
    }

    return h;
  }
};
} // namespace std

struct TransientDescriptorSetKey {
  std::array<CombinedTransientNameAndDescriptorType, MAX_BINDINGS_PER_SET>
      bindings{};
  uint8_t count_bindings;

  bool operator==(const TransientDescriptorSetKey &other) const noexcept {

    if (count_bindings != other.count_bindings) {
      return false;
    }

    for (size_t i = 0; i < count_bindings; i++) {

      if (bindings[i] != other.bindings[i]) {
        return false;
      }
    }

    return true;
  }
};

namespace std {
template <> struct hash<TransientDescriptorSetKey> {
  size_t operator()(const TransientDescriptorSetKey &key) const noexcept {
    size_t h = 0;

    for (size_t i = 0; i < key.count_bindings; i++) {

      const CombinedTransientNameAndDescriptorType &v = key.bindings[i];

      h ^= std::hash<std::string_view>{}(v.name) + 0x9e3779b9 + (h << 6) +
           (h >> 2);
      h ^= std::hash<size_t>{}(v.size) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= std::hash<size_t>{}(static_cast<size_t>(v.type)) + 0x9e3779b9 +
           (h << 6) + (h >> 2);
    }

    return h;
  }
};

}; // namespace std

class ResourceManager {

public:
  ResourceManager(VkDevice &device, VkPhysicalDevice _gpu,
                  VmaAllocator &allocator);
  ~ResourceManager();

  DeletionQueue<ResourceManager> _deletion_queue;

  ResourceHandle createBuffer(size_t size, VkBufferUsageFlags usage_flags,
                              std::optional<std::string> name = std::nullopt);

  // Writes to buffer, returns BufferHandle, if offset is uint32_max, uses
  // current offset of buffer
  BufferHandle
  writeBuffer(ResourceHandle handle, void *data, uint32_t size,
              uint32_t offset = UINT32_MAX,
              VkAccessFlags new_buffer_access_flags = VK_ACCESS_NONE);

  void freeBuffer(ResourceHandle handle, std::array<uint32_t, 2> free_space);

  ResourceHandle createImage(std::array<uint32_t, 3> extent,
                             VkImageType image_type, VkFormat image_format,
                             VkImageUsageFlags image_usage,
                             VkImageViewType view_type,
                             VkImageAspectFlags aspect_mask,
                             uint32_t number_mipmaps, uint32_t array_layers);

  void writeImage(ResourceHandle handle, void *data, uint32_t size,
                  std::array<uint32_t, 3> offset = {0, 0, 0},
                  std::span<MipMapData> mipmap_data = {},
                  VkImageLayout new_layout = VK_IMAGE_LAYOUT_GENERAL);

  const Image &getImage(size_t idx);
  const Buffer &getBuffer(size_t idx);

  const VkDevice &_device;
  const VmaAllocator &_allocator;

  void runDeletionQueue();

  void removeResource(size_t idx) { resources.erase(idx); }

  Descriptor bindResources(
      std::span<CombinedResourceIndexAndDescriptorType> resources_to_bind,
      uint32_t binding_count, VkDescriptorSetLayout layout);

  std::span<ResourceWriteInfo> getWrites() { return writes; }

  void clearWrites() { writes.clear(); }

  void registerTransientImage(const std::string &name,
                              const TransientImageKey &key);

  void transistionTransientImage(const std::string &name, const uint32_t frame,
                                 VkCommandBuffer cmd, VkImageLayout new_layout);

  void resetAllTransientImages(const uint32_t frame);

  Image &getTransientImage(const std::string &name, const uint32_t frame);

  inline const ResourceHandle getPlaceholderImageHandle() const noexcept {
    return placeholder_image_handle;
  }

  Buffer &getTransientBuffer(const std::string &name, const uint32_t frame);

  void registerTransientBuffer(const std::string &name,
                               const TransientBufferKey &key);

  void resetAllTransientBuffers(const uint32_t frame);

  void transistionTransientBuffer(const std::string &name, const uint32_t frame,
                                  VkCommandBuffer cmd, VkQueueFlags queue_flags, VkAccessFlags old_access,
                                  VkAccessFlags new_access, uint32_t size,
                                  uint32_t offset);

  void transistionResourceImage(VkCommandBuffer cmd, size_t resource_idx,
                                VkImageLayout new_layout,
                                uint32_t mip_levels = 1,
                                uint32_t array_layers = 1,
                                uint32_t old_family_queue = UINT32_MAX,
                                uint32_t new_family_queue = UINT32_MAX);

  void transistionResourceBuffer(VkCommandBuffer cmd, VkQueueFlags queue_flags,
                                 size_t resource_idx, VkAccessFlags old_access,
                                 VkAccessFlags new_access, uint32_t size,
                                 uint32_t offset,
                                 uint32_t src_queue_family = UINT32_MAX,
                                 uint32_t dst_queue_family = UINT32_MAX);
  // Used in Transfer Buffer
  void commitWrite(VkCommandBuffer cmd, VkQueueFlags queue_flags, ResourceWriteInfo &write_info,
                   uint32_t old_family_index, uint32_t new_family_index);

  // Used for Qeueue Transfer when using dedicated Transfer Buffer,
  void commitWriteTransmit(VkCommandBuffer cmd, VkQueueFlags queue_flags, ResourceWriteInfo &write_info,
                           uint32_t old_family_index,
                           uint32_t new_family_index);

  Descriptor bindTransient(
      std::span<CombinedTransientNameAndDescriptorType> resources_to_bind,
      uint32_t binding_count, VkDescriptorSetLayout layout,
      const uint32_t frame);

private:
  std::unordered_map<size_t, std::weak_ptr<Resource>> resources{};
  std::unordered_map<std::string, size_t> resource_names{};

  std::unordered_map<SamplerKey, VkSampler> samplers{};

  VkSampler getSampler(SamplerKey &key);

  ResourceHandle placeholder_image_handle;

  VkSampler default_sampler; // Fallback sampler

  // Caches created transient resources based on resource data

  struct TransientImagesCache {
    std::unordered_map<TransientImageKey, std::vector<Image>>
        free_transient_images{};

    std::unordered_map<std::string, std::pair<TransientImageKey, Image>>
        used_transient_images{};

    std::unordered_map<std::string, TransientImageKey>
        transient_virtual_images{};
  };

  struct TransientBuffersCache {
    std::unordered_map<TransientBufferKey, std::vector<Buffer>>
        free_transient_buffers{};

    std::unordered_map<std::string, std::pair<TransientBufferKey, Buffer>>
        used_transient_buffers{};

    std::unordered_map<std::string, TransientBufferKey>
        transient_virtual_buffers{};
  };

  std::array<TransientBuffersCache, FRAMES_IN_FLIGHT> transient_buffers_cache{};

  std::array<TransientImagesCache, FRAMES_IN_FLIGHT> transient_images_cache{};

  DescriptorAllocatorGrowable _dynamic_allocator;

  DescriptorWriter writer;

  struct ResourceDescriptors {

    std::unordered_map<ResourceDescriptorSetKey, Descriptor>
        bound_resource_descriptor_sets = {};
    std::unordered_map<ResourceDescriptorSetKey, std::vector<Descriptor>>
        free_resource_descriptor_sets = {};

  } resource_descriptors;

  struct TransientDescriptors {

    std::unordered_map<TransientDescriptorSetKey, Descriptor>
        bound_resource_descriptor_sets = {};
    std::unordered_map<TransientDescriptorSetKey, std::vector<Descriptor>>
        free_resource_descriptor_sets = {};
  };
  std::array<TransientDescriptors, FRAMES_IN_FLIGHT> transient_descriptors{};

  inline size_t getNextId() const {
    static size_t id = 0;
    return id++;
  }

  VkPhysicalDeviceProperties _properties;

  std::vector<ResourceWriteInfo> writes = {};
};
