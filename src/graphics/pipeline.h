#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "context.h"
#include "descriptor_pool.h"
#include "pipeline_cache.h"
#include "render_pass.h"
#include "shader.h"

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

// data to keep alive during pipeline creation
struct PipelineData {
  std::vector<vk::SpecializationMapEntry> specialization_entries;
  std::vector<uint8_t> specialization_data;
  vk::SpecializationInfo specialization_info;

  std::vector<vk::VertexInputBindingDescription> binding_descs;
  std::vector<vk::VertexInputAttributeDescription> attr_descs;

  std::vector<vk::PipelineColorBlendAttachmentState> color_attachments{};
  std::vector<vk::DynamicState> dynamic_states{
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
  };

  vk::CullModeFlagBits cull_mode{vk::CullModeFlagBits::eNone};
  vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};
  bool use_vbo{false};
};

class Pipeline : public npr_core::NonCopyable {
 public:
  Pipeline(const Context& ctx, const PipelineCache& cache,
           const BasePass& render_pass, VertexShader& vert_shader,
           FragmentShader& frag_shader)
      : ctx_{ctx},
        cache_{cache},
        render_pass_{render_pass},
        vert_shader_{vert_shader},
        frag_shader_{frag_shader} {}
  virtual ~Pipeline();

  vk::Pipeline GetPipeline() const { return pipeline_; }
  vk::PipelineLayout GetLayout() const { return layout_; }

  virtual void Recreate() = 0;
  bool IsUpToDate() const {
    return vert_shader_.GetVersion() == vert_shader_ver_ &&
           frag_shader_.GetVersion() == frag_shader_ver_;
  }

 protected:
  void CreatePipeline(
      size_t subpass = 0, const std::string& vert_entry = "main",
      const std::string& frag_entry = "main",
      const std::vector<SpecConstInfo>& specialization_consts = {});
  void DestroyPipeline();

  void CreateLayout(
      const std::vector<vk::DescriptorSetLayout>& set_layouts = {},
      const std::vector<PushConstInfo>& push_consts = {});

  virtual const std::string GetDbgName() const = 0;

  std::array<vk::PipelineShaderStageCreateInfo, 2> GetShaderStages(
      const std::string& vert_entry, const std::string& frag_entry,
      const std::vector<SpecConstInfo>& specialization_consts = {});
  virtual vk::PipelineVertexInputStateCreateInfo GetVertexInputState();

  virtual vk::PipelineInputAssemblyStateCreateInfo GetInputAssemblyState()
      const;
  virtual vk::PipelineViewportStateCreateInfo GetViewportState() const;
  virtual vk::PipelineRasterizationStateCreateInfo GetRasterizationState()
      const;
  virtual vk::PipelineMultisampleStateCreateInfo GetMultisampleState() const;
  virtual vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const;
  virtual vk::PipelineColorBlendStateCreateInfo GetColorBlendState() const;
  virtual vk::PipelineDynamicStateCreateInfo GetDynamicState() const;

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

  VertexShader& vert_shader_;
  FragmentShader& frag_shader_;

  vk::Pipeline pipeline_{nullptr};
  vk::PipelineLayout layout_{nullptr};

  PipelineData data_;

  // track currently used shader version for hot-reloading
  uint64_t vert_shader_ver_;
  uint64_t frag_shader_ver_;
};

class GBuffPipe : public Pipeline {
 public:
  GBuffPipe(const Context& ctx, const PipelineCache& cache,
            const GBuffPass& render_pass, VertexShader& vert_shader,
            FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
            const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout({desc_pool.GetCameraSets().GetLayout(),
                  TextureArraySet(ctx_).GetLayout(),
                  desc_pool.GetMaterialSets().GetLayout()},
                 {});

    data_.use_vbo = true;
    data_.samples = samples;

    data_.color_attachments.resize(5);
    for (auto& att : data_.color_attachments) {
      att.blendEnable = VK_FALSE;
      att.colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    }
    data_.color_attachments[4].colorWriteMask = vk::ColorComponentFlagBits::eR;

    data_.dynamic_states = {vk::DynamicState::eViewport,
                            vk::DynamicState::eScissor,
                            vk::DynamicState::eCullMode};

    Recreate();
  }

  void Recreate() override {
    CreatePipeline(0, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(data_.samples)),
                    MakeSpecConst(1, static_cast<uint32_t>(MAX_TEXTURES))});
  }

 private:
  const std::string GetDbgName() const override { return "gbuff_pipe"; }
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;
};

class AOGenPipe : public Pipeline {
 public:
  AOGenPipe(const Context& ctx, const PipelineCache& cache,
            const AOGenPass& render_pass, VertexShader& vert_shader,
            FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
            const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout({desc_pool.GetCameraSets().GetLayout(),
                  desc_pool.GetGBuffSets().GetLayout(),
                  desc_pool.GetAOSet().GetLayout()},
                 {});

    data_.samples = samples;

    // ao_ms output
    data_.color_attachments.resize(1);
    data_.color_attachments[0].blendEnable = VK_FALSE;
    data_.color_attachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR;

    Recreate();
  }

  void Recreate() override {
    CreatePipeline(0, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(AO_NOISE_DIM)),
                    MakeSpecConst(1, static_cast<uint32_t>(AO_KERNEL_SIZE))});
  }

 private:
  const std::string GetDbgName() const override { return "ao_gen_pipe"; }
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;
};

class AOBlurPipe : public Pipeline {
 public:
  AOBlurPipe(const Context& ctx, const PipelineCache& cache,
             const BlurPass& render_pass, VertexShader& vert_shader,
             FragmentShader& frag_shader, const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout(
        {desc_pool.GetAOResSets().GetLayout()},
        {MakePushConst<BlurPushConst>(vk::ShaderStageFlagBits::eFragment)});

    // ao_res or ao_temp output
    data_.color_attachments.resize(1);
    data_.color_attachments[0].blendEnable = VK_FALSE;
    data_.color_attachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR;

    Recreate();
  }

  void Recreate() override {
    CreatePipeline(
        0, "main", "main",
        {MakeSpecConst(0, static_cast<uint32_t>(MAX_GAUSSIAN_RADIUS))});
  }

 private:
  const std::string GetDbgName() const override { return "ao_blur_pipe"; }
};

class LightPipe : public Pipeline {
 public:
  LightPipe(const Context& ctx, const PipelineCache& cache,
            const BasePass& render_pass, VertexShader& vert_shader,
            FragmentShader& frag_shader)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {}

 private:
  vk::PipelineVertexInputStateCreateInfo GetVertexInputState() override;
};

class DirLightPipe : public LightPipe {
 public:
  DirLightPipe(const Context& ctx, const PipelineCache& cache,
               const DirLightPass& render_pass, VertexShader& vert_shader,
               FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
               const DescriptorPool& desc_pool)
      : LightPipe(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout({desc_pool.GetAOResSets().GetLayout(),
                  desc_pool.GetGBuffSets().GetLayout(),
                  desc_pool.GetDirLightSets().GetLayout()},
                 {});

    data_.samples = samples;

    // color_ms
    data_.color_attachments.resize(1);
    data_.color_attachments[0].blendEnable = VK_FALSE;
    data_.color_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    Recreate();
  }

  void Recreate() override {
    CreatePipeline(0, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(data_.samples)),
                    MakeSpecConst(1, static_cast<uint32_t>(MAX_DIR_LIGHTS))});
  }

 private:
  const std::string GetDbgName() const override { return "dir_light_pipe"; }
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;
};

class LightStencilPipe : public LightPipe {
 public:
  LightStencilPipe(const Context& ctx, const PipelineCache& cache,
                   const LightVolPass& render_pass, VertexShader& vert_shader,
                   FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                   const DescriptorPool& desc_pool)
      : LightPipe(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout(
        {desc_pool.GetCameraSets().GetLayout()},
        {MakePushConst<LightPushConst>(vk::ShaderStageFlagBits::eVertex)});

    data_.use_vbo = true;
    data_.samples = samples;
    data_.cull_mode = vk::CullModeFlagBits::eBack;

    Recreate();
  }

  void Recreate() override { CreatePipeline(0, "main", "main"); }

 private:
  const std::string GetDbgName() const override { return "light_stencil_pipe"; }
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;
};

class LightVolPipe : public LightPipe {
 public:
  LightVolPipe(const Context& ctx, const PipelineCache& cache,
               const LightVolPass& render_pass, VertexShader& vert_shader,
               FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
               const DescriptorPool& desc_pool, bool is_point_light)
      : LightPipe(ctx, cache, render_pass, vert_shader, frag_shader),
        is_point_light_{is_point_light} {
    CreateLayout(
        {desc_pool.GetCameraSets().GetLayout(),
         desc_pool.GetGBuffSets().GetLayout(),
         is_point_light ? desc_pool.GetPointLightSets().GetLayout()
                        : desc_pool.GetSpotLightSets().GetLayout()},
        {MakePushConst<LightPushConst>(vk::ShaderStageFlagBits::eVertex)});

    data_.use_vbo = true;
    data_.samples = samples;
    data_.cull_mode = vk::CullModeFlagBits::eFront;

    // color_ms with blending
    data_.color_attachments.resize(1);
    data_.color_attachments[0].blendEnable = VK_TRUE;
    data_.color_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    // Additive blending for volumetric lights
    data_.color_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].colorBlendOp = vk::BlendOp::eAdd;

    data_.color_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

    Recreate();
  }

  void Recreate() override {
    CreatePipeline(1, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(data_.samples))});
  }

 private:
  const std::string GetDbgName() const override {
    return is_point_light_ ? "point_light_pipe" : "spot_light_pipe";
  }
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;

 private:
  bool is_point_light_;
};

class ABuffFillPipe : public Pipeline {
 public:
  ABuffFillPipe(const Context& ctx, const PipelineCache& cache,
                const ABuffPass& render_pass, VertexShader& vert_shader,
                FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout(
        {
            desc_pool.GetCameraSets().GetLayout(),
            TextureArraySet(ctx_).GetLayout(),
            desc_pool.GetABufferSets().GetLayout(),
            desc_pool.GetMaterialSets().GetLayout(),
        },
        {MakePushConst<ABuffFillPushConst>(
            vk::ShaderStageFlagBits::eFragment)});

    data_.use_vbo = true;
    data_.samples = samples;
    Recreate();
  }

  void Recreate() override {
    CreatePipeline(0, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(data_.samples)),
                    MakeSpecConst(1, static_cast<uint32_t>(MAX_TEXTURES))});
  }

 private:
  const std::string GetDbgName() const override { return "abuff_fill_pipe"; }
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;
};

class ABuffResolvePipe : public Pipeline {
 public:
  ABuffResolvePipe(const Context& ctx, const PipelineCache& cache,
                   const ABuffPass& render_pass, VertexShader& vert_shader,
                   FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
                   const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader),
        samples_{samples} {
    CreateLayout({desc_pool.GetABufferSets().GetLayout()},
                 {MakePushConst<uint32_t>(vk::ShaderStageFlagBits::eFragment)});

    // present_color
    data_.color_attachments.resize(1);
    data_.color_attachments[0].blendEnable = VK_TRUE;
    data_.color_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    data_.color_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstColorBlendFactor =
        vk::BlendFactor::eOneMinusSrcAlpha;
    data_.color_attachments[0].colorBlendOp = vk::BlendOp::eAdd;

    data_.color_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstAlphaBlendFactor =
        vk::BlendFactor::eOneMinusSrcAlpha;
    data_.color_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

    Recreate();
  }

  void Recreate() override {
    CreatePipeline(1, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(samples_))});
  }

 private:
  const std::string GetDbgName() const override { return "abuff_resolve_pipe"; }

 private:
  vk::SampleCountFlagBits samples_;
};

class WBoitAccPipe : public Pipeline {
 public:
  WBoitAccPipe(const Context& ctx, const PipelineCache& cache,
               const WBoitPass& render_pass, VertexShader& vert_shader,
               FragmentShader& frag_shader, vk::SampleCountFlagBits samples,
               const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout(
        {
            desc_pool.GetCameraSets().GetLayout(),
            TextureArraySet(ctx_).GetLayout(),
            desc_pool.GetMaterialSets().GetLayout(),
        },
        {MakePushConst<ABuffFillPushConst>(
            vk::ShaderStageFlagBits::eFragment)});

    data_.use_vbo = true;
    data_.samples = samples;

    data_.color_attachments.resize(2);

    // acc_color_res
    data_.color_attachments[0].blendEnable = VK_TRUE;
    data_.color_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    data_.color_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].colorBlendOp = vk::BlendOp::eAdd;
    data_.color_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

    // acc_weight_res
    data_.color_attachments[1].blendEnable = VK_TRUE;
    data_.color_attachments[1].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    data_.color_attachments[1].srcColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[1].dstColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[1].colorBlendOp = vk::BlendOp::eAdd;
    data_.color_attachments[1].srcAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[1].dstAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[1].alphaBlendOp = vk::BlendOp::eAdd;

    Recreate();
  }

  void Recreate() override {
    CreatePipeline(0, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(data_.samples)),
                    MakeSpecConst(1, static_cast<uint32_t>(MAX_TEXTURES))});
  }

 private:
  const std::string GetDbgName() const override { return "wboit_acc_pipe"; }
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;
};

class WBoitComposePipe : public Pipeline {
 public:
  WBoitComposePipe(const Context& ctx, const PipelineCache& cache,
                   const WBoitPass& render_pass, VertexShader& vert_shader,
                   FragmentShader& frag_shader, const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout({desc_pool.GetWBoitInputSets().GetLayout()}, {});

    // present_color
    data_.color_attachments.resize(1);
    data_.color_attachments[0].blendEnable = VK_TRUE;
    data_.color_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    data_.color_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstColorBlendFactor =
        vk::BlendFactor::eOneMinusSrcAlpha;
    data_.color_attachments[0].colorBlendOp = vk::BlendOp::eAdd;

    data_.color_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
    data_.color_attachments[0].dstAlphaBlendFactor =
        vk::BlendFactor::eOneMinusSrcAlpha;
    data_.color_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

    Recreate();
  }

  void Recreate() override { CreatePipeline(1, "main", "main"); }

 private:
  const std::string GetDbgName() const override { return "wboit_compose_pipe"; }
};

class SwapPipe : public Pipeline {
 public:
  SwapPipe(const Context& ctx, const PipelineCache& cache,
           const SwapPass& render_pass, VertexShader& vert_shader,
           FragmentShader& frag_shader, const DescriptorPool& desc_pool)
      : Pipeline(ctx, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout(
        {desc_pool.GetColorSets().GetLayout()},
        // {desc_pool.GetAOResSets().GetLayout()},
        {MakePushConst<LoadPushConst>(vk::ShaderStageFlagBits::eFragment)});

    data_.color_attachments.resize(1);
    data_.color_attachments[0].blendEnable = VK_FALSE;
    data_.color_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    Recreate();
  }

  void Recreate() override { CreatePipeline(); }

 private:
  const std::string GetDbgName() const override { return "swap_pipe"; }
};

}  // namespace npr_graphics

#endif  // PIPELINE_H_
