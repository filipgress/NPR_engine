#ifndef DESCRIPTOR_POOL_H_
#define DESCRIPTOR_POOL_H_

#include "context.h"
#include "descriptor_sets.h"

namespace npr_graphics {

class Resources;
class DescriptorPool : public npr_core::NonCopyable {
 public:
  DescriptorPool(const Context& ctx, const Resources& resrc);
  ~DescriptorPool() {
    if (pool_) ctx_.GetDevice().destroyDescriptorPool(pool_);
  }

  const GBuffSets& GetGBuffSets() const { return gbuff_sets_; }
  const CameraUnifSets& GetCameraSets() const { return camera_sets_; }
  const MaterialUnifSets& GetMaterialSets() const { return material_sets_; }
  const AOSet& GetAOSet() const { return ao_set_; }
  const AOResSets& GetAOResSets() const { return ao_res_sets_; }
  const AOTempSets& GetAOTempSets() const { return ao_temp_sets_; }
  const ABufferSets& GetABufferSets() const { return abuff_sets_; }
  const WBoitInputSets& GetWBoitInputSets() const { return wboit_input_sets_; }
  const PresentSets& GetPresentSets() const { return present_sets_; }

 private:
  void CreateDescriptorPool();

 private:
  const Context& ctx_;

  vk::DescriptorPool pool_{nullptr};

  GBuffSets gbuff_sets_;
  CameraUnifSets camera_sets_;
  MaterialUnifSets material_sets_;

  AOSet ao_set_;
  AOResSets ao_res_sets_;
  AOTempSets ao_temp_sets_;

  ABufferSets abuff_sets_;
  WBoitInputSets wboit_input_sets_;
  PresentSets present_sets_;
};

class TexDescriptorPool : public npr_core::NonCopyable {
 public:
  TexDescriptorPool(const Context& ctx, const std::string& dbg_name);
  ~TexDescriptorPool() {
    if (pool_) ctx_.GetDevice().destroyDescriptorPool(pool_);
  }

  const TextureArraySet& GetTextureSet() const { return tex_set_; }

 private:
  const Context& ctx_;

  vk::DescriptorPool pool_{nullptr};
  TextureArraySet tex_set_{ctx_};
};

class ImGuiDescriptorPool : public npr_core::NonCopyable {
 public:
  ImGuiDescriptorPool(const Context& ctx);
  ~ImGuiDescriptorPool() {
    if (pool_) ctx_.GetDevice().destroyDescriptorPool(pool_);
  }

  vk::DescriptorPool GetPool() const { return pool_; }

 private:
  const Context& ctx_;
  vk::DescriptorPool pool_{nullptr};
};

}  // namespace npr_graphics

#endif  // DESCRIPTOR_POOL_H_
