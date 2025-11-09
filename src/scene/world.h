#ifndef WORLD_H_
#define WORLD_H_

#include "components.h"
#include "graphics/descriptor_pool.h"

#include <flecs.h>

namespace npr_scene {

struct GpuResources;
struct MeshMaterialKey {
  MeshComp mesh;
  npr_graphics::MaterialUnif material;

  bool operator==(const MeshMaterialKey& other) const {
    return mesh.vbo_idx == other.mesh.vbo_idx &&
           mesh.ibo_idx == other.mesh.ibo_idx;
    material.maps == other.material.maps&& material.color_factor ==
        other.material.color_factor&& material.emissive_factor ==
        other.material.emissive_factor&& material.metallic_factor ==
        other.material.metallic_factor&& material.roughness_factor ==
        other.material.roughness_factor&& material.alpha_cutoff ==
        other.material.alpha_cutoff&& material.flags == other.material.flags;
  }
};

struct MeshMaterialHash {
  size_t operator()(const MeshMaterialKey& key) const {
    return std::hash<std::string_view>{}(std::string_view(
        reinterpret_cast<const char*>(&key), sizeof(MeshMaterialKey)));
  }
};

class World : npr_core::NonCopyable {
  friend class SceneLoader;
  friend class Scene;

 private:
  void Update();
  void Reset();
  void BuildQueries();

  void RecordOpaque(vk::CommandBuffer cmd_buff, vk::PipelineLayout layout,
                    uint frame_idx, const npr_graphics::Resources& res,
                    const GpuResources& gpu_res,
                    const npr_graphics::DescriptorPool& desc_pool) const;

  void UpdateTransforms();
  void UpdateInstances();

 private:
  flecs::world entities_;
  std::unordered_map<MeshMaterialKey, std::vector<npr_graphics::Instance>,
                     MeshMaterialHash>
      instances_;

  flecs::query<const CameraTag, const TransformComp> camera_query_;
  flecs::query<TransformComp,         // transform to update
               const TransformComp*>  // parent transform
      transform_query_;
  flecs::query<const TransformComp,  // parent transform
               const PrimitiveTag,   // tag to identify primitives
               const MeshComp,       // mesh data
               const MaterialComp>   // material data
      render_opaque_query_;

  static uint material_at;
  static uint instance_at;
};

}  // namespace npr_scene

#endif  // WORLD_H_
