#ifndef COMPONENTS_H_
#define COMPONENTS_H_

#include <glm/gtc/quaternion.hpp>

namespace npr_scene {

struct ObjectTag {};
struct LightTag {};
struct CameraTag {};

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

}  // namespace npr_scene

#endif  // COMPONENTS_H_
