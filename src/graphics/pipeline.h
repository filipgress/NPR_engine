#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "context.h"
#include "pipeline_cache.h"
#include "render_pass.h"
#include "shader.h"
#include "descriptor_pool.h"

namespace npr_graphics {

struct PushConstInfo {
  size_t size;
  vk::ShaderStageFlags stage;
};

struct SpecConstInfo {
  uint32_t constant_id;
  size_t size;
  const void* data;
};

struct ShaderStageInfo {
  const Shader& shader;
  std::vector<SpecConstInfo> spec_consts;

  std::string entry = "main";
  uint32_t built_ver{0};
};

struct PipelineState {
  std::vector<ShaderStageInfo> shaders;

  std::vector<vk::VertexInputBindingDescription> vertex_bindings;
  std::vector<vk::VertexInputAttributeDescription> vertex_attribs;

  vk::PipelineInputAssemblyStateCreateInfo input_assembly;
  vk::PipelineViewportStateCreateInfo viewport;
  vk::PipelineRasterizationStateCreateInfo rasterization;
  vk::PipelineMultisampleStateCreateInfo multisample;
  vk::PipelineDepthStencilStateCreateInfo depth_stencil;

  std::vector<vk::PipelineColorBlendAttachmentState> color_attachments;
  std::vector<vk::DynamicState> dynamic_states{vk::DynamicState::eViewport,
                                               vk::DynamicState::eScissor};

  uint32_t subpass{0};

  PipelineState();
};

class Pipeline : public npr_core::NonCopyable {
 public:
  Pipeline(const Context& ctx, const PipelineCache& cache,
           const BasePass& render_pass);
  virtual ~Pipeline();

  vk::Pipeline GetPipeline() const { return pipeline_; }
  vk::PipelineLayout GetLayout() const { return layout_; }

  bool IsUpToDate() const;
  void BuildPipeline();

 protected:
  void CreateLayout(const std::vector<vk::DescriptorSetLayout>& sets,
                    const std::vector<PushConstInfo>& push_consts = {});

  void AddShader(const Shader& shader,
                 const std::vector<SpecConstInfo>& spec_consts = {},
                 const std::string& entry = "main") {
    state_.shaders.push_back({shader, spec_consts, entry});
  }
  void AddVertexBinding(
      uint32_t binding, uint32_t stride,
      vk::VertexInputRate input_rate = vk::VertexInputRate::eVertex) {
    state_.vertex_bindings.push_back({binding, stride, input_rate});
  }
  void AddVertexAttribute(uint32_t location, uint32_t binding,
                          vk::Format format, uint32_t offset) {
    state_.vertex_attribs.push_back({location, binding, format, offset});
  }

  virtual const std::string GetDbgName() const = 0;

  template <typename T>
  static PushConstInfo MakePushConst(vk::ShaderStageFlags stages) {
    return {sizeof(T), stages};
  }
  template <typename T>
  static SpecConstInfo MakeSpecConst(uint32_t constant_id, const T& value) {
    return {constant_id, sizeof(T), &value};
  }

 protected:
  const Context& ctx_;
  const PipelineCache& cache_;
  const BasePass& render_pass_;

  vk::Pipeline pipeline_{nullptr};
  vk::PipelineLayout layout_{nullptr};

  PipelineState state_;
};

class GBuffPipe : public Pipeline {
 public:
  GBuffPipe(const Context& ctx, const PipelineCache& cache,
            const GBuffPass& render_pass, VertexShader& vert_shader,
            FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
            const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "gbuff_pipe"; }
};

class AOPipe : public Pipeline {
 public:
  AOPipe(const Context& ctx, const PipelineCache& cache,
         const AOPass& render_pass, VertexShader& vert_shader,
         FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
         const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "ao_pipe"; }
};

class AOBlurPipe : public Pipeline {
 public:
  AOBlurPipe(const Context& ctx, const PipelineCache& cache,
             const BlurPass& render_pass, VertexShader& vert_shader,
             FragmentShader& frag_shader, const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "ao_blur_pipe"; }
};

class GlobLightPipe : public Pipeline {
 public:
  GlobLightPipe(const Context& ctx, const PipelineCache& cache,
                const GlobLightPass& render_pass, VertexShader& vert_shader,
                FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "global_light_pipe"; }
};

class LocalLightPipe : public Pipeline {
 public:
  LocalLightPipe(const Context& ctx, const PipelineCache& cache,
                 const LocalLightPass& render_pass, VertexShader& vert_shader,
                 vk::SampleCountFlagBits samples,
                 const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "local_light_pipe"; }
};

class PointLightPipe : public Pipeline {
 public:
  PointLightPipe(const Context& ctx, const PipelineCache& cache,
                 const LocalLightPass& render_pass, VertexShader& vert_shader,
                 FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                 const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "point_light_pipe"; }
};

class SpotLightPipe : public Pipeline {
 public:
  SpotLightPipe(const Context& ctx, const PipelineCache& cache,
                const LocalLightPass& render_pass, VertexShader& vert_shader,
                FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "spot_light_pipe"; }
};

class ABuffFillPipe : public Pipeline {
 public:
  ABuffFillPipe(const Context& ctx, const PipelineCache& cache,
                const ABuffPass& render_pass, VertexShader& vert_shader,
                FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "abuff_fill_pipe"; }
};

class ABuffResolvePipe : public Pipeline {
 public:
  ABuffResolvePipe(const Context& ctx, const PipelineCache& cache,
                   const ABuffPass& render_pass, VertexShader& vert_shader,
                   FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                   const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "abuff_resolve_pipe"; }
};

class WBoitAccPipe : public Pipeline {
 public:
  WBoitAccPipe(const Context& ctx, const PipelineCache& cache,
               const WBoitPass& render_pass, VertexShader& vert_shader,
               FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
               const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "wboit_acc_pipe"; }
};

class WBoitResolvePipe : public Pipeline {
 public:
  WBoitResolvePipe(const Context& ctx, const PipelineCache& cache,
                   const WBoitPass& render_pass, VertexShader& vert_shader,
                   FragmentShader& frag_shader,
                   const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "wboit_resolve_pipe"; }
};

class SwapPipe : public Pipeline {
 public:
  SwapPipe(const Context& ctx, const PipelineCache& cache,
           const SwapPass& render_pass, VertexShader& vert_shader,
           FragmentShader& frag_shader, const DescriptorPool& desc_pool);

 private:
  const std::string GetDbgName() const override { return "swap_pipe"; }
};

}  // namespace npr_graphics

#endif  // PIPELINE_H_
