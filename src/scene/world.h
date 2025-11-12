#ifndef WORLD_H_
#define WORLD_H_

#include "components.h"
#include "frustum.h"

#include "graphics/resources.h"
#include <flecs.h>

namespace npr_scene {

struct GpuResources;
struct MeshMaterialKey {
  MeshComp mesh;
  npr_graphics::MaterialUnif material;

  bool operator==(const MeshMaterialKey& other) const {
    return mesh.vbo_idx == other.mesh.vbo_idx &&
           mesh.ibo_idx == other.mesh.ibo_idx &&
           material.maps == other.material.maps &&
           material.color_factor == other.material.color_factor &&
           material.emissive_factor == other.material.emissive_factor &&
           material.metallic_factor == other.material.metallic_factor &&
           material.roughness_factor == other.material.roughness_factor &&
           material.alpha_cutoff == other.material.alpha_cutoff &&
           material.flags == other.material.flags;
  }
};

struct MeshMaterialHash {
  size_t operator()(const MeshMaterialKey& key) const {
    return std::hash<std::string_view>{}(std::string_view(
        reinterpret_cast<const char*>(&key), sizeof(MeshMaterialKey)));
  }
};

struct PerInstanceData {
  TransformComp tf;
  BoundingBoxComp bb;
};

class World : npr_core::NonCopyable {
  using KeyPredFn = std::function<bool(const MeshMaterialKey&)>;

  friend class SceneLoader;
  friend class Scene;

 private:
  void Update();
  void Reset();
  void BuildQueries();

  void Record(vk::CommandBuffer cmd_buff, vk::PipelineLayout layout,
              const npr_graphics::FrameResources& frame_res, uint32_t set_idx,
              vk::DescriptorSet material_set, const Frustum& frustum,
              const GpuResources& gpu_res, bool set_culling,
              KeyPredFn pred) const;

  void UpdateTransforms();
  void UpdateBoundingBoxes();
  void UpdateInstances();

 private:
  flecs::query<const CameraTag, const TransformComp> camera_query_;
  flecs::query<TransformComp,         // transform to update
               const TransformComp*>  // parent transform
      transform_query_;
  flecs::query<const TransformComp,  // parent transform
               const PrimitiveTag,   // tag to identify primitives
               const MeshComp,       // mesh data
               const MaterialComp,
               BoundingBoxComp>  // material data
      renderable_query_;
  flecs::world entities_;

  std::unordered_map<MeshMaterialKey, std::vector<PerInstanceData>,
                     MeshMaterialHash>
      instances_;

  static uint material_at;
  static uint instance_at;
};

}  // namespace npr_scene

#endif  // WORLD_H_
