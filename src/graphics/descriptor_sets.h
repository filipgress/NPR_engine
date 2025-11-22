#ifndef DESCRIPTOR_SETS_H_
#define DESCRIPTOR_SETS_H_

#include "context.h"
#include "resources.h"
#include "image.h"

namespace npr_graphics {
class BaseDescSets : public npr_core::NonCopyable {
 public:
  BaseDescSets(const Context& ctx, uint count) : ctx_{ctx}, count_{count} {}
  virtual ~BaseDescSets() {
    if (layout_) ctx_.GetDevice().destroyDescriptorSetLayout(layout_);
  }

  uint GetCount() const { return count_; }
  vk::DescriptorSet GetSet(uint idx = 0) const { return sets_[idx]; }
  vk::DescriptorSetLayout GetLayout() const { return layout_; }

  void AllocSets(vk::DescriptorPool pool);
  virtual std::vector<vk::DescriptorPoolSize> GetPoolSizes() const = 0;

 protected:
  virtual void CreateLayout() = 0;

 protected:
  const Context& ctx_;

  vk::DescriptorSetLayout layout_{nullptr};
  std::vector<vk::DescriptorSet> sets_;
  uint count_{0};
};

/*
 * SingleTexSets
 */
class SingleTexSets : public BaseDescSets {
 public:
  SingleTexSets(const Context& ctx, uint count) : BaseDescSets{ctx, count} {
    CreateLayout();
  }
  virtual ~SingleTexSets() = default;

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, count_}};
  }

 protected:
  void CreateLayout() override;
  virtual const Texture* GetAttach(const Resources& resrc,
                                   int frame_idx) const = 0;
};

class PresentSets : public SingleTexSets {
 public:
  PresentSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].present_color.get();
  }
};

class AOResSets : public SingleTexSets {
 public:
  AOResSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].ao_res.get();
  }
};

class AOTempSets : public SingleTexSets {
 public:
  AOTempSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].ao_temp.get();
  }
};
/*
 * SingleBuffSets
 */
class SingleBuffSets : public BaseDescSets {
 public:
  SingleBuffSets(const Context& ctx, uint count, vk::DescriptorType type,
                 vk::ShaderStageFlags stage_flags)
      : BaseDescSets{ctx, count}, desc_type_{type}, stage_flags_{stage_flags} {
    CreateLayout();
  }
  virtual ~SingleBuffSets() = default;

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

class CameraUnifSets : public SingleBuffSets {
 public:
  CameraUnifSets(const Context& ctx, uint count)
      : SingleBuffSets{ctx, count, vk::DescriptorType::eUniformBuffer,
                       vk::ShaderStageFlagBits::eVertex |
                           vk::ShaderStageFlagBits::eFragment} {}

  void Update(const Resources& resrc) const;
};

class MaterialUnifSets : public SingleBuffSets {
 public:
  MaterialUnifSets(const Context& ctx, uint count)
      : SingleBuffSets{ctx, count, vk::DescriptorType::eUniformBufferDynamic,
                       vk::ShaderStageFlagBits::eFragment} {}

  void Update(const Resources& resrc) const;
};

/*
 * GBuffSets
 */
class GBuffSets : public BaseDescSets {
 public:
  GBuffSets(const Context& ctx, uint count) : BaseDescSets{ctx, count} {
    CreateLayout();
  }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, 5 * count_}};
  }

 private:
  void CreateLayout() override;
};

/*
 * TextureArraySet
 */
class TextureArraySet : public BaseDescSets {
 public:
  TextureArraySet(const Context& ctx) : BaseDescSets{ctx, 1} { CreateLayout(); }

  void Update(const std::vector<npr_graphics::Texture>& textures,
              const npr_graphics::Texture& default_tex) const;
  void Update(uint idx, const npr_graphics::Texture& texture) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURES}};
  }

 private:
  void CreateLayout() override;
};

/*
 * AOSet
 */
class AOSet : public BaseDescSets {
 public:
  AOSet(const Context& ctx) : BaseDescSets{ctx, 1} { CreateLayout(); }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eUniformBuffer, 1},
            {vk::DescriptorType::eCombinedImageSampler, 1}};
  }

 private:
  void CreateLayout() override;
};

/*
 * ABufferSets
 */
class ABufferSets : public BaseDescSets {
 public:
  ABufferSets(const Context& ctx, uint count) : BaseDescSets{ctx, count} {
    CreateLayout();
  }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eStorageBuffer, 3 * count_}};
  }

 private:
  void CreateLayout() override;
};

/*
 * WBoitInputSets
 */
class WBoitInputSets : public BaseDescSets {
 public:
  WBoitInputSets(const Context& ctx, uint count) : BaseDescSets{ctx, count} {
    CreateLayout();
  }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eInputAttachment, 2 * count_}};
  }

 private:
  void CreateLayout() override;
};

}  // namespace npr_graphics

#endif  // DESCRIPTOR_SETS_H_
