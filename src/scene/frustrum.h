#ifndef FRUSTRUM_H_
#define FRUSTRUM_H_

namespace npr_scene {

struct BoundingBox {
  std::array<glm::vec4, 8> corners;
  glm::vec3 min_pos{std::numeric_limits<float>::max()};
  glm::vec3 max_pos{std::numeric_limits<float>::min()};

  BoundingBox() = default;
  BoundingBox(glm::vec3 min_pos, glm::vec3 max_pos)
      : min_pos{min_pos}, max_pos{max_pos} {}
  void Update(const glm::mat4& model);
};

class Frustrum {
 public:
  void Update(const glm::mat4& view_proj);
  bool IsVisible(const BoundingBox& bb) const;

 private:
  std::array<glm::vec4, 6> faces_;
  float scale_{1.0f};
};

}  // namespace npr_scene

#endif  // FRUSTRUM_H_
