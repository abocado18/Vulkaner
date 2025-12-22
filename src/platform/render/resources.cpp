#include "resources.h"
#include "vulkan_macros.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <sys/types.h>
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

  if (std::holds_alternative<Image>(value)) {

    Image img = std::get<Image>(value);

    resource_manager->_deletion_queue.pushFunction(
        [img](ResourceManager *manager) {
          vkDestroyImageView(manager->_device, img.view, nullptr);

          vmaDestroyImage(manager->_allocator, img.image, img.allocation);
        });
  } else {

    Buffer buffer = std::get<Buffer>(value);
    resource_manager->_deletion_queue.pushFunction(
        [buffer](ResourceManager *manager) {
          vmaDestroyBuffer(manager->_allocator, buffer.buffer,
                           buffer.allocation);
        });
  }

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
}

ResourceManager::~ResourceManager() {

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
    std::vector<CombinedResourceIndexAndDescriptorType> &resources_to_bind,
    VkDescriptorSetLayout layout) {

  Descriptor set;

  {
    auto it = bound_descriptor_sets.find(resources_to_bind);

    if (it != bound_descriptor_sets.end()) {

      // Already bound
      set = it->second;

      return set;
    }
  }

  std::vector<VkDescriptorType> searched_types(resources_to_bind.size());

  {
    size_t i = 0;
    for (const auto &p : resources_to_bind) {

      const size_t idx = p.idx;
      const VkDescriptorType type = p.type;

      auto r = resources[idx].lock();

      if (!r) {
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};
      }

      searched_types[i++] = type;
    }
  }

  auto it = free_descriptor_sets.find(searched_types);

  if (it != free_descriptor_sets.end()) {

    set = it->second[it->second.size() - 1];
    it->second.pop_back();

  } else {

    set.layout = layout;
    set.set = _dynamic_allocator.allocate(_device, set.layout);
  }

  std::vector<VkWriteDescriptorSet> writes(resources_to_bind.size());

  DescriptorWriter writer = {};

  for (size_t i = 0; i < writes.size(); i++) {

    auto &write = writes[i];

    Resource *r = resources[resources_to_bind[i].idx].lock().get();

    if (std::holds_alternative<Image>(r->value)) {

      Image &img = std::get<Image>(r->value);

      writer.writeImage(i, img.view, VK_NULL_HANDLE, img.current_layout,
                        searched_types[i]);

    } else {
      Buffer &buf = std::get<Buffer>(r->value);

      writer.writeBuffer(i, buf.buffer, resources_to_bind[i].size, 0,
                         searched_types[i]);
    }
  }

  writer.updateSet(_device, set.set);

  bound_descriptor_sets[resources_to_bind] = set;

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

  vmaCreateBuffer(_allocator, &create_info, &alloc_info, &staging_buffer.buffer,
                  &staging_buffer.allocation, &staging_buffer.allocation_info);

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
    return {};
  }

  ResourceWriteInfo info(resources.at(handle.idx).lock()->value,
                         {allocated_offset}, staging_buffer);

  info.buffer_write_data = {new_access};

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
                                 std::span<size_t> mip_lvl_offsets,
                                 VkImageLayout new_layout) {

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

  Image &img_ref = std::get<Image>(ref.value);

  Buffer staging_buffer;

  VkBufferCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = size;
  create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT;

  vmaCreateBuffer(_allocator, &create_info, &alloc_info, &staging_buffer.buffer,
                  &staging_buffer.allocation, &staging_buffer.allocation_info);

  staging_buffer.current_offset = 0;
  staging_buffer.size = size;

  std::memcpy(staging_buffer.allocation_info.pMappedData, data, size);

  ResourceWriteInfo write_info(resources.at(handle.idx).lock()->value, offset,
                               staging_buffer);
  write_info.image_write_data.new_layout = new_layout;
  write_info.image_write_data.mip_lvl_offsets.reserve(mip_lvl_offsets.size());

  for (auto &o : mip_lvl_offsets) {
    write_info.image_write_data.mip_lvl_offsets.push_back(
        static_cast<uint32_t>(o));
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

Image ResourceManager::getTransientImage(const std::string &name,
                                         const uint32_t frame) {

  TransientImageKey &key =
      transient_images_cache[frame].transient_virtual_images.at(name);

  auto &images = transient_images_cache[frame].free_transient_images[key];

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

    Image found_image = images[i];

    images[i] = images.back();
    images.pop_back();

    transient_images_cache[frame].used_transient_images[name] = {key,
                                                                 found_image};

    return found_image;
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

  transient_images_cache[frame].used_transient_images[name] = {key, new_image};

  return new_image;
}

void ResourceManager::registerTransientImage(const std::string &name,
                                             const TransientImageKey &key) {

  for (auto &cache : transient_images_cache) {
    cache.transient_virtual_images[name] = key;
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

void ResourceManager::transistionImage(VkCommandBuffer cmd, Image &image,
                                       VkImageLayout new_layout,
                                       uint32_t mip_levels,
                                       uint32_t array_layers,
                                       uint32_t old_family_queue,
                                       uint32_t new_family_queue) {

  vk_utils::transistionImage(cmd, image.current_layout, new_layout, image.image,
                             mip_levels, array_layers, old_family_queue,
                             new_family_queue);

  image.current_layout = new_layout;
}