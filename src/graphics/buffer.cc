#include "buffer.h"

namespace npr_graphics {

Buffer::Buffer(const VulkanContext& context, vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags properties, uint32_t size,
               const std::string& dbg_name)
    : c_{context},
      properties_{properties},
      usage_{usage},
      size_{size},
      dbg_name_{dbg_name} {
  assert(size_);

  CreateBuffer();
  AllocMem();
}

Buffer::~Buffer() {
  auto device = c_.GetDevice();
  device.waitIdle();

  if (buff_mem_) device.freeMemory(buff_mem_);
  if (buff_) device.destroyBuffer(buff_);
}

void Buffer::CreateStagingBuffer() {
  if (staging_buff_) return;
  staging_buff_ =
      std::make_unique<Buffer>(c_, vk::BufferUsageFlagBits::eTransferSrc,
                               vk::MemoryPropertyFlagBits::eHostVisible |
                                   vk::MemoryPropertyFlagBits::eHostCoherent,
                               size_, "staging_buff");
}

void Buffer::CreateBuffer() {
  vk::BufferCreateInfo buffer_info;
  buffer_info.size = size_;
  buffer_info.usage = usage_;

  auto q_families = c_.GetQFamilies();

  // only vertex & index buffers can be eConcurrent as
  // they are loaded from file and written using transfer queue
  if (q_families.graphics_i.value() != q_families.transfer_i.value() &&
      properties_ & vk::MemoryPropertyFlagBits::eDeviceLocal &&
      (usage_ & vk::BufferUsageFlagBits::eIndexBuffer ||
       usage_ & vk::BufferUsageFlagBits::eVertexBuffer)) {
    uint32_t indices[] = {q_families.graphics_i.value(),
                          q_families.transfer_i.value()};

    buffer_info.sharingMode = vk::SharingMode::eConcurrent;
    buffer_info.queueFamilyIndexCount = 2;
    buffer_info.pQueueFamilyIndices = indices;
  } else {
    buffer_info.sharingMode = vk::SharingMode::eExclusive;
  }

  buff_ = c_.GetDevice().createBuffer(buffer_info);
  c_.SetDbgName((uint64_t)(VkBuffer)buff_, vk::ObjectType::eBuffer, dbg_name_);
}

void Buffer::AllocMem() {
  auto device = c_.GetDevice();

  vk::MemoryRequirements mem_req = device.getBufferMemoryRequirements(buff_);
  vk::MemoryAllocateInfo allocInfo{};
  allocInfo.allocationSize = mem_req.size;
  allocInfo.memoryTypeIndex =
      c_.FindMemTypeIdx(mem_req.memoryTypeBits, properties_);

  buff_mem_ = device.allocateMemory(allocInfo);
  device.bindBufferMemory(buff_, buff_mem_, 0);
}

void Buffer::Write(vk::CommandBuffer cmd_buff, const void* data) {
  auto device = c_.GetDevice();

  if (properties_ & vk::MemoryPropertyFlagBits::eHostVisible) {
    void* mapped_mem = device.mapMemory(buff_mem_, 0, size_);
    memcpy(mapped_mem, data, size_);
    device.unmapMemory(buff_mem_);

  } else {
    assert(cmd_buff);

    vk::PipelineStageFlags stage;
    vk::AccessFlags access;

    if (usage_ & vk::BufferUsageFlagBits::eVertexBuffer) {
      stage = vk::PipelineStageFlagBits::eVertexInput;
      access = vk::AccessFlagBits::eVertexAttributeRead;

    } else if (usage_ & vk::BufferUsageFlagBits::eIndexBuffer) {
      stage = vk::PipelineStageFlagBits::eVertexInput;
      access = vk::AccessFlagBits::eIndexRead;

    } else if (usage_ & vk::BufferUsageFlagBits::eUniformBuffer) {
      stage = vk::PipelineStageFlagBits::eVertexShader |
              vk::PipelineStageFlagBits::eFragmentShader;
      access = vk::AccessFlagBits::eUniformRead;

    } else if (usage_ & vk::BufferUsageFlagBits::eStorageBuffer) {
      stage = vk::PipelineStageFlagBits::eVertexShader |
              vk::PipelineStageFlagBits::eFragmentShader |
              vk::PipelineStageFlagBits::eComputeShader;
      access =
          vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    }

    vk::BufferMemoryBarrier pre_copy_barrier{};
    pre_copy_barrier.srcAccessMask = access;
    pre_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    pre_copy_barrier.buffer = buff_;
    pre_copy_barrier.offset = 0;
    pre_copy_barrier.size = size_;
    pre_copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre_copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    cmd_buff.pipelineBarrier(stage, vk::PipelineStageFlagBits::eTransfer,
                             vk::DependencyFlagBits::eByRegion, 0, nullptr, 1,
                             &pre_copy_barrier, 0, nullptr);

    CreateStagingBuffer();
    staging_buff_->Write(nullptr, data);
    cmd_buff.copyBuffer(staging_buff_->buff_, buff_, {0, 0, size_});

    vk::BufferMemoryBarrier post_copy_barrier{};
    post_copy_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    post_copy_barrier.dstAccessMask = access;
    post_copy_barrier.buffer = buff_;
    post_copy_barrier.offset = 0;
    post_copy_barrier.size = size_;
    post_copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    cmd_buff.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, stage,
                             vk::DependencyFlagBits::eByRegion, 0, nullptr, 1,
                             &post_copy_barrier, 0, nullptr);
  }
}

/*
 * Vertex
 */
vk::VertexInputBindingDescription Vertex::GetBindingDesc() {
  vk::VertexInputBindingDescription binding_desc{};
  binding_desc.binding = 0;
  binding_desc.stride = sizeof(Vertex);
  binding_desc.inputRate = vk::VertexInputRate::eVertex;

  return binding_desc;
}

std::vector<vk::VertexInputAttributeDescription> Vertex::GetAttributeDescs() {
  std::vector<vk::VertexInputAttributeDescription> descs(4);

  // position
  descs[0].binding = 0;
  descs[0].location = 0;
  descs[0].format = vk::Format::eR32G32B32Sfloat;
  descs[0].offset = offsetof(Vertex, pos);

  // uv
  descs[1].binding = 0;
  descs[1].location = 1;
  descs[1].format = vk::Format::eR32G32Sfloat;
  descs[1].offset = offsetof(Vertex, uv);

  // normal
  descs[2].binding = 0;
  descs[2].location = 2;
  descs[2].format = vk::Format::eR32G32B32Sfloat;
  descs[2].offset = offsetof(Vertex, normal);

  // tan
  descs[3].binding = 0;
  descs[3].location = 3;
  descs[3].format = vk::Format::eR32G32B32A32Sfloat;
  descs[3].offset = offsetof(Vertex, tangent);

  return descs;
}

}  // namespace npr_graphics
