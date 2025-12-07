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

  virtual void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, count_}};
  }

 protected:
  void CreateLayout() override;
  virtual const Texture* GetAttach(const Resources& resrc,
                                   int frame_idx) const = 0;
};

class ColorSets : public SingleTexSets {
 public:
  ColorSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].color_res.get();
  }
};

class PresentColorSets : public SingleTexSets {
 public:
  PresentColorSets(const Context& ctx, uint count)
      : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].present_color.get();
  }
};

// ao
class AOResSets : public SingleTexSets {
 public:
  AOResSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].ssao_res.get();
  }
};

class AOTempSets : public SingleTexSets {
 public:
  AOTempSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].ssao_temp.get();
  }
};

// bloom
class BrightColorSets : public SingleTexSets {
 public:
  BrightColorSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].bright_color.get();
  }
};

class BrightTempSets : public SingleTexSets {
 public:
  BrightTempSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].bright_temp.get();
  }
};

// coc
class DepthSets : public SingleTexSets {
 public:
  DepthSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  void Update(const Resources& resrc) const override final;
  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].ds_ms.get();
  }
};

class CocMapSets : public SingleTexSets {
 public:
  CocMapSets(const Context& ctx, uint count) : SingleTexSets{ctx, count} {}

  const Texture* GetAttach(const Resources& resrc,
                           int frame_idx) const override {
    return resrc.GetResrc()[frame_idx].coc_map.get();
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

class DirLightUnifSets : public SingleBuffSets {
 public:
  DirLightUnifSets(const Context& ctx, uint count)
      : SingleBuffSets{ctx, count, vk::DescriptorType::eUniformBuffer,
                       vk::ShaderStageFlagBits::eFragment} {}

  void Update(const Resources& resrc) const;
};

class PointLightUnifSets : public SingleBuffSets {
 public:
  PointLightUnifSets(const Context& ctx, uint count)
      : SingleBuffSets{ctx, count, vk::DescriptorType::eUniformBufferDynamic,
                       vk::ShaderStageFlagBits::eFragment} {}

  void Update(const Resources& resrc) const;
};

class SpotLightUnifSets : public SingleBuffSets {
 public:
  SpotLightUnifSets(const Context& ctx, uint count)
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
    return {{vk::DescriptorType::eCombinedImageSampler, kMaxTextures}};
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

/*
 * DofSets
 */
class DofSet : public BaseDescSets {
 public:
  DofSet(const Context& ctx) : BaseDescSets{ctx, 1} { CreateLayout(); }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, count_},  // blue_noise
            {vk::DescriptorType::eUniformBuffer, count_}};  // poisson kernel
  }

 private:
  void CreateLayout() override;
};

/*
 * DitherNoiseSets
 */
class DitherNoiseSets : public BaseDescSets {
 public:
  DitherNoiseSets(const Context& ctx) : BaseDescSets{ctx, 4} { CreateLayout(); }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, 2 * count_}};
  }

 private:
  void CreateLayout() override;
};

/*
 * PaletteSets
 */
class PaletteSets : public BaseDescSets {
 public:
  PaletteSets(const Context& ctx, uint count) : BaseDescSets{ctx, count} {
    CreateLayout();
  }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, count_}};
  }

 private:
  void CreateLayout() override;
};

/*
 * HatchingArraySets - 4 descriptor sets, one for each hatching type
 * Set 0: Hatch textures (9 levels)
 * Set 1: Cross-hatch textures (9 levels)
 * Set 2: Scribble textures (9 levels)
 * Set 3: Stipple textures (9 levels)
 */
class HatchingArraySets : public BaseDescSets {
 public:
  HatchingArraySets(const Context& ctx) : BaseDescSets{ctx, 4} {
    CreateLayout();
  }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler, 4 * kHatchLevels}};
  }

 private:
  void CreateLayout() override;
};

class StylizedShadingSets : public BaseDescSets {
 public:
  StylizedShadingSets(const Context& ctx, uint count)
      : BaseDescSets{ctx, count} {
    CreateLayout();
  }

  void Update(const Resources& resrc) const;
  std::vector<vk::DescriptorPoolSize> GetPoolSizes() const override {
    return {{vk::DescriptorType::eCombinedImageSampler,
             2 * count_}};  // 2 textures per set
  }

 private:
  void CreateLayout() override;
};

}  // namespace npr_graphics

#endif  // DESCRIPTOR_SETS_H_
