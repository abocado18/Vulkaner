#pragma once

#include "platform/renderer/gpu_structs.h"
#include "platform/renderer/vertex.h"
#include "tinygltf/tiny_gltf.h"

#include "game/ecs/vox_ecs.h"
#include "platform/renderer/renderer.h"
#include "platform/renderer/material.h"
#include <vector>

namespace gltf_load {

static void loadScene(const std::string &path, vecs::Ecs &world,
                      render::RenderContext &render_ctx) {
  using namespace tinygltf;

  Model model;
  TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
  // bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename); // for
  // binary glTF(.glb)

  if (!warn.empty()) {
    printf("Warn: %s\n", warn.c_str());
  }

  if (!err.empty()) {
    printf("Err: %s\n", err.c_str());
  }

  if (!ret) {
    printf("Failed to parse glTF: %s\n", path.c_str());
  }

  auto &p = model.meshes[0].primitives[0];

  std::vector<vertex::Vertex> vertex_data = {};
  std::vector<vertex::Index> indices = {};

  if (p.attributes.find("POSITION") != p.attributes.end()) {
    int accessorIndex = p.attributes.at("POSITION");
    const tinygltf::Accessor &accessor = model.accessors[accessorIndex];
    const tinygltf::BufferView &bufferView =
        model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

    // Pointer to the vertex data
    const float *vertices = reinterpret_cast<const float *>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]);

    size_t vertexCount = accessor.count;
    std::cout << "Vertex count: " << vertexCount << std::endl;

    vertex_data.resize(vertexCount);

    size_t stride = accessor.ByteStride(bufferView);
    if (stride == 0)
      stride = 3 * sizeof(float);

    for (size_t i = 0; i < vertexCount; i++) {
      const float *vertexPtr = reinterpret_cast<const float *>(
          &buffer
               .data[bufferView.byteOffset + accessor.byteOffset + i * stride]);

      vertex_data[i].position =
          Vector3(vertexPtr[0], vertexPtr[1], vertexPtr[2]);
    }
  }

  if (p.attributes.find("TEX_COORD0") != p.attributes.end()) {
    int accessorIndex = p.attributes.at("TEX_COORD0");
    const tinygltf::Accessor &accessor = model.accessors[accessorIndex];
    const tinygltf::BufferView &bufferView =
        model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

    // Pointer to the vertex data
    const float *vertices = reinterpret_cast<const float *>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]);

    size_t vertexCount = accessor.count;
    std::cout << "Vertex count: " << vertexCount << std::endl;

    vertex_data.resize(vertexCount);

    size_t stride = accessor.ByteStride(bufferView);
    if (stride == 0)
      stride = 2 * sizeof(float);

    for (size_t i = 0; i < vertexCount; i++) {
      const float *vertexPtr = reinterpret_cast<const float *>(
          &buffer
               .data[bufferView.byteOffset + accessor.byteOffset + i * stride]);

      vertex_data[i].tex_coords =
          Vector2(vertexPtr[0], vertexPtr[1]);
    }
  }

  if (p.indices >= 0) {
    const tinygltf::Accessor &indexAccessor = model.accessors[p.indices];
    const tinygltf::BufferView &indexBufferView =
        model.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer &indexBuffer = model.buffers[indexBufferView.buffer];

    indices.resize(indexAccessor.count);

    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
      const uint16_t *buf = reinterpret_cast<const uint16_t *>(
          &indexBuffer
               .data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
      for (size_t i = 0; i < indexAccessor.count; i++) {
        indices[i].value = static_cast<uint32_t>(buf[i]);
      }
    } else if (indexAccessor.componentType ==
               TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
      const uint32_t *buf = reinterpret_cast<const uint32_t *>(
          &indexBuffer
               .data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
      for (size_t i = 0; i < indexAccessor.count; i++) {
        indices[i].value = static_cast<uint32_t>(buf[i]);
      }
    } else {
      throw std::runtime_error("Unsupported index type");
    }
  }


  std::cout << "Number of indices: " << indices.size() << "\n";

  auto offset = render_ctx.writeToBuffer<vertex::Vertex>(vertex_data.data(), vertex_data.size());
  auto i_offset = render_ctx.writeToBuffer<vertex::Index>(indices.data(), indices.size());


  assert(offset == 0);
  assert(i_offset == 0);

}

} // namespace gltf_load