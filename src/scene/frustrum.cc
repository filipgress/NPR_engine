#include "frustrum.h"

namespace npr_scene {

void BoundingBox::Update(const glm::mat4& model) {
  corners[0] = glm::vec4(min_pos.x, min_pos.y, min_pos.z, 1.0f);
  corners[1] = glm::vec4(max_pos.x, min_pos.y, min_pos.z, 1.0f);
  corners[2] = glm::vec4(min_pos.x, max_pos.y, min_pos.z, 1.0f);
  corners[3] = glm::vec4(max_pos.x, max_pos.y, min_pos.z, 1.0f);
  corners[4] = glm::vec4(min_pos.x, min_pos.y, max_pos.z, 1.0f);
  corners[5] = glm::vec4(max_pos.x, min_pos.y, max_pos.z, 1.0f);
  corners[6] = glm::vec4(min_pos.x, max_pos.y, max_pos.z, 1.0f);
  corners[7] = glm::vec4(max_pos.x, max_pos.y, max_pos.z, 1.0f);

  // to world-space
  for (auto& corner : corners) corner = model * corner;
}

void Frustrum::Update(const glm::mat4& view_proj) {
  // Left plane
  faces_[0].x = view_proj[0][3] + view_proj[0][0];
  faces_[0].y = view_proj[1][3] + view_proj[1][0];
  faces_[0].z = view_proj[2][3] + view_proj[2][0];
  faces_[0].w = view_proj[3][3] + view_proj[3][0];

  // Right plane
  faces_[1].x = view_proj[0][3] - view_proj[0][0];
  faces_[1].y = view_proj[1][3] - view_proj[1][0];
  faces_[1].z = view_proj[2][3] - view_proj[2][0];
  faces_[1].w = view_proj[3][3] - view_proj[3][0];

  // Bottom plane
  faces_[2].x = view_proj[0][3] + view_proj[0][1];
  faces_[2].y = view_proj[1][3] + view_proj[1][1];
  faces_[2].z = view_proj[2][3] + view_proj[2][1];
  faces_[2].w = view_proj[3][3] + view_proj[3][1];

  // Top plane
  faces_[3].x = view_proj[0][3] - view_proj[0][1];
  faces_[3].y = view_proj[1][3] - view_proj[1][1];
  faces_[3].z = view_proj[2][3] - view_proj[2][1];
  faces_[3].w = view_proj[3][3] - view_proj[3][1];

  // Near plane
  faces_[4].x = view_proj[0][3] + view_proj[0][2];
  faces_[4].y = view_proj[1][3] + view_proj[1][2];
  faces_[4].z = view_proj[2][3] + view_proj[2][2];
  faces_[4].w = view_proj[3][3] + view_proj[3][2];

  // Far plane
  faces_[5].x = view_proj[0][3] - view_proj[0][2];
  faces_[5].y = view_proj[1][3] - view_proj[1][2];
  faces_[5].z = view_proj[2][3] - view_proj[2][2];
  faces_[5].w = view_proj[3][3] - view_proj[3][2];

  for (auto& plane : faces_) {
    plane /= glm::length(plane);
    plane.w *= scale_;
  }
}

bool Frustrum::IsVisible(const BoundingBox& bb) const {
  for (const auto& face : faces_) {
    bool all_out = true;

    for (const auto& corner : bb.corners) {
      float dist = glm::dot(face, corner);

      if (dist >= 0.0f) {
        all_out = false;
        break;
      }
    }

    if (all_out) return false;
  }

  return true;
}

}  // namespace npr_scene
