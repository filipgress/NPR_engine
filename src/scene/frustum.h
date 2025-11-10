#ifndef FRUSTUM_H_
#define FRUSTUM_H_

#include "components.h"
namespace npr_scene {

class Frustum {
 public:
  void Update(const glm::mat4& proj_view);
  bool IsVisible(const BoundingBoxComp& bb) const;

 private:
  std::array<glm::vec4, 6> planes_;
  float scale_{1.0f};
};
}  // namespace npr_scene

#endif  // FRUSTUM_H_
