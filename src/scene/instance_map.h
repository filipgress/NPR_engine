#ifndef INSTANCE_MAP_H_
#define INSTANCE_MAP_H_

#include "components.h"

namespace npr_scene {

struct MeshMaterialKey {
  MeshComp mesh;
  MaterialComp mat;

  bool operator==(const MeshMaterialKey& other) const {
    return mesh.vbo_idx == other.mesh.vbo_idx &&
           mesh.ibo_idx == other.mesh.ibo_idx &&

           mat.color_map_idx == other.mat.color_map_idx &&
           mat.normal_map_idx == other.mat.normal_map_idx &&
           mat.metallic_roughness_map_idx ==
               other.mat.metallic_roughness_map_idx &&
           mat.emissive_map_idx == other.mat.emissive_map_idx &&

           mat.color_factor == other.mat.color_factor &&
           mat.emissive_factor == other.mat.emissive_factor &&
           mat.metallic_factor == other.mat.metallic_factor &&
           mat.roughness_factor == other.mat.roughness_factor &&

           mat.alpha_cutoff == other.mat.alpha_cutoff &&

           mat.double_sided == other.mat.double_sided &&
           mat.is_opaque == other.mat.is_opaque &&
           mat.is_mask == other.mat.is_mask;
  }
};

struct MeshMaterialHash {
  size_t operator()(const MeshMaterialKey& key) const {
    using npr_core::HashCombine;
    size_t seed = 0;

    // Hash Mesh IDs
    HashCombine(seed, std::hash<int>{}(key.mesh.vbo_idx));
    HashCombine(seed, std::hash<int>{}(key.mesh.ibo_idx));

    // Hash Texture IDs
    HashCombine(seed, std::hash<int>{}(key.mat.color_map_idx));
    HashCombine(seed, std::hash<int>{}(key.mat.normal_map_idx));
    HashCombine(seed, std::hash<int>{}(key.mat.metallic_roughness_map_idx));
    HashCombine(seed, std::hash<int>{}(key.mat.emissive_map_idx));

    uint32_t flags = 0;
    if (key.mat.double_sided) flags |= BIT(1);
    if (key.mat.is_opaque) flags |= BIT(2);
    if (key.mat.is_mask) flags |= BIT(3);
    HashCombine(seed, flags);

    return seed;
  }
};

struct PerInstanceData {
  TransformComp tf;
  BoundingBoxComp bb;
};

using InstanceMap =
    std::unordered_map<MeshMaterialKey, std::vector<PerInstanceData>,
                       MeshMaterialHash>;

}  // namespace npr_scene

#endif  // INSTANCE_MAP_H_
