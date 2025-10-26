#include "image.h"

namespace npr_graphics {

Image::Image(const VulkanContext& context, vk::Format format,
             vk::Extent2D extent, vk::ImageUsageFlags usage,
             vk::ImageAspectFlags aspect, vk::SharingMode sharing_mode,
             vk::SampleCountFlagBits samples, uint32_t mip_levels,
             std::string dbg_name)
    : c_{context},
      format_{format},
      aspect_{aspect},
      extent_{extent},
      mip_levels_{CalculateMipLevels(mip_levels)},
      dbg_name_{dbg_name} {
  CreateImage(usage, samples, sharing_mode);
  AllocMem();

  CreateImageView();
}

Image::Image(Image&& other) noexcept
    : c_(other.c_),
      image_(std::move(other.image_)),
      image_mem_(std::move(other.image_mem_)),
      image_view_(std::move(other.image_view_)),
      format_(other.format_),
      aspect_(other.aspect_),
      extent_(other.extent_),
      mip_levels_(other.mip_levels_),
      dbg_name_(std::move(other.dbg_name_)) {
  other.image_ = nullptr;
  other.image_mem_ = nullptr;
  other.image_view_ = nullptr;
  other.mip_levels_ = 0;
}

Image::~Image() {
  auto device = c_.GetDevice();

  if (image_view_) device.destroyImageView(image_view_);
  if (image_mem_) device.freeMemory(image_mem_);
  if (image_) device.destroyImage(image_);
}

void Image::CreateImage(vk::ImageUsageFlags usage,
                        vk::SampleCountFlagBits samples,
                        vk::SharingMode sharing_mode) {
  vk::ImageCreateInfo image_info;
  image_info.initialLayout = vk::ImageLayout::eUndefined;
  image_info.imageType = vk::ImageType::e2D;
  image_info.format = format_;
  image_info.samples = samples;
  image_info.extent = vk::Extent3D(extent_, 1);
  image_info.mipLevels = mip_levels_;
  image_info.arrayLayers = 1;
  image_info.tiling = vk::ImageTiling::eOptimal;
  image_info.usage = usage;

  auto q_families = c_.GetQFamilies();
  if (sharing_mode == vk::SharingMode::eConcurrent &&
      q_families.graphics_i.value() != q_families.transfer_i.value()) {
    uint32_t indices[] = {q_families.graphics_i.value(),
                          q_families.transfer_i.value()};

    image_info.sharingMode = vk::SharingMode::eConcurrent;
    image_info.queueFamilyIndexCount = 2;
    image_info.pQueueFamilyIndices = indices;
  } else {
    image_info.sharingMode = vk::SharingMode::eExclusive;
  }

  image_ = c_.GetDevice().createImage(image_info);
  c_.SetDbgName((uint64_t)(VkImage)image_, vk::ObjectType::eImage, dbg_name_);
}

void Image::CreateImageView() {
  vk::ImageViewCreateInfo view_info{};
  view_info.image = image_;
  view_info.viewType = vk::ImageViewType::e2D;
  view_info.format = format_;
  view_info.subresourceRange.aspectMask = aspect_;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = mip_levels_;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  image_view_ = c_.GetDevice().createImageView(view_info);
}

void Image::AllocMem() {
  auto device = c_.GetDevice();

  vk::MemoryRequirements mem_req = device.getImageMemoryRequirements(image_);
  vk::MemoryAllocateInfo alloc_info{};
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex = c_.FindMemTypeIdx(
      mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

  image_mem_ = device.allocateMemory(alloc_info);
  device.bindImageMemory(image_, image_mem_, 0);
}

/*
 * Texture
 */
Texture::Texture(const VulkanContext& context, const TextureData& data,
                 const SamplerProps& sampler_props)
    : Image{context,
            GetTypeInfo(data.type).format,
            vk::Extent2D{data.width, data.height},
            vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eSampled,
            vk::ImageAspectFlagBits::eColor,
            vk::SharingMode::eExclusive,
            vk::SampleCountFlagBits::e1,
            UINT32_MAX,
            data.name} {
  CreateSampler(sampler_props);
}

Texture::Texture(Texture&& other) noexcept
    : Image(std::move(other)),
      sampler_(std::move(other.sampler_)),
      staging_buff_(std::move(other.staging_buff_)) {
  other.sampler_ = nullptr;
}

Texture::~Texture() {
  if (sampler_) c_.GetDevice().destroySampler(sampler_);
}

TextureInfo Texture::GetTypeInfo(TextureType type) {
  switch (type) {
    case TextureType::kColor:
      return {vk::Format::eR8G8B8A8Srgb, 4, "color"};
    case TextureType::kNormal:
      return {vk::Format::eR8G8B8A8Unorm, 4, "normal"};
    case TextureType::kMetallicRoughness:
      return {vk::Format::eR8G8Unorm, 2, "metallic_roughness"};
    case TextureType::kEmissive:
      return {vk::Format::eR8G8B8A8Srgb, 4, "emissive"};
    default:
      throw std::runtime_error("unsupported texture type");
  }
}

void Texture::CreateSampler(const SamplerProps& props) {
  vk::SamplerCreateInfo samplerInfo{};

  samplerInfo.magFilter = props.mag_filter;
  samplerInfo.minFilter = props.min_filter;
  samplerInfo.mipmapMode = props.mipmap_mode;

  samplerInfo.addressModeU = props.address_mode_U;
  samplerInfo.addressModeV = props.address_mode_V;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;

  samplerInfo.anisotropyEnable = VK_FALSE;
  // samplerInfo.maxAnisotropy = 1.0f;

  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  // samplerInfo.compareOp = vk::CompareOp::eAlways;

  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = mip_levels_;
  samplerInfo.mipLodBias = props.mip_bias;

  sampler_ = c_.GetDevice().createSampler(samplerInfo);
}

void Texture::Write(vk::CommandBuffer cmd_buff,
                    const std::vector<unsigned char>& data,
                    vk::ImageLayout src_layout) {
  if (!staging_buff_)
    staging_buff_ = std::make_unique<StagingBuffer>(c_, data.size());
  staging_buff_->Write(cmd_buff, data.data());

  Transition(cmd_buff, src_layout, vk::ImageLayout::eTransferDstOptimal, 0,
             mip_levels_);

  CopyFromBuffer(cmd_buff);
  GenerateMipmaps(cmd_buff);

  Transition(cmd_buff, vk::ImageLayout::eTransferDstOptimal,
             vk::ImageLayout::eShaderReadOnlyOptimal, mip_levels_ - 1, 1);
}

void Texture::Transition(vk::CommandBuffer cmd_buff, vk::ImageLayout old_layout,
                         vk::ImageLayout new_layout, uint32_t start_mip_level,
                         uint32_t mip_levels) {
  assert(mip_levels != 0 && start_mip_level + mip_levels <= mip_levels_);

  vk::ImageMemoryBarrier barrier{};
  barrier.image = image_;

  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;

  barrier.subresourceRange.aspectMask = aspect_;
  barrier.subresourceRange.baseMipLevel = start_mip_level;
  barrier.subresourceRange.levelCount = mip_levels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vk::PipelineStageFlags src_stage, dst_stage;

  if (old_layout == vk::ImageLayout::eUndefined &&
      new_layout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    dst_stage = vk::PipelineStageFlagBits::eTransfer;

  } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
             new_layout == vk::ImageLayout::eTransferSrcOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    src_stage = vk::PipelineStageFlagBits::eTransfer;
    dst_stage = vk::PipelineStageFlagBits::eTransfer;

  } else if (old_layout == vk::ImageLayout::eTransferSrcOptimal &&
             new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    src_stage = vk::PipelineStageFlagBits::eTransfer;
    dst_stage = vk::PipelineStageFlagBits::eFragmentShader;

  } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
             new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    src_stage = vk::PipelineStageFlagBits::eTransfer;
    dst_stage = vk::PipelineStageFlagBits::eFragmentShader;

  } else {
    throw std::runtime_error("unsupported image layout transition: " +
                             std::to_string(static_cast<int>(old_layout)) +
                             " -> " +
                             std::to_string(static_cast<int>(new_layout)));
  }

  cmd_buff.pipelineBarrier(src_stage, dst_stage, {}, nullptr, nullptr, barrier);
}

void Texture::CopyFromBuffer(vk::CommandBuffer cmd_buff) {
  vk::BufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = aspect_;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = vk::Offset3D{0, 0, 0};
  region.imageExtent = vk::Extent3D{extent_.width, extent_.height, 1};

  cmd_buff.copyBufferToImage(staging_buff_->GetBuffer(), image_,
                             vk::ImageLayout::eTransferDstOptimal, region);
}

void Texture::GenerateMipmaps(vk::CommandBuffer cmd_buff) {
  if (mip_levels_ == 1) return;

  const auto& props = c_.GetFormatProperties(format_);
  if (!(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc) ||
      !(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
    throw std::runtime_error("texture image format doesn't support blitting!");

  int32_t mip_width = extent_.width;
  int32_t mip_height = extent_.height;

  for (uint32_t i = 0; i < mip_levels_ - 1; i++) {
    Transition(cmd_buff, vk::ImageLayout::eTransferDstOptimal,
               vk::ImageLayout::eTransferSrcOptimal, i, 1);

    vk::ImageBlit blit{};
    blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    blit.srcOffsets[1] = vk::Offset3D{mip_width, mip_height, 1};
    blit.srcSubresource.aspectMask = aspect_;
    blit.srcSubresource.mipLevel = i;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;

    blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    blit.dstOffsets[1] = vk::Offset3D{mip_width >>= 1, mip_height >>= 1, 1};
    blit.dstSubresource.aspectMask = aspect_;
    blit.dstSubresource.mipLevel = i + 1;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    assert(mip_width != 0 && mip_height != 0);

    cmd_buff.blitImage(image_, vk::ImageLayout::eTransferSrcOptimal, image_,
                       vk::ImageLayout::eTransferDstOptimal, 1, &blit,
                       vk::Filter::eLinear);

    Transition(cmd_buff, vk::ImageLayout::eTransferSrcOptimal,
               vk::ImageLayout::eShaderReadOnlyOptimal, i, 1);
  }
}

}  // namespace npr_graphics
