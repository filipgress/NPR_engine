#ifndef DESCRIPTOR_POOL_H_
#define DESCRIPTOR_POOL_H_

#include "vulkan_context.h"
#include "descriptor_sets.h"

namespace npr_graphics {

class Resources;
class DescriptorPool : public npr_core::NonCopyable {
 public:
  DescriptorPool(const VulkanContext& context, const Resources& res);
  ~DescriptorPool() {
    if (pool_) c_.GetDevice().destroyDescriptorPool(pool_);
  }

  const GBuffSets& GetGBuffSets() const { return gbuff_sets_; }
  const CameraUnifSets& GetCameraSets() const { return camera_sets_; }
  const MaterialUnifSets& GetMaterialSets() const { return material_sets_; }
  const PresentSets& GetPresentSets() const { return present_sets_; }

 private:
  void CreateDescriptorPool();

 private:
  const VulkanContext& c_;

  vk::DescriptorPool pool_{nullptr};

  GBuffSets gbuff_sets_;
  CameraUnifSets camera_sets_;
  MaterialUnifSets material_sets_;
  PresentSets present_sets_;
};

class TexDescriptorPool : public npr_core::NonCopyable {
 public:
  TexDescriptorPool(const VulkanContext& context);
  ~TexDescriptorPool() {
    if (pool_) c_.GetDevice().destroyDescriptorPool(pool_);
  }

  const TextureArraySet& GetTextureSet() const { return tex_set_; }
  void Update(const std::vector<npr_graphics::Texture>& textures) {
    tex_set_.Update(textures);
  }

 private:
  void CreateDescriptorPool();

 private:
  const VulkanContext& c_;

  vk::DescriptorPool pool_{nullptr};
  TextureArraySet tex_set_{c_};
};

}  // namespace npr_graphics

#endif  // DESCRIPTOR_POOL_H_
