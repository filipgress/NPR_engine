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

 private:
  void CreateBuffer(vk::SharingMode sharing_mode);
  void AllocMem();

 private:
  const VulkanContext& c_;

  vk::Buffer buff_;
  vk::DeviceMemory buff_mem_;
  std::unique_ptr<StagingBuffer> staging_buff_;

  vk::MemoryPropertyFlags mem_props_;
  vk::BufferUsageFlags usage_;

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

  static vk::VertexInputBindingDescription GetBindingDesc();
  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDescs();
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
               dbg_name) {
    Write(cmd_buff, vertices.data());
  }
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

}  // namespace npr_graphics
#endif  // BUFFER_H_
