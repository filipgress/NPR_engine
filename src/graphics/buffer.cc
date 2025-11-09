#include "buffer.h"

namespace npr_graphics {

Buffer::Buffer(const VulkanContext& context, vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags mem_props, vk::SharingMode sharing_mode,
               uint32_t size, const std::string& dbg_name)
    : c_{context},
      mem_props_{mem_props},
      usage_{usage},
      size_{size},
      dbg_name_{dbg_name} {
  assert(size_);

  CreateBuffer(sharing_mode);
  AllocMem();
}

Buffer::Buffer(Buffer&& other) noexcept
    : c_(other.c_),
      buff_(std::move(other.buff_)),
      buff_mem_(std::move(other.buff_mem_)),
      mem_props_(other.mem_props_),
      usage_(other.usage_),
      mapped_mem_(other.mapped_mem_),
      staging_buff_(std::move(other.staging_buff_)),
      size_(other.size_),
      dbg_name_(std::move(other.dbg_name_)) {
  other.buff_ = nullptr;
  other.buff_mem_ = nullptr;
  other.mapped_mem_ = nullptr;
  other.size_ = 0;
}

Buffer::~Buffer() {
  UnmapMemory();

  auto device = c_.GetDevice();
  if (buff_mem_) device.freeMemory(buff_mem_);
  if (buff_) device.destroyBuffer(buff_);
}

void Buffer::CreateBuffer(vk::SharingMode sharing_mode) {
  vk::BufferCreateInfo buffer_info;
  buffer_info.size = size_;
  buffer_info.usage = usage_;

  auto q_families = c_.GetQFamilies();
  if (sharing_mode == vk::SharingMode::eConcurrent &&
      mem_props_ & vk::MemoryPropertyFlagBits::eDeviceLocal &&
      q_families.graphics_i.value() != q_families.transfer_i.value()) {
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
      c_.FindMemTypeIdx(mem_req.memoryTypeBits, mem_props_);

  buff_mem_ = device.allocateMemory(allocInfo);
  device.bindBufferMemory(buff_, buff_mem_, 0);
}

void Buffer::Write(vk::CommandBuffer cmd_buff, const void* data) {
  auto device = c_.GetDevice();

  if (mem_props_ & vk::MemoryPropertyFlagBits::eHostVisible) {
    if (!mapped_mem_) mapped_mem_ = device.mapMemory(buff_mem_, 0, size_);
    memcpy(mapped_mem_, data, size_);

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
    pre_copy_barrier.size = VK_WHOLE_SIZE;
    pre_copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre_copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    cmd_buff.pipelineBarrier(stage, vk::PipelineStageFlagBits::eTransfer,
                             vk::DependencyFlagBits::eByRegion, 0, nullptr, 1,
                             &pre_copy_barrier, 0, nullptr);

    if (!staging_buff_)
      staging_buff_ = std::make_unique<StagingBuffer>(c_, size_);
    staging_buff_->Write(nullptr, data);

    vk::BufferCopy region{0, 0, size_};
    cmd_buff.copyBuffer(staging_buff_->buff_, buff_, region);

    vk::BufferMemoryBarrier post_copy_barrier{};
    post_copy_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    post_copy_barrier.dstAccessMask = access;
    post_copy_barrier.buffer = buff_;
    post_copy_barrier.offset = 0;
    post_copy_barrier.size = VK_WHOLE_SIZE;
    post_copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    cmd_buff.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, stage,
                             vk::DependencyFlagBits::eByRegion, 0, nullptr, 1,
                             &post_copy_barrier, 0, nullptr);
  }
}

}  // namespace npr_graphics
