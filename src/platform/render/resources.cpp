#include "resources.h"
#include "vulkan_macros.h"
#include <memory>
#include <optional>
#include <variant>
#include <vulkan/vulkan_core.h>

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

void DescriptorAllocator::initPool(VkDevice device, uint32_t max_sets,
                                   std::span<PoolSizeRatio> pool_ratios) {

  std::vector<VkDescriptorPoolSize> pool_sizes = {};

  for (PoolSizeRatio &ratio : pool_ratios) {
    pool_sizes.push_back(VkDescriptorPoolSize{
        ratio.type, static_cast<uint>(ratio.ratio * max_sets)});
  }

  VkDescriptorPoolCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  create_info.flags = 0;
  create_info.maxSets = max_sets;
  create_info.poolSizeCount = pool_sizes.size();
  create_info.pPoolSizes = pool_sizes.data();

  VK_CHECK(vkCreateDescriptorPool(device, &create_info, nullptr, &pool),
           "Create Descriptor Pool");
}

void DescriptorAllocator::clearDescriptors(VkDevice device) {

  VK_CHECK(vkResetDescriptorPool(device, pool, 0), "Reset Descriptor Pool");
}

void DescriptorAllocator::destroyPool(VkDevice device) {
  vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device,
                                              VkDescriptorSetLayout layout) {
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet set;
  VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &set),
           "Create Descriptor Set");

  return set;
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

ResourceManager::ResourceManager(VkDevice &device, VmaAllocator &allocator)
    : _device(device), _allocator(allocator) {

  DescriptorAllocatorGrowable::PoolSizeRatio ratios[] = {

      {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3.0f},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 3.0f},
      {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 4.0f},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 2.0f},

  };

  _dynamic_allocator.init(_device, 50, ratios);

  DescriptorSetLayoutBuilder builder;
  builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  _global_descriptor_set_layout = builder.build(device, VK_SHADER_STAGE_ALL);

  _global_descriptor_set =
      _dynamic_allocator.allocate(_device, _global_descriptor_set_layout);
}

ResourceManager::~ResourceManager() {};

void ResourceManager::runDeletionQueue() { _deletion_queue.flush(this); }

ResourceHandle ResourceManager::createBuffer(size_t size,
                                             VkBufferUsageFlagBits usage_flags,
                                             std::optional<std::string> name) {

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

  Resource new_resource(new_buffer, this, id);

  resources.insert_or_assign(id, new_resource);

  ResourceHandle new_handle(id, std::make_shared<Resource>(new_resource));

  if (name != std::nullopt) {
    resource_names.insert_or_assign(name.value(), id);
  }

  return new_handle;
}