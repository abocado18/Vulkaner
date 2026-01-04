#include "resources.h"
#include "platform/render/render_object.h"
#include "vulkan_macros.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

#include <cmath>

#include "vk_utils.h"

void DescriptorSetLayoutBuilder::addBinding(uint32_t binding,
                                            VkDescriptorType type) {
  VkDescriptorSetLayoutBinding new_binding = {};
  new_binding.binding = binding;
  new_binding.descriptorCount = 1;
  new_binding.descriptorType = type;

  bindings.push_back(new_binding);
}

void DescriptorSetLayoutBuilder::clear() { bindings.clear(); }

VkDescriptorSetLayout DescriptorSetLayoutBuilder::build(
    VkDevice device, VkShaderStageFlags shader_stages,
    VkDescriptorSetLayoutCreateFlags create_flags) {

  for (auto &b : bindings) {
    b.stageFlags |= shader_stages;
  }

  VkDescriptorSetLayoutCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  create_info.bindingCount = bindings.size();
  create_info.pBindings = bindings.data();
  create_info.flags = create_flags;

  VkDescriptorSetLayout layout;
  VK_CHECK(vkCreateDescriptorSetLayout(device, &create_info, nullptr, &layout),
           "Create Descriptor Set Layout\n");

  return layout;
}

VkDescriptorPool DescriptorAllocatorGrowable::getPool(VkDevice device) {

  VkDescriptorPool new_pool;

  if (ready_pools.size() != 0) {

    new_pool = ready_pools.back();
    ready_pools.pop_back();
  } else {

    new_pool = createPool(device, sets_per_pool, ratios);

    sets_per_pool = sets_per_pool * 1.5f;

    if (sets_per_pool > 4092) {

      sets_per_pool = 4092;
    }
  }

  return new_pool;
}

VkDescriptorPool
DescriptorAllocatorGrowable::createPool(VkDevice device, uint32_t set_count,
                                        std::span<PoolSizeRatio> pool_ratios) {

  std::vector<VkDescriptorPoolSize> pool_sizes;

  pool_sizes.reserve(pool_ratios.size());

  for (PoolSizeRatio ratio : pool_ratios) {

    pool_sizes.push_back(VkDescriptorPoolSize{
        .type = ratio.type,
        .descriptorCount = static_cast<uint32_t>(ratio.ratio * set_count)});
  }

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = 0;
  pool_info.maxSets = set_count;
  pool_info.poolSizeCount = pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();

  VkDescriptorPool new_pool;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &new_pool);

  return new_pool;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t max_sets,
                                       std::span<PoolSizeRatio> pool_ratios) {
  ratios.clear();

  for (auto r : pool_ratios) {
    ratios.push_back(r);
  }

  VkDescriptorPool new_pool = createPool(device, max_sets, pool_ratios);

  sets_per_pool = max_sets * 1.5f;

  ready_pools.push_back(new_pool);
}

void DescriptorAllocatorGrowable::clearPools(VkDevice device) {
  for (auto p : ready_pools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }

  for (auto p : full_pools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }

  full_pools.clear();
}

void DescriptorAllocatorGrowable::destroyPools(VkDevice device) {
  for (auto p : ready_pools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  ready_pools.clear();
  for (auto p : full_pools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  full_pools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(
    VkDevice device, VkDescriptorSetLayout layout, void *p_next) {
  VkDescriptorPool pool_to_use = getPool(device);

  VkDescriptorSetAllocateInfo allocate_info = {};
  allocate_info.pNext = p_next;
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = pool_to_use;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &layout;

  VkDescriptorSet ds;

  VkResult result = vkAllocateDescriptorSets(device, &allocate_info, &ds);

  if (result == VK_ERROR_OUT_OF_POOL_MEMORY ||
      result == VK_ERROR_FRAGMENTED_POOL) {

    full_pools.push_back(pool_to_use);

    pool_to_use = getPool(device);

    VK_ERROR(vkAllocateDescriptorSets(device, &allocate_info, &ds),
             "Allocate Descriptor Set");
  }

  ready_pools.push_back(pool_to_use);

  return ds;
}

void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size,
                                   size_t offset, VkDescriptorType type) {

  VkDescriptorBufferInfo &info =
      buffer_infos.emplace_back(VkDescriptorBufferInfo{
          .buffer = buffer, .offset = offset, .range = size});

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = &info;

  writes.push_back(write);
}

void DescriptorWriter::writeImage(int binding, VkImageView image,
                                  VkSampler sampler, VkImageLayout layout,
                                  VkDescriptorType type) {

  VkDescriptorImageInfo &info = image_infos.emplace_back(VkDescriptorImageInfo{
      .sampler = sampler, .imageView = image, .imageLayout = layout});

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorType = type;
  write.pImageInfo = &info;
  write.descriptorCount = 1;

  writes.push_back(write);
}

void DescriptorWriter::clear() {

  image_infos.clear();
  writes.clear();
  buffer_infos.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set) {
  for (auto &write : writes) {
    write.dstSet = set;
  }

  vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

Resource::Resource(std::variant<Image, Buffer> v, ResourceManager *manager,
                   size_t idx)
    : resource_manager(manager), value(v), idx(idx) {}

Resource::~Resource() {

  std::visit(
      [&](auto &v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, Buffer>) {

          auto buffer_copy = v;

          this->resource_manager->_deletion_queue.pushFunction(
              [buffer_copy](ResourceManager *manager) {
                vmaDestroyBuffer(manager->_allocator, buffer_copy.buffer,
                                 buffer_copy.allocation);
              });

        }

        else if constexpr (std::is_same_v<T, Image>) {

          auto image_copy = v;

          this->resource_manager->_deletion_queue.pushFunction(
              [image_copy](ResourceManager *manager) {
                vkDestroyImageView(manager->_device, image_copy.view, nullptr);

                vmaDestroyImage(manager->_allocator, image_copy.image,
                                image_copy.allocation);
              });
        }
      },
      this->value);

  resource_manager->removeResource(idx);
}

BufferSpace::BufferSpace(std::array<uint32_t, 2> space,
                         ResourceHandle buffer_handle, ResourceManager *manager)
    : values(space), buffer_handle(buffer_handle), manager(manager) {}

BufferSpace::~BufferSpace() {

  auto v0 = values[0];
  auto v1 = values[1];
  auto bh = buffer_handle;

  manager->_deletion_queue.pushFunction([bh, v0, v1](ResourceManager *manager) {
    std::array<uint32_t, 2> free = {v0, v1};
    manager->freeBuffer(bh, free);
  });
}

ResourceManager::ResourceManager(VkDevice &device, VkPhysicalDevice _gpu,
                                 VmaAllocator &allocator)
    : _device(device), _allocator(allocator) {

  DescriptorAllocatorGrowable::PoolSizeRatio ratios[] = {

      {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .ratio = 3.0f},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .ratio = 3.0f},
      {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 4.0f},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 2.0f},

  };

  _dynamic_allocator.init(_device, 50, ratios);

  vkGetPhysicalDeviceProperties(_gpu, &_properties);

  //

  // In ResourceManager Konstruktor nach _device Init
  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.anisotropyEnable = VK_TRUE;
  sampler_info.maxAnisotropy = 16.0f;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  VK_ERROR(vkCreateSampler(_device, &sampler_info, nullptr, &default_sampler),
           "Create default sampler");

  // Create Placeholder Image Resource

  this->placeholder_image_handle = this->createImage(
      {1, 1, 1}, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);

  assert(placeholder_image_handle.idx == 0);

  std::array<uint8_t, 4> tex_data = {255, 255, 255, 255};
  std::array<MipMapData, 1> mipmap_data = {{sizeof(tex_data), 0}};

  this->writeImage(placeholder_image_handle, &tex_data, sizeof(tex_data),
                   {0, 0, 0}, mipmap_data,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

ResourceManager::~ResourceManager() {

  runDeletionQueue();

  vkDestroySampler(_device, default_sampler, nullptr);

  _dynamic_allocator.destroyPools(_device);

  for (auto &w : writes) {
    vmaDestroyBuffer(_allocator, w.source_buffer.buffer,
                     w.source_buffer.allocation);
  }

  for (auto &r : resources) {

    if (r.second.expired())
      continue;

    Resource *res = r.second.lock().get();

    if (std::holds_alternative<Image>(res->value)) {

      auto &img = std::get<Image>(res->value);
      vkDestroyImageView(_device, img.view, nullptr);
      vmaDestroyImage(_allocator, img.image, img.allocation);

    }

    else {

      auto &buf = std::get<Buffer>(res->value);
      vmaDestroyBuffer(_allocator, buf.buffer, buf.allocation);
    }
  }

  resources.clear();

  for (size_t i = 0; i < transient_images_cache.size(); i++) {
    resetAllTransientImages(i);
  }

  for (auto &cache : transient_images_cache) {
    for (auto &pair : cache.free_transient_images) {
      auto &images = pair.second;

      for (auto &img : images) {
        vkDestroyImageView(_device, img.view, nullptr);

        vmaDestroyImage(_allocator, img.image, img.allocation);
      }
    }
  }
};

void ResourceManager::runDeletionQueue() { _deletion_queue.flush(this); }

ResourceHandle ResourceManager::createBuffer(size_t size,
                                             VkBufferUsageFlags usage_flags,
                                             std::optional<std::string> name) {

  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.size = size;
  create_info.usage = usage_flags;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  Buffer new_buffer;

  VK_ERROR(vmaCreateBuffer(_allocator, &create_info, &alloc_info,
                           &new_buffer.buffer, &new_buffer.allocation,
                           &new_buffer.allocation_info),
           "Could not create Buffer");

  size_t id = getNextId();

  new_buffer.size = size;
  new_buffer.current_offset = 0;
  new_buffer.free_spaces.clear();
  new_buffer.usage_flags = usage_flags;

  RefCounted<Resource> ref_resource =
      std::make_shared<Resource>(new_buffer, this, id);

  std::weak_ptr<Resource> weak_r = ref_resource;

  resources.insert_or_assign(id, weak_r);

  ResourceHandle new_handle(id, ref_resource);

  if (name.has_value()) {
    resource_names.insert_or_assign(name.value(), id);
  }

  return new_handle;
}

Descriptor ResourceManager::bindResources(
    std::span<CombinedResourceIndexAndDescriptorType> resources_to_bind,
    uint32_t binding_count, VkDescriptorSetLayout layout) {

  Descriptor set;

  assert(binding_count <= MAX_BINDINGS_PER_SET);
  assert(resources_to_bind.size() == binding_count);

  ResourceDescriptorSetKey key{};

  std::copy(resources_to_bind.begin(),
            resources_to_bind.begin() + binding_count, key.bindings.begin());

  key.count_bindings = binding_count;

  {

    auto it = resource_descriptors.bound_resource_descriptor_sets.find(key);

    if (it != resource_descriptors.bound_resource_descriptor_sets.end()) {

      // Already bound
      set = it->second;

      return set;
    }
  }

  ResourceDescriptorSetKey searched_types_key{};

  {
    size_t i = 0;
    for (const auto &p : resources_to_bind) {

      const size_t idx = p.idx;
      const VkDescriptorType type = p.type;

      auto r = resources[idx].lock();

      if (!r) {
        // Resource does not exist anymore
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
      }

      CombinedResourceIndexAndDescriptorType free_searched_type{};
      free_searched_type.type = type;
      free_searched_type.idx = SIZE_MAX;
      free_searched_type.size = SIZE_MAX;

      searched_types_key.bindings[i++] = free_searched_type;
    }
  }

  auto it = resource_descriptors.free_resource_descriptor_sets.find(
      searched_types_key);

  if (it != resource_descriptors.free_resource_descriptor_sets.end()) {

    set = it->second[it->second.size() - 1];
    it->second.pop_back();

  } else {

    set.layout = layout;
    set.set = _dynamic_allocator.allocate(_device, set.layout);
  }

  std::array<VkWriteDescriptorSet, MAX_BINDINGS_PER_SET> writes;

  DescriptorWriter writer = {};

  for (size_t i = 0; i < binding_count; i++) {

    auto &write = writes[i];

    Resource *r = resources[resources_to_bind[i].idx].lock().get();

    if (std::holds_alternative<Image>(r->value)) {

      Image &img = std::get<Image>(r->value);

      writer.writeImage(
          i, img.view, getSampler(searched_types_key.bindings[i].sampler),
          img.current_layout, searched_types_key.bindings[i].type);

    } else {
      Buffer &buf = std::get<Buffer>(r->value);

      writer.writeBuffer(i, buf.buffer, resources_to_bind[i].size, 0,
                         searched_types_key.bindings[i].type);
    }
  }

  writer.updateSet(_device, set.set);

  resource_descriptors.bound_resource_descriptor_sets[key] = set;

  return set;
}

BufferHandle ResourceManager::writeBuffer(ResourceHandle handle, void *data,
                                          uint32_t size, uint32_t offset,
                                          VkAccessFlags new_access) {

  auto weak_ref = resources.at(handle.idx);

  if (weak_ref.expired()) {
    std::cout << "Resource does not exist anymore\n";
    return {};
  }

  Resource &ref = *weak_ref.lock().get();

  if (std::holds_alternative<Buffer>(ref.value) == false) {
    std::cout << "Resource is not a buffer\n";

    return {};
  }

  Buffer &buf_ref = std::get<Buffer>(ref.value);

  Buffer staging_buffer;

  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = size;
  create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VK_ERROR(vmaCreateBuffer(_allocator, &create_info, &alloc_info,
                           &staging_buffer.buffer, &staging_buffer.allocation,
                           &staging_buffer.allocation_info),
           "");

  staging_buffer.current_offset = 0;
  staging_buffer.size = size;

  std::memcpy(staging_buffer.allocation_info.pMappedData, data, size);

  uint32_t allocated_offset = UINT32_MAX;

  if (offset == UINT32_MAX) {

    // Calculate aligned offset

    // Buffer is either always bound as Uniform or Storage, not both for
    // simplicity
    bool is_uniform = buf_ref.usage_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bool is_storage = buf_ref.usage_flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (buf_ref.free_spaces.empty())
      buf_ref.free_spaces.push_back({0, buf_ref.size});

    for (size_t i = 0; i < buf_ref.free_spaces.size(); i++) {

      uint32_t range_start = buf_ref.free_spaces[i][0];
      uint32_t range_end = buf_ref.free_spaces[i][1];

      uint32_t alignment = 1;

      if (is_uniform)
        alignment = _properties.limits.minUniformBufferOffsetAlignment;

      else if (is_storage)
        alignment = _properties.limits.minStorageBufferOffsetAlignment;

      uint32_t aligned = (range_start + alignment - 1) &
                         ~(alignment - 1); // Align to correct offset

      if (aligned + size > range_end) {
        continue; // Not enough space lol
      }

      allocated_offset = aligned;

      if (aligned == range_start) {
        buf_ref.free_spaces[i][0] += size;
      } else {
        uint32_t old_end = range_end;

        buf_ref.free_spaces[i][1] = aligned;

        if (aligned + size < old_end) {
          buf_ref.free_spaces.push_back({aligned + size, old_end});

          std::sort(buf_ref.free_spaces.begin(), buf_ref.free_spaces.end());
        }
      }

      if (buf_ref.free_spaces[i][0] >= buf_ref.free_spaces[i][1]) {
        buf_ref.free_spaces[i] = buf_ref.free_spaces.back();
        buf_ref.free_spaces.pop_back();
      }

      break;
    }

  } else {

    // Assumes correct alignment
    allocated_offset = offset;
  }

  if (allocated_offset == UINT32_MAX) {
    // Not enough space
    vmaDestroyBuffer(_allocator, buf_ref.buffer, buf_ref.allocation);

    // Implement grpwable buffer later, for now throw error
    throw std::runtime_error("Not enough space in buffer");
  }

  ResourceWriteInfo info(handle.idx, {allocated_offset}, staging_buffer);

  info.buffer_write_data = {new_access, size, allocated_offset};

  writes.push_back(info);

  std::array<uint32_t, 2> free_array_space = {allocated_offset,
                                              allocated_offset + size};

  RefCounted<BufferSpace> used_buffer_space =
      std::make_shared<BufferSpace>(free_array_space, handle, this);

  BufferHandle buffer_handle(handle.idx, used_buffer_space);

  return buffer_handle;
}

void ResourceManager::freeBuffer(ResourceHandle handle,
                                 std::array<uint32_t, 2> free_space) {

  std::variant<Image, Buffer> &res = resources.at(handle.idx).lock()->value;

  if (std::holds_alternative<Image>(res))
    return;

  Buffer &buf = std::get<Buffer>(res);

  uint32_t start = free_space[0];
  uint32_t end = free_space[1];

  buf.free_spaces.push_back({start, end});

  std::sort(buf.free_spaces.begin(), buf.free_spaces.end());

  std::vector<std::array<uint32_t, 2>> merged;

  if (!buf.free_spaces.empty()) {
    merged.push_back(buf.free_spaces[0]);
  }

  for (size_t i = 1; i < buf.free_spaces.size(); i++) {
    auto &last = merged.back();
    auto &current = buf.free_spaces[i];

    if (current[0] <= last[1]) {

      last[1] = std::max(last[1], current[1]);
    } else {
      merged.push_back(current);
    }
  }

  buf.free_spaces = std::move(merged);
}

ResourceHandle ResourceManager::createImage(
    std::array<uint32_t, 3> extent, VkImageType image_type,
    VkFormat image_format, VkImageUsageFlags image_usage,
    VkImageViewType view_type, VkImageAspectFlags aspect_mask,
    uint32_t number_mipmaps, uint32_t array_layers) {

  VkImageCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create_info.imageType = image_type;
  create_info.extent = {extent[0], extent[1], extent[2]};
  create_info.mipLevels = number_mipmaps;

  create_info.arrayLayers = array_layers;
  create_info.format = image_format;
  create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  create_info.usage = image_usage;
  create_info.samples = VK_SAMPLE_COUNT_1_BIT;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  alloc_info.priority = 1.0f;

  Image image;

  VK_ERROR(vmaCreateImage(_allocator, &create_info, &alloc_info, &image.image,
                          &image.allocation, &image.allocation_info),
           "Create Image");

  image.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  image.extent = create_info.extent;
  image.format = image_format;
  image.mip_map_number = create_info.mipLevels;
  image.array_layers = create_info.arrayLayers;
  image.aspect_mask = aspect_mask;
  image.image_usage = image_usage;

  {
    VkImageViewCreateInfo view_create_info = vk_utils::imageViewCreateInfo(
        image_format, image.image, aspect_mask, view_type);

    VK_ERROR(
        vkCreateImageView(_device, &view_create_info, nullptr, &image.view),
        "Create View");
  }

  uint32_t id = getNextId();

  RefCounted<Resource> ref_resource =
      std::make_shared<Resource>(image, this, id);

  std::weak_ptr<Resource> weak_r = ref_resource;

  resources.insert_or_assign(id, weak_r);

  ResourceHandle new_handle(id, ref_resource);

  return new_handle;
}

void ResourceManager::writeImage(ResourceHandle handle, void *data,
                                 uint32_t size, std::array<uint32_t, 3> offset,
                                 std::span<MipMapData> mipmap_data,
                                 VkImageLayout new_layout) {

  {

    auto weak_ref = resources.at(handle.idx);

    if (weak_ref.expired()) {
      std::cout << "Resource does not exist anymore\n";
      return;
    }

    Resource &ref = *weak_ref.lock().get();

    if (std::holds_alternative<Image>(ref.value) == false) {
      std::cout << "Resource is not am Image\n";

      return;
    }
  }

  Buffer staging_buffer;

  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = size;
  create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VK_ERROR(vmaCreateBuffer(_allocator, &create_info, &alloc_info,
                           &staging_buffer.buffer, &staging_buffer.allocation,
                           &staging_buffer.allocation_info),
           "");

  staging_buffer.current_offset = 0;
  staging_buffer.size = size;

  std::memcpy(staging_buffer.allocation_info.pMappedData, data, size);

  ResourceWriteInfo write_info(handle.idx, offset, staging_buffer);
  write_info.image_write_data.new_layout = new_layout;
  write_info.image_write_data.mip_lvl_data.reserve(mipmap_data.size());

  for (auto &m : mipmap_data) {
    write_info.image_write_data.mip_lvl_data.push_back(m);
  }

  writes.push_back(write_info);
}

const Buffer &ResourceManager::getBuffer(size_t idx) {

  auto weak_r = resources.at(idx);

  if (weak_r.expired()) {
    std::cerr << "No valid Buffer, is expired\n";
    std::abort();
  }

  const Resource &resource = *weak_r.lock().get();

  if (std::holds_alternative<Buffer>(resource.value)) {

    return std::get<Buffer>(resource.value);
  }

  std::cerr << "Not a Buffer\n";
  std::abort();
}

const Image &ResourceManager::getImage(size_t idx) {
  auto weak_r = resources.at(idx);

  if (weak_r.expired()) {
    std::cerr << "No valid Image, is expired\n";
    std::abort();
  }

  const Resource &resource = *weak_r.lock().get();

  if (std::holds_alternative<Image>(resource.value)) {

    return std::get<Image>(resource.value);
  }

  std::cerr << "Not a Buffer\n";
  std::abort();
}

Image &ResourceManager::getTransientImage(const std::string &name,
                                          const uint32_t frame) {

  auto &cache = transient_images_cache[frame];

  

  auto it_used = cache.used_transient_images.find(name);
  if (it_used != cache.used_transient_images.end()) {
    return it_used->second.second;
  }

  TransientImageKey &key =
      cache.transient_virtual_images.at(name);

  auto &images = cache.free_transient_images[key];

  size_t i = 0;
  bool found = false;

  for (auto &img : images) {

    if ((img.image_usage & key.image_usage) == key.image_usage) {

      found = true;
      break;
    }

    i++;
  }

  if (found) {

    Image &found_image = images[i];

    images[i] = images.back();
    images.pop_back();

    cache.used_transient_images[name] = {
        key, std::move(found_image)};

    return cache.used_transient_images[name].second;
  }

  VkImageCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

  create_info.imageType = key.image_type;
  create_info.extent = key.extent;
  create_info.mipLevels = key.mip_levels;

  create_info.arrayLayers = key.array_layers;
  create_info.format = key.format;
  create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  create_info.usage = key.image_usage;
  create_info.samples = VK_SAMPLE_COUNT_1_BIT;

  create_info.queueFamilyIndexCount = 1;
  create_info.pQueueFamilyIndices = &key.queue_family;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  alloc_info.priority = 1.0f;

  Image new_image;

  VK_ERROR(vmaCreateImage(_allocator, &create_info, &alloc_info,
                          &new_image.image, &new_image.allocation,
                          &new_image.allocation_info),
           "Create Image");

  new_image.image_usage = key.image_usage;
  new_image.aspect_mask = key.aspect_mask;
  new_image.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  new_image.extent = key.extent;
  new_image.format = key.format;
  new_image.mip_map_number = key.mip_levels;
  new_image.array_layers = key.array_layers;

  {
    VkImageViewCreateInfo view_create_info = vk_utils::imageViewCreateInfo(
        key.format, new_image.image, key.aspect_mask, key.view_type);

    VK_ERROR(
        vkCreateImageView(_device, &view_create_info, nullptr, &new_image.view),
        "Create View");
  }

  cache.used_transient_images[name] = {key, new_image};

  return cache.used_transient_images[name].second;
}

Buffer &ResourceManager::getTransientBuffer(const std::string &name,
                                            const uint32_t frame) {

  TransientBufferKey &key =
      transient_buffers_cache[frame].transient_virtual_buffers.at(name);

  auto &buffers = transient_buffers_cache[frame].free_transient_buffers[key];

  size_t i = 0;
  bool found = false;

  for (auto &buf : buffers) {

    if ((buf.usage_flags & key.usage_flags) == key.usage_flags) {

      found = true;
      break;
    }

    i++;
  }

  if (found) {

    Buffer found_buffer = buffers[i];

    buffers[i] = buffers.back();
    buffers.pop_back();

    transient_buffers_cache[frame].used_transient_buffers[name] = {
        key, found_buffer};

    return transient_buffers_cache[frame].used_transient_buffers[name].second;
  }

  VkBufferCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = key.size;
  create_info.usage = key.usage_flags;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 1;
  create_info.pQueueFamilyIndices = &key.queue_family;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = key.allocation_flags;
  alloc_info.priority = 1.0f;

  Buffer new_buffer;

  VK_ERROR(vmaCreateBuffer(_allocator, &create_info, &alloc_info,
                           &new_buffer.buffer, &new_buffer.allocation,
                           &new_buffer.allocation_info),
           "Create transient buffer");

  new_buffer.current_offset = 0;
  new_buffer.size = key.size;
  new_buffer.free_spaces.push_back({0, key.size});
  new_buffer.usage_flags = key.usage_flags;

  transient_buffers_cache[frame].used_transient_buffers[name] = {key,
                                                                 new_buffer};

  return transient_buffers_cache[frame].used_transient_buffers[name].second;
}

void ResourceManager::registerTransientImage(const std::string &name,
                                             const TransientImageKey &key) {

  for (auto &cache : transient_images_cache) {
    cache.transient_virtual_images[name] = key;
  }
}

void ResourceManager::registerTransientBuffer(const std::string &name,
                                              const TransientBufferKey &key) {

  for (auto &cache : transient_buffers_cache) {
    cache.transient_virtual_buffers[name] = key;
  }
}

void ResourceManager::resetAllTransientImages(const uint32_t frame) {

  auto &cache = transient_images_cache[frame];

  for (auto &p : cache.used_transient_images) {

    auto &key = p.second.first;
    auto &img = p.second.second;

    cache.free_transient_images[key].push_back(img);
  }

  cache.used_transient_images.clear();
}

void ResourceManager::resetAllTransientBuffers(const uint32_t frame) {

  auto &cache = transient_buffers_cache[frame];

  for (auto &p : cache.used_transient_buffers) {

    auto &key = p.second.first;
    auto &buf = p.second.second;

    cache.free_transient_buffers[key].push_back(buf);
  }

  cache.used_transient_buffers.clear();
}

void ResourceManager::transistionResourceImage(
    VkCommandBuffer cmd, size_t resource_idx, VkImageLayout new_layout,
    uint32_t mip_levels, uint32_t array_layers, uint32_t old_family_queue,
    uint32_t new_family_queue) {

  auto *r = this->resources[resource_idx].lock().get();

  if (!r) {
    return;
  }

  std::visit(
      [&](auto &value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, Image>) {

          VkImageLayout old_layout = value.current_layout;
          value.current_layout = new_layout;

          vk_utils::transistionImage(cmd, old_layout, new_layout, value.image,
                                     mip_levels, array_layers,
                                     value.aspect_mask, old_family_queue,
                                     new_family_queue);

        }

        else if constexpr (std::is_same_v<T, Buffer>) {

          std::cerr << "Resourcs is not image\n";
          return;
        }
      },
      r->value);
}

void ResourceManager::transistionResourceBuffer(
    VkCommandBuffer cmd, VkQueueFlags queue_flags, size_t resource_idx, VkAccessFlags old_access,
    VkAccessFlags new_access, uint32_t size, uint32_t offset,
    uint32_t src_queue_family, uint32_t dst_queue_family) {

  auto *r = this->resources[resource_idx].lock().get();

  if (!r) {
    return;
  }

  std::visit(
      [&](auto &value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, Buffer>) {

          vk_utils::transistionBuffer(cmd, old_access, new_access, size, offset,
                                      value.buffer, queue_flags, src_queue_family,
                                      dst_queue_family);

        }

        else if constexpr (std::is_same_v<T, Image>) {

          std::cerr << "Resourcs is not buffer\n";
          return;
        }
      },
      r->value);
}

void ResourceManager::commitWriteTransmit(VkCommandBuffer cmd, VkQueueFlags queue_flags,
                                          ResourceWriteInfo &write_info,
                                          uint32_t old_family_index,
                                          uint32_t new_family_index) {

  if (old_family_index == new_family_index)
    return;

  auto weak_r = this->resources.at(write_info.target_index);

  if (weak_r.expired()) {
    return;
  }

  Resource *ref = weak_r.lock().get();

  std::visit(
      [&](auto &value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, Image>) {

          bool queue_family_transfer = old_family_index != new_family_index;

          this->transistionResourceImage(
              cmd, write_info.target_index,
              write_info.image_write_data.new_layout,
              write_info.image_write_data.mip_lvl_data.size(), 1,
              queue_family_transfer ? old_family_index : UINT32_MAX,
              queue_family_transfer ? new_family_index : UINT32_MAX);
        }

        else if constexpr (std::is_same_v<T, Buffer>) {

          bool queue_family_transfer = old_family_index != new_family_index;

          this->transistionResourceBuffer(
              cmd, queue_flags, write_info.target_index, VK_ACCESS_TRANSFER_WRITE_BIT,
              write_info.buffer_write_data.new_access,
              queue_family_transfer ? old_family_index : UINT32_MAX,
              queue_family_transfer ? new_family_index : UINT32_MAX);
        }
      },
      ref->value);
}

void ResourceManager::commitWrite(VkCommandBuffer cmd, VkQueueFlags queue_flags,
                                  ResourceWriteInfo &write_info,
                                  uint32_t old_family_index,
                                  uint32_t new_family_index) {

  auto weak_r = this->resources.at(write_info.target_index);

  if (weak_r.expired()) {
    return;
  }

  Resource *ref = weak_r.lock().get();

  std::visit(
      [&](auto &value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, Image>) {

          this->transistionResourceImage(
              cmd, write_info.target_index,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              write_info.image_write_data.mip_lvl_data.size(), 1);

          for (size_t i = 0;
               i < write_info.image_write_data.mip_lvl_data.size(); i++) {

            uint64_t offset =
                write_info.image_write_data.mip_lvl_data[i].offset;

            uint32_t width = std::max(1u, value.extent.width >> i);
            uint32_t height = std::max(1u, value.extent.height >> i);
            uint32_t depth = std::max(1u, value.extent.depth >> i);

            VkBufferImageCopy region = {};
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.bufferOffset = offset;
            region.imageExtent = {width, height, depth};
            region.imageOffset = {
                static_cast<int32_t>(write_info.target_offset[0]),
                static_cast<int32_t>(write_info.target_offset[1]),
                static_cast<int32_t>(write_info.target_offset[2])};
            region.imageSubresource.aspectMask = value.aspect_mask;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.mipLevel = i;
            region.imageSubresource.layerCount = 1;

            vkCmdCopyBufferToImage(
                cmd, write_info.source_buffer.buffer, value.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
          }

          bool queue_family_transfer = old_family_index != new_family_index;

          this->transistionResourceImage(
              cmd, write_info.target_index,
              queue_family_transfer ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                    : write_info.image_write_data.new_layout,
              write_info.image_write_data.mip_lvl_data.size(), 1,
              queue_family_transfer ? old_family_index : UINT32_MAX,
              queue_family_transfer ? new_family_index : UINT32_MAX);

        } else if constexpr (std::is_same_v<T, Buffer>) {

          this->transistionResourceBuffer(
              cmd, queue_flags, write_info.target_index, VK_ACCESS_NONE,
              VK_ACCESS_TRANSFER_WRITE_BIT,
              write_info.buffer_write_data.write_size,
              write_info.buffer_write_data.write_offset);

          VkBufferCopy region = {};
          region.srcOffset = 0;
          region.dstOffset = write_info.buffer_write_data.write_offset;
          region.size =  write_info.buffer_write_data.write_size;

          vkCmdCopyBuffer(cmd, write_info.source_buffer.buffer, value.buffer, 1,
                          &region);
          bool queue_family_transfer = old_family_index != new_family_index;


          this->transistionResourceBuffer(
              cmd, queue_flags, write_info.target_index, VK_ACCESS_TRANSFER_WRITE_BIT,
              queue_family_transfer ? write_info.buffer_write_data.new_access
                                    : VK_ACCESS_TRANSFER_WRITE_BIT,
              write_info.buffer_write_data.write_size,
              write_info.buffer_write_data.write_offset,
              queue_family_transfer ? old_family_index : UINT32_MAX,
              queue_family_transfer ? new_family_index : UINT32_MAX);
        }
      },
      ref->value);
}

void ResourceManager::transistionTransientImage(const std::string &name,
                                                const uint32_t frame,
                                                VkCommandBuffer cmd,
                                                VkImageLayout new_layout) {

  Image *ref =
      &transient_images_cache.at(frame).used_transient_images.at(name).second;

  vk_utils::transistionImage(cmd, ref->current_layout, new_layout, ref->image,
                             ref->mip_map_number, ref->array_layers,
                             ref->aspect_mask);

  ref->current_layout = new_layout;
}

void ResourceManager::transistionTransientBuffer(
    const std::string &name, const uint32_t frame, VkCommandBuffer cmd, VkQueueFlags queue_flags,
    VkAccessFlags old_access, VkAccessFlags new_access, uint32_t size,
    uint32_t offset) {

  Buffer &ref =
      transient_buffers_cache.at(frame).used_transient_buffers.at(name).second;

      

  vk_utils::transistionBuffer(cmd, old_access, new_access, size, offset, 
                              ref.buffer, queue_flags);
}

Descriptor ResourceManager::bindTransient(
    std::span<CombinedTransientNameAndDescriptorType> resources_to_bind,
    uint32_t binding_count, VkDescriptorSetLayout layout,
    const uint32_t frame) {

  Descriptor set;

  assert(binding_count <= MAX_BINDINGS_PER_SET);
  assert(resources_to_bind.size() == binding_count);

  TransientDescriptorSetKey key{};

  std::copy(resources_to_bind.begin(),
            resources_to_bind.begin() + binding_count, key.bindings.begin());

  key.count_bindings = binding_count;

  {

    auto it =
        transient_descriptors[frame].bound_resource_descriptor_sets.find(key);

    if (it !=
        transient_descriptors[frame].bound_resource_descriptor_sets.end()) {

      // Already bound
      set = it->second;

      return set;
    }
  }

  TransientDescriptorSetKey searched_types_key{};

  {
    size_t i = 0;
    for (const auto &p : resources_to_bind) {

      const VkDescriptorType type = p.type;

      switch (p.kind) {

      case TransientKind::Buffer: {

        CombinedTransientNameAndDescriptorType free_searched_types_type{
            "", type, SIZE_MAX, TransientKind::Buffer, {}};

        searched_types_key.bindings[i++] = free_searched_types_type;
        break;
      }

      case TransientKind::Image: {

        CombinedTransientNameAndDescriptorType free_searched_types_type{
            "", type, SIZE_MAX, TransientKind::Image, {}};

        searched_types_key.bindings[i++] = free_searched_types_type;
        break;
      }

      case TransientKind::Undefined:
        throw std::runtime_error("No defined Kind for transient Resource");
        break;
      }
    }
  }
  auto it = transient_descriptors[frame].free_resource_descriptor_sets.find(
      searched_types_key);

  if (it != transient_descriptors[frame].free_resource_descriptor_sets.end()) {

    set = it->second[it->second.size() - 1];
    it->second.pop_back();

  } else {

    set.layout = layout;
    set.set = _dynamic_allocator.allocate(_device, set.layout);
  }

  std::array<VkWriteDescriptorSet, MAX_BINDINGS_PER_SET> writes;

  DescriptorWriter writer = {};

  for (size_t i = 0; i < binding_count; i++) {

    auto &write = writes[i];

    CombinedTransientNameAndDescriptorType &type = resources_to_bind[i];

    switch (type.kind) {

    case TransientKind::Buffer: {
      Buffer *buf = &transient_buffers_cache[frame]
                         .used_transient_buffers.at(type.name)
                         .second;

      assert(buf);

      writer.writeBuffer(i, buf->buffer, type.size, 0, type.type);

      break;
    }
    case TransientKind::Image: {
      Image *img = &transient_images_cache[frame]
                        .used_transient_images.at(type.name)
                        .second;

      assert(img);

      writer.writeImage(i, img->view, getSampler(type.sampler),
                        img->current_layout, type.type);

      break;
    }
    case TransientKind::Undefined:
      throw std::runtime_error("Undefined Type");

      break;
    }
  }

  writer.updateSet(_device, set.set);

  transient_descriptors[frame].bound_resource_descriptor_sets[key] = set;

  return set;
}

VkSampler ResourceManager::getSampler(SamplerKey &key) {

  auto it = samplers.find(key);

  if (it != samplers.end()) {

    return it->second;
  }

  VK_ERROR(vkCreateSampler(_device, &key.create_info, nullptr, &samplers[key]),
           "Create Sampler");

  return samplers[key];
}