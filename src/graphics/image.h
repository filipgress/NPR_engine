#ifndef IMAGE_H_
#define IMAGE_H_

#include "context.h"
#include "buffer.h"

namespace npr_graphics {

class Image : public npr_core::NonCopyable {
 public:
  Image(const Context& ctx, vk::Format format, vk::Extent2D extent,
        vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
        vk::SharingMode sharing_mode, vk::SampleCountFlagBits samples,
        uint32_t mip_levels, std::string dbg_name);
  Image(const Context& ctx, vk::Format format, vk::Extent2D extent,
        vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
        vk::SharingMode sharing_mode, vk::SampleCountFlagBits samples,
        uint32_t mip_levels, uint32_t array_layers, std::string dbg_name);

  Image(Image&&) noexcept;
  virtual ~Image();

  vk::ImageView GetImageView() const { return image_view_; }

  void Transition(vk::CommandBuffer cmd_buff, vk::ImageLayout old_layout,
                  vk::ImageLayout new_layout, uint32_t start_mip_level,
                  uint32_t mip_level_count);
  void TransitionLayer(vk::CommandBuffer cmd_buff, uint32_t layer,
                       vk::ImageLayout old_layout, vk::ImageLayout new_layout);

  void Resolve(vk::CommandBuffer cmd_buff, Image& dst,
               vk::ImageLayout src_layout, vk::ImageLayout dst_layout);
  void Clear(vk::CommandBuffer cmd_buff, vk::ClearColorValue clear_color,
             vk::ImageLayout layout = vk::ImageLayout::eTransferDstOptimal);
  void CopyTo(vk::CommandBuffer cmd_buff, Image& dst,
              vk::ImageLayout src_layout, vk::ImageLayout dst_layout);

 private:
  void CreateImage(vk::ImageUsageFlags usage, vk::SampleCountFlagBits samples,
                   vk::SharingMode sharing_mode);
  void CreateImageView();
  void AllocMem();

  uint32_t CalculateMipLevels(uint32_t mip_levels) {
    uint32_t max_mip_levels =
        std::floor(std::log2(std::max(extent_.width, extent_.height))) + 1;
    return std::clamp(mip_levels, 1u, max_mip_levels);
  }

 protected:
  const Context& ctx_;

  vk::Image image_{nullptr};
  vk::DeviceMemory image_mem_{nullptr};
  vk::ImageView image_view_{nullptr};

  vk::Format format_;
  vk::ImageAspectFlags aspect_;
  vk::Extent2D extent_;

  uint32_t mip_levels_;
  uint32_t array_layers_;

  std::string dbg_name_;
};

enum class TextureType { kColor, kNormal, kMetallicRoughness, kEmissive };
struct TextureTypeInfo {
  vk::Format format;
  int component;
  std::string name;
};

struct TextureProps {
  std::string name;
  TextureType type;
  uint32_t width, height;
};

struct SamplerProps {
  vk::Filter mag_filter{vk::Filter::eNearest};
  vk::Filter min_filter{vk::Filter::eNearest};
  vk::SamplerMipmapMode mipmap_mode{vk::SamplerMipmapMode::eLinear};

  vk::SamplerAddressMode address_mode_U{vk::SamplerAddressMode::eClampToBorder};
  vk::SamplerAddressMode address_mode_V{vk::SamplerAddressMode::eClampToBorder};

  float mip_bias{0.0f};
};

class Texture : public Image {
 public:
  Texture(const Context& ctx, vk::Format format, vk::Extent2D extent,
          vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
          vk::SampleCountFlagBits samples, std::string dbg_name,
          const SamplerProps& sampler_props = SamplerProps(),
          uint32_t array_layers = 1);
  Texture(const Context& ctx, const TextureProps& data,
          const SamplerProps& sampler_props = SamplerProps());
  Texture(Texture&&) noexcept;
  ~Texture();

  vk::Sampler GetSampler() const { return sampler_; }
  static TextureTypeInfo GetTypeInfo(TextureType type);

  void DestroyStagingBuff() { staging_buff_.reset(); }
  void Write(vk::CommandBuffer cmd_buff, void* data, size_t elem_size,
             vk::ImageLayout src_layout = vk::ImageLayout::eUndefined);
  void Write(vk::CommandBuffer cmd_buff, const std::vector<unsigned char>& data,
             vk::ImageLayout src_layout = vk::ImageLayout::eUndefined);

 protected:
  void CreateSampler(const SamplerProps& props = SamplerProps());

  void CopyFromBuffer(vk::CommandBuffer cmd_buff);
  void GenerateMipmaps(vk::CommandBuffer cmd_buff);

 protected:
  vk::Sampler sampler_;
  std::unique_ptr<StagingBuffer> staging_buff_;
};

class TextureArray : public Texture {
 public:
  TextureArray(const Context& ctx, vk::Format format, vk::Extent2D extent,
               uint32_t layer_count, vk::ImageUsageFlags usage,
               vk::ImageAspectFlags aspect, const std::string& dbg_name,
               const SamplerProps& sampler_props = SamplerProps());
  TextureArray(TextureArray&& o) noexcept
      : Texture(std::move(o)), layer_layouts_(std::move(o.layer_layouts_)) {}

  ~TextureArray() = default;

  void WriteLayer(vk::CommandBuffer cmd_buff, uint32_t layer, const void* data,
                  vk::DeviceSize size);

 private:
  std::vector<vk::ImageLayout> layer_layouts_;
  std::vector<std::unique_ptr<StagingBuffer>> temp_staging_buffers_;
};

}  // namespace npr_graphics

#endif  // IMAGE_H_
