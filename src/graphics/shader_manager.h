#ifndef SHADER_MANAGER_H_
#define SHADER_MANAGER_H_

#include "shader.h"

namespace npr_graphics {
class ShaderManager : public npr_core::NonCopyable {
  friend class Renderer;
  friend class PipeManager;

 public:
  ShaderManager(const Context& ctx);

  void Recompile();
  bool IsDirty();

 private:
  VertexShader quad_vert_;
  VertexShader gbuff_vert_;
  VertexShader light_vert_;

  FragmentShader gbuff_frag_;
  FragmentShader ao_frag_;
  FragmentShader blur_frag_;
  FragmentShader dir_light_frag_;
  FragmentShader point_light_frag_;
  FragmentShader spot_light_frag_;
  FragmentShader abuff_fill_frag_;
  FragmentShader abuff_res_frag_;
  FragmentShader wboit_acc_frag_;
  FragmentShader wboit_res_frag_;
  FragmentShader bright_extract_frag_;
  FragmentShader coc_extract_frag_;
  FragmentShader dof_poisson_frag_;
  FragmentShader swap_frag_;

  std::vector<Shader*> shaders_;
};
}  // namespace npr_graphics

#endif  // SHADER_MANAGER_H_
