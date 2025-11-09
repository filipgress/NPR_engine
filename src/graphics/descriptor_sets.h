#ifndef DESCRIPTOR_SETS_H_
#define DESCRIPTOR_SETS_H_

#include "vulkan_context.h"
#include "resources.h"
#include "image.h"

namespace npr_graphics {
class Resources;
class DescriptorSets : public npr_core::NonCopyable {
 public:
  DescriptorSets(const VulkanContext& context, uint count)
      : c_{context}, count_{count} {}
  virtual ~DescriptorSets() {
    if (layout_) c_.GetDevice().destroyDescriptorSetLayout(layout_);
  }

  uint GetCount() const { return count_; }
  vk::DescriptorSet GetSet(uint idx = 0) const { return sets_[idx]; }
  vk::DescriptorSetLayout GetLayout() const { return layout_; }

  void AllocSets(vk::DescriptorPool pool);
  virtual std::vector<vk::DescriptorPoolSize> GetPoolSizes() const = 0;

 protected:
  virtual void CreateLayout() = 0;

 protected:
  const VulkanContext& c_;

  vk::DescriptorSetLayout layout_{nullptr};
  std::vector<vk::DescriptorSet> sets_;
  uint count_{0};
};

class GBuffSets : public DescriptorSets {
 public:
  GBuffSets(const VulkanContext& context, uint count)
      : DescriptorSets{context, count} {
    CreateLayout();
  }

  void Update(const Resources& res) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, 5 * count_}};
  }

 private:
  void CreateLayout() override;
};

class TextureSets : public DescriptorSets {
 public:
  TextureSets(const VulkanContext& context, uint count)
      : DescriptorSets{context, count} {
    CreateLayout();
  }
  virtual ~TextureSets() = default;

  void Update(const Resources& res) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, count_}};
  }

 protected:
  void CreateLayout() override;
  virtual const Texture* GetAttach(const Resources& res,
                                   int frame_idx) const = 0;
};

class PresentSets : public TextureSets {
 public:
  PresentSets(const VulkanContext& context, uint count)
      : TextureSets{context, count} {}

  const Texture* GetAttach(const Resources& res, int frame_idx) const override {
    return res.GetResources()[frame_idx].present_color.get();
  }
};

class TextureArraySet : public DescriptorSets {
 public:
  TextureArraySet(const VulkanContext& context) : DescriptorSets{context, 1} {
    CreateLayout();
  }

  void Update(const std::vector<npr_graphics::Texture>& textures,
              const npr_graphics::Texture& default_tex) const;
  void Update(uint idx, const npr_graphics::Texture& texture) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURES}};
  }

 private:
  void CreateLayout() override;
};

class BufferSets : public DescriptorSets {
 public:
  BufferSets(const VulkanContext& context, uint count, vk::DescriptorType type,
             vk::ShaderStageFlags stage_flags)
      : DescriptorSets{context, count},
        desc_type_{type},
        stage_flags_{stage_flags} {
    CreateLayout();
  }
  virtual ~BufferSets() = default;

  void Update(vk::Buffer buffer, vk::DeviceSize range, uint idx) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{desc_type_, count_}};
  }

 protected:
  void CreateLayout() override;

 protected:
  vk::DescriptorType desc_type_;
  vk::ShaderStageFlags stage_flags_;
};

class CameraUnifSets : public BufferSets {
 public:
  CameraUnifSets(const VulkanContext& context, uint count)
      : BufferSets{context, count, vk::DescriptorType::eUniformBuffer,
                   vk::ShaderStageFlagBits::eVertex |
                       vk::ShaderStageFlagBits::eFragment} {}

  void Update(const Resources& res) const;
};

class MaterialUnifSets : public BufferSets {
 public:
  MaterialUnifSets(const VulkanContext& context, uint count)
      : BufferSets{context, count, vk::DescriptorType::eUniformBufferDynamic,
                   vk::ShaderStageFlagBits::eFragment} {}

  void Update(const Resources& res) const;
};

}  // namespace npr_graphics

#endif  // DESCRIPTOR_SETS_H_
