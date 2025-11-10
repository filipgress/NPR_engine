#include "frustum.h"

namespace npr_scene {

void Frustum::Update(const glm::mat4& proj_view) {
  const glm::mat4& m = proj_view;

  // left plane
  planes_[0].x = m[0][3] + m[0][0];
  planes_[0].y = m[1][3] + m[1][0];
  planes_[0].z = m[2][3] + m[2][0];
  planes_[0].w = m[3][3] + m[3][0];

  // right plane
  planes_[1].x = m[0][3] - m[0][0];
  planes_[1].y = m[1][3] - m[1][0];
  planes_[1].z = m[2][3] - m[2][0];
  planes_[1].w = m[3][3] - m[3][0];

  // bottom plane
  planes_[2].x = m[0][3] + m[0][1];
  planes_[2].y = m[1][3] + m[1][1];
  planes_[2].z = m[2][3] + m[2][1];
  planes_[2].w = m[3][3] + m[3][1];

  // top plane
  planes_[3].x = m[0][3] - m[0][1];
  planes_[3].y = m[1][3] - m[1][1];
  planes_[3].z = m[2][3] - m[2][1];
  planes_[3].w = m[3][3] - m[3][1];

  // near plane
  planes_[4].x = m[0][3] + m[0][2];
  planes_[4].y = m[1][3] + m[1][2];
  planes_[4].z = m[2][3] + m[2][2];
  planes_[4].w = m[3][3] + m[3][2];

  // far plane
  planes_[5].x = m[0][3] - m[0][2];
  planes_[5].y = m[1][3] - m[1][2];
  planes_[5].z = m[2][3] - m[2][2];
  planes_[5].w = m[3][3] - m[3][2];

  for (auto& plane : planes_) {
    float length = glm::length(glm::vec3(plane.x, plane.y, plane.z));
    if (length > 0.0f) plane /= length;
    plane.w *= scale_;
  }
}

bool Frustum::IsVisible(const BoundingBoxComp& bb) const {
  for (const auto& plane : planes_) {
    auto world_norm = glm::vec3(plane);
    float dist = plane.w + glm::dot(world_norm, bb.center);

    glm::vec3 local_normal = bb.inv_rot * world_norm;
    float projection = bb.extent.x * std::abs(local_normal.x) +
                       bb.extent.y * std::abs(local_normal.y) +
                       bb.extent.z * std::abs(local_normal.z);

    if (dist + projection < 0.0f) return false;
  }

  return true;
}

}  // namespace npr_scene
