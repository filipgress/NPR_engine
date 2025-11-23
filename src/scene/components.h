#ifndef COMPONENTS_H_
#define COMPONENTS_H_

#include <glm/gtc/quaternion.hpp>

namespace npr_scene {

struct PrimitiveTag {
  /* MeshComp */
  /* MaterialComp */
  /* BoundingBoxComp */
};

struct ObjectTag {
  /* TransformComp */
};

struct CameraTag {
  /* TransformComp */
  /* PerspectiveCameraComp or OrthographicCameraComp */
};

struct DirLightTag {
  /* TransformComp */
  /* LightComp */
};

struct SpotLightTag {
  /* TransformComp */
  /* LightComp */
  /* SpotComp */
  /* RangeComp */
  /* BoundingBoxComp */
};

struct PointLightTag {
  /* TransformComp */
  /* LightComp */
  /* RangeComp */
  /* BoundingBoxComp */
};

struct TransformComp {
  glm::vec3 pos{0.0f};
  glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};

  glm::mat4 local_mat;
  glm::mat4 glob_mat;

  bool dirty{true};
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

struct BoundingBoxComp {
  glm::vec3 center{0.0f};
  glm::vec3 extent{1.0f};
  glm::mat3 inv_rot{1.0f};

  glm::vec3 min_pos{std::numeric_limits<float>::max()};
  glm::vec3 max_pos{std::numeric_limits<float>::min()};
};

struct PerspectiveComp {
  float aspect{-1.0f};
  float fov{glm::pi<float>() / 4.0f};
  float near{0.01f};
  float far{100.0f};
};

struct OrthographicComp {
  float xmag{1.0f};
  float ymag{1.0f};
  float near{0.01f};
  float far{100.0f};
};

struct LightComp {
  glm::vec3 color{1.0f};
  float intensity{1.0f};
};

struct RangeComp {
  float range{0.0f};  // 0 = inf
};

struct SpotComp {
  float inner_cone_angle{0.0f};
  float outer_cone_angle{0.0f};
};

}  // namespace npr_scene

#endif  // COMPONENTS_H_
