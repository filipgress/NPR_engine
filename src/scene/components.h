#ifndef COMPONENTS_H_
#define COMPONENTS_H_

#include <glm/gtc/quaternion.hpp>

namespace npr_scene {

struct PrimitiveTag {
  /* MeshComp */
  /* MaterialComp */
};

struct ObjectTag {
  /* TransformComp */
};

struct TransformComp {
  glm::vec3 pos{0.0f};
  glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};

  glm::mat4 local_mat;
  glm::mat4 global_mat;
};

struct MeshComp {
  int vbo_idx{-1};
  int ibo_idx{-1};
};

struct MaterialComp {
  int color_map_idx{-1};
  int normal_map_idx{-1};
  int metallic_roughness_map_idx{-1};
  int emissive_map_idx{-1};

  glm::vec4 color_factor{1.0f};
  glm::vec3 emissive_factor{0.0f};
  float metallic_factor{0.0f};
  float roughness_factor{1.0f};

  float alpha_cutoff{0.5f};

  bool double_sided{false};
  bool is_opaque{false};
  bool is_mask{false};
};

}  // namespace npr_scene

#endif  // COMPONENTS_H_
