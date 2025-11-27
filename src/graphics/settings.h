#ifndef SETTINGS_H
#define SETTINGS_H

namespace npr_graphics {
enum class TransparencyMode { kNone, kABuff, kWBoit };
struct RenderSettings {
  vk::Extent2D target_size{500, 400};

  glm::vec3 ambient_color{0.3f, 0.3f, 0.3f};
  float ambient_intensity{0.225f};

  bool enable_ssao{true};
  float ssao_radius{0.5f};
  float ssao_bias{0.025f};

  TransparencyMode trans_mode{TransparencyMode::kABuff};
  float alpha_cutoff{0.001f};

  uint32_t abuff_avg_nodes{4};
  uint32_t abuff_sorted_nodes{8};

  float wboit_alpha_multiplier{10.0f};
  float wboit_alpha_power{3.0};
  float wboit_depth_factor{0.9};
  float wboit_depth_power{3.0};
  float wboit_weight_min{1e-2};
  float wboit_weight_max{3e3};

  bool dirty_target_size{false};
  bool dirty_abuff_size{false};
};
}  // namespace npr_graphics

#endif  // SETTINGS_H
