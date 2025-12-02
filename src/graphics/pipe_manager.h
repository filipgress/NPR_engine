#ifndef PIPE_MANAGER_H_
#define PIPE_MANAGER_H_

#include "descriptor_pool.h"
#include "pipeline_cache.h"
#include "pipeline.h"
#include "pass_manager.h"
#include "shader_manager.h"

namespace npr_graphics {
class PipeManager : public npr_core::NonCopyable {
  friend class Renderer;

 public:
  PipeManager(const Context& ctx, const Resources& resrc,
              const DescriptorPool& desc_pool, const PassManager& passes,
              const ShaderManager& shaders);
  ~PipeManager() = default;

 private:
  void RebuildPipes();

  void BuildGBuff(const VertexShader& vert_shader,
                  const FragmentShader& frag_shader,
                  vk::SampleCountFlagBits samples,
                  const DescriptorPool& desc_pool);

  void BuildSSAO(const VertexShader& vert_shader,
                 const FragmentShader& frag_shader,
                 vk::SampleCountFlagBits samples,
                 const DescriptorPool& desc_pool);
  void BuildBlurSSAO(const VertexShader& vert_shader,
                     const FragmentShader& frag_shader,
                     const DescriptorPool& desc_pool);

  void BuildGlobLight(const VertexShader& vert_shader,
                      const FragmentShader& frag_shader,
                      vk::SampleCountFlagBits samples,
                      const DescriptorPool& desc_pool);
  void BuildLocalLight(const VertexShader& vert_shader,
                       vk::SampleCountFlagBits samples,
                       const DescriptorPool& desc_pool);
  void BuildPointLight(const VertexShader& vert_shader,
                       const FragmentShader& frag_shader,
                       vk::SampleCountFlagBits samples,
                       const DescriptorPool& desc_pool);
  void BuildSpotLight(const VertexShader& vert_shader,
                      const FragmentShader& frag_shader,
                      vk::SampleCountFlagBits samples,
                      const DescriptorPool& desc_pool);

  void BuildABuffFill(const VertexShader& vert_shader,
                      const FragmentShader& frag_shader,
                      vk::SampleCountFlagBits samples,
                      const DescriptorPool& desc_pool);
  void BuildABuffRes(const VertexShader& vert_shader,
                     const FragmentShader& frag_shader,
                     vk::SampleCountFlagBits samples,
                     const DescriptorPool& desc_pool);

  void BuildWBoitAcc(const VertexShader& vert_shader,
                     const FragmentShader& frag_shader,
                     vk::SampleCountFlagBits samples,
                     const DescriptorPool& desc_pool);
  void BuildWBoitRes(const VertexShader& vert_shader,
                     const FragmentShader& frag_shader,
                     const DescriptorPool& desc_pool);

  void BuildBright(const VertexShader& vert_shader,
                   const FragmentShader& frag_shader,
                   const DescriptorPool& desc_pool);
  void BuildBlurColor(const VertexShader& vert_shader,
                      const FragmentShader& frag_shader,
                      const DescriptorPool& desc_pool);
  void BuildBlurColorBlend(const VertexShader& vert_shader,
                           const FragmentShader& frag_shader,
                           const DescriptorPool& desc_pool);

  void BuildCoC(const VertexShader& vert_shader,
                const FragmentShader& frag_shader,
                vk::SampleCountFlagBits samples,
                const DescriptorPool& desc_pool);
  void BuildDof(const VertexShader& vert_shader,
                const FragmentShader& frag_shader,
                const DescriptorPool& desc_pool);

  void BuildSwap(const VertexShader& vert_shader,
                 const FragmentShader& frag_shader,
                 const DescriptorPool& desc_pool);

 private:
  PipelineCache pipe_cache_;

  Pipeline gbuff_;

  Pipeline ssao_;
  Pipeline blur_ssao_;

  Pipeline glob_light_;
  Pipeline local_light_;
  Pipeline point_light_;
  Pipeline spot_light_;

  Pipeline abuff_fill_;
  Pipeline abuff_res_;

  Pipeline wboit_acc_;
  Pipeline wboit_res_;

  Pipeline bright_;
  Pipeline blur_color_;
  Pipeline blur_color_blend_;

  Pipeline coc_;
  Pipeline dof_;

  Pipeline swap_;
};

}  // namespace npr_graphics

#endif  // PIPE_MANAGER_H_
