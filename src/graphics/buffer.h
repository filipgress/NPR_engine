#ifndef BUFFER_H_
#define BUFFER_H_

#include "vulkan_context.h"

namespace npr_graphics {

class StagingBuffer;
class Buffer : public npr_core::NonCopyable {
 public:
  Buffer(const VulkanContext& context, vk::BufferUsageFlags usage,
         vk::MemoryPropertyFlags mem_props, vk::SharingMode sharing_mode,
         uint32_t size, const std::string& dbg_name);
  Buffer(Buffer&&) noexcept;
  virtual ~Buffer();

  vk::Buffer GetBuffer() const { return buff_; }

  void Write(vk::CommandBuffer cmd_buff, const void* data);

  void DestroyStagingBuff() { staging_buff_.reset(); }
  void UnmapMemory() {
    if (mapped_mem_) c_.GetDevice().unmapMemory(buff_mem_);
    mapped_mem_ = nullptr;
  }

 private:
  void CreateBuffer(vk::SharingMode sharing_mode);
  void AllocMem();

 protected:
  const VulkanContext& c_;

  vk::Buffer buff_;
  vk::DeviceMemory buff_mem_;

  vk::MemoryPropertyFlags mem_props_;
  vk::BufferUsageFlags usage_;

  void* mapped_mem_{nullptr};
  std::unique_ptr<StagingBuffer> staging_buff_;

  uint32_t size_{0};
  std::string dbg_name_;
};

class StagingBuffer : public Buffer {
 public:
  StagingBuffer(const VulkanContext& context, uint32_t size)
      : Buffer(context, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vk::SharingMode::eExclusive, size, "staging_buff") {}
};

struct Vertex {
  glm::vec3 pos;
  glm::vec2 uv;

  glm::vec3 normal{0.0f};
  glm::vec4 tangent{1.0f};
};

class VertexBuffer : public Buffer {
 public:
  VertexBuffer(const VulkanContext& context, vk::CommandBuffer cmd_buff,
               const std::vector<Vertex>& vertices, const std::string& dbg_name)
      : Buffer(context,
               vk::BufferUsageFlagBits::eVertexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal,
               vk::SharingMode::eExclusive, vertices.size() * sizeof(Vertex),
               dbg_name),
        count_{vertices.size()} {
    Write(cmd_buff, vertices.data());
  }

  size_t GetCount() const { return count_; }

 private:
  size_t count_;
};

class IndexBuffer : public Buffer {
 public:
  IndexBuffer(const VulkanContext& context, vk::CommandBuffer cmd_buff,
              const std::vector<uint32_t>& indices, const std::string& dbg_name)
      : Buffer(context,
               vk::BufferUsageFlagBits::eIndexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal,
               vk::SharingMode::eExclusive, indices.size() * sizeof(indices[0]),
               dbg_name),
        count_{indices.size()} {
    Write(cmd_buff, indices.data());
  }

  size_t GetCount() const { return count_; }

 private:
  size_t count_;
};

struct Mesh {
  VertexBuffer vbo;
  IndexBuffer ibo;

  Mesh(VertexBuffer&& v, IndexBuffer&& i)
      : vbo(std::move(v)), ibo(std::move(i)) {}
};

struct Instance {
  glm::mat4 model;
  glm::mat3 normal;
};

class InstanceBuffer : public Buffer {
 public:
  InstanceBuffer(const VulkanContext& context, uint32_t max_instances,
                 const std::string& dbg_name)
      : Buffer(context, vk::BufferUsageFlagBits::eVertexBuffer,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vk::SharingMode::eExclusive, max_instances * sizeof(Instance),
               dbg_name),
        max_instances_{max_instances} {}

  void Write(const std::vector<Instance>& instances, uint32_t offset = 0) {
    assert(instances.size() <= GetMaxInstances());

    if (offset + instances.size() > GetMaxInstances()) offset = 0;
    if (!mapped_mem_)
      mapped_mem_ = c_.GetDevice().mapMemory(buff_mem_, 0, size_);

    memcpy(static_cast<char*>(mapped_mem_) + offset * sizeof(Instance),
           instances.data(), instances.size() * sizeof(Instance));
  }

  uint32_t GetMaxInstances() const { return max_instances_; }

 private:
  uint32_t max_instances_;
};

template <typename T>
class UniformBuffer : public Buffer {
 public:
  UniformBuffer(const VulkanContext& context, const std::string& dbg_name)
      : Buffer(context, vk::BufferUsageFlagBits::eUniformBuffer,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vk::SharingMode::eExclusive, sizeof(T), dbg_name) {}
  void Write(const T& ubo) { Buffer::Write(nullptr, &ubo); }
};

template <typename T>
class DynamicUniformBuffer : public Buffer {
 public:
  DynamicUniformBuffer(const VulkanContext& context, uint32_t obj_count,
                       const std::string& dbg_name)
      : Buffer{context,
               vk::BufferUsageFlagBits::eUniformBuffer,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vk::SharingMode::eExclusive,
               CalcBufferSize(context, obj_count),
               dbg_name} {}

  uint32_t GetElemSize() const { return aligned_size_; }
  uint32_t GetElemOffset(uint32_t idx) const {
    assert(idx < elem_count_);
    return idx * aligned_size_;
  }

  void Write(uint32_t idx, const T& ubo) {
    assert(idx < elem_count_);
    if (!mapped_mem_)
      mapped_mem_ = c_.GetDevice().mapMemory(buff_mem_, 0, size_);
    memcpy(static_cast<char*>(mapped_mem_) + idx * aligned_size_, &ubo,
           sizeof(T));
  }

 private:
  uint32_t CalcBufferSize(const VulkanContext& context, uint32_t elem_count) {
    aligned_size_ = npr_core::Align(
        sizeof(T),
        context.GetProperties().limits.minUniformBufferOffsetAlignment);
    elem_count_ = elem_count;

    return aligned_size_ * elem_count_;
  }

 private:
  uint32_t elem_count_;
  uint32_t aligned_size_;
};

template <typename T>
class StorageBuffer : public Buffer {
 public:
  StorageBuffer(const VulkanContext& context, const std::string& dbg_name)
      : Buffer(context,
               vk::BufferUsageFlagBits::eStorageBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal,
               vk::SharingMode::eExclusive, sizeof(T), dbg_name) {}
  void Write(vk::CommandBuffer cmd_buff, const T& data) {
    Buffer::Write(cmd_buff, &data);
  }
};

}  // namespace npr_graphics

#endif  // BUFFER_H_
