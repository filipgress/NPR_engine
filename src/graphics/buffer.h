#ifndef BUFFER_H_
#define BUFFER_H_

#include "vulkan_context.h"

namespace npr_graphics {

class Buffer : public npr_core::NonCopyable {
 public:
  Buffer(const VulkanContext& context, vk::BufferUsageFlags usage,
         vk::MemoryPropertyFlags properties, uint32_t size,
         const std::string& dbg_name);
  virtual ~Buffer();

  void Write(vk::CommandBuffer cmd_buff, const void* data);
  void DestroyStagingBuff() { staging_buff_.reset(); }

 private:
  void CreateBuffer();
  void CreateStagingBuffer();
  void AllocMem();

 private:
  const VulkanContext& c_;

  vk::Buffer buff_;
  vk::DeviceMemory buff_mem_;
  std::unique_ptr<Buffer> staging_buff_;

  vk::MemoryPropertyFlags properties_;
  vk::BufferUsageFlags usage_;

  uint32_t size_{0};

  std::string dbg_name_;
};

struct Vertex {
  glm::vec3 pos;
  glm::vec2 uv;

  glm::vec3 normal;
  glm::vec4 tangent;

  static vk::VertexInputBindingDescription GetBindingDesc();
  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDescs();
};

class VertexBuffer : public Buffer {
 public:
  VertexBuffer(const VulkanContext& context,
               const std::vector<Vertex>& vertices)
      : Buffer(context,
               vk::BufferUsageFlagBits::eVertexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal,
               vertices.size() * sizeof(Vertex), "vertex_buffer") {}
};

class IndexBuffer : public Buffer {
 public:
  IndexBuffer(const VulkanContext& context,
              const std::vector<uint32_t>& indices)
      : Buffer(context,
               vk::BufferUsageFlagBits::eIndexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal,
               indices.size() * sizeof(indices[0]), "index_buffer"),
        count_{static_cast<uint32_t>(indices.size())} {}

  size_t GetCount() const { return count_; }

 private:
  size_t count_;
};

}  // namespace npr_graphics
#endif  // BUFFER_H_
