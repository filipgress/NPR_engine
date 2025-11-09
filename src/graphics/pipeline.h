#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "vulkan_context.h"
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

// Data to keep alive during pipeline creation
struct PipelineData {
  std::vector<vk::SpecializationMapEntry> specialization_entries;
  std::vector<uint8_t> specialization_data;
  vk::SpecializationInfo specialization_info;

  std::array<vk::VertexInputBindingDescription, 2> binding_descs;
  std::vector<vk::VertexInputAttributeDescription> attr_descs;

  std::vector<vk::PipelineColorBlendAttachmentState> color_attachments;
  std::vector<vk::DynamicState> dynamic_states;
};

class Pipeline : public npr_core::NonCopyable {
 public:
  Pipeline(const VulkanContext& context, const PipelineCache& cache,
           const BasePass& render_pass, VertexShader& vert_shader,
           FragmentShader& frag_shader)
      : c_{context},
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
      size_t subpass = 0, bool use_vbo = false,
      const std::string& vert_entry = "main",
      const std::string& frag_entry = "main",
      const std::vector<SpecConstInfo>& specialization_consts = {});
  void DestroyPipeline();

  void CreateLayout(
      const std::vector<vk::DescriptorSetLayout>& set_layouts = {},
      const std::vector<PushConstInfo>& push_consts = {});

  virtual const std::string GetDbgName() const = 0;

  std::array<vk::PipelineShaderStageCreateInfo, 2> GetShaderStages(
      PipelineData& data, const std::string& vert_entry,
      const std::string& frag_entry,
      const std::vector<SpecConstInfo>& specialization_consts = {});

  vk::PipelineVertexInputStateCreateInfo GetVertexInputState(
      PipelineData& data, bool use_vbo) const;

  virtual vk::PipelineInputAssemblyStateCreateInfo GetInputAssemblyState()
      const;
  virtual vk::PipelineViewportStateCreateInfo GetViewportState() const;
  virtual vk::PipelineRasterizationStateCreateInfo GetRasterizationState()
      const;
  virtual vk::PipelineMultisampleStateCreateInfo GetMultisampleState() const;
  virtual vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const;
  virtual vk::PipelineColorBlendStateCreateInfo GetColorBlendState(
      PipelineData& data) const;
  virtual vk::PipelineDynamicStateCreateInfo GetDynamicState(
      PipelineData& data) const;

  template <typename T>
  static PushConstInfo MakePushConst(vk::ShaderStageFlags stages) {
    return {sizeof(T), stages};
  }
  template <typename T>
  static SpecConstInfo MakeSpecConst(uint32_t constant_id, const T& value) {
    return {constant_id, sizeof(T), &value};
  }

 protected:
  const VulkanContext& c_;
  const PipelineCache& cache_;

  const BasePass& render_pass_;

  VertexShader& vert_shader_;
  FragmentShader& frag_shader_;

  vk::Pipeline pipeline_{nullptr};
  vk::PipelineLayout layout_{nullptr};

  // track currently used shader version for hot-reloading
  uint64_t vert_shader_ver_;
  uint64_t frag_shader_ver_;
};

class GBuffPipe : public Pipeline {
 public:
  GBuffPipe(const VulkanContext& context, const PipelineCache& cache,
            const GBuffPass& render_pass, VertexShader& vert_shader,
            FragmentShader& frag_shader, const FrameProps& props,
            const DescriptorPool& desc_pool)
      : Pipeline(context, cache, render_pass, vert_shader, frag_shader),
        props_{props} {
    CreateLayout(
        {desc_pool.GetCameraSets().GetLayout(), TextureArraySet(c_).GetLayout(),
         desc_pool.GetMaterialSets().GetLayout()},
        {});
    Recreate();
  }

  void Recreate() override {
    CreatePipeline(0, true, "main", "main",
                   {MakeSpecConst(0, static_cast<uint32_t>(props_.samples)),
                    MakeSpecConst(1, static_cast<uint32_t>(MAX_TEXTURES))});
  }

 private:
  const std::string GetDbgName() const override { return "gbuff_pipe"; }

  vk::PipelineMultisampleStateCreateInfo GetMultisampleState() const override;
  vk::PipelineDepthStencilStateCreateInfo GetDepthStencilState() const override;
  vk::PipelineColorBlendStateCreateInfo GetColorBlendState(
      PipelineData& data) const override;

 private:
  const FrameProps& props_;
};

class LoadPipe : public Pipeline {
 public:
  LoadPipe(const VulkanContext& context, const PipelineCache& cache,
           const LoadPass& render_pass, VertexShader& vert_shader,
           FragmentShader& frag_shader)
      : Pipeline(context, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout();
    Recreate();
  }

  void Recreate() override { CreatePipeline(); }

 private:
  const std::string GetDbgName() const override { return "load_pipe"; }
};

class SwapPipe : public Pipeline {
 public:
  SwapPipe(const VulkanContext& context, const PipelineCache& cache,
           const SwapPass& render_pass, VertexShader& vert_shader,
           FragmentShader& frag_shader, const DescriptorPool& desc_pool)
      : Pipeline(context, cache, render_pass, vert_shader, frag_shader) {
    CreateLayout(
        // {desc_pool.GetPresentSets().GetLayout()},
        {desc_pool.GetGBuffSets().GetLayout()},
        {MakePushConst<LoadPushConst>(vk::ShaderStageFlagBits::eFragment)});
    Recreate();
  }

  void Recreate() override { CreatePipeline(); }

 private:
  const std::string GetDbgName() const override { return "swap_pipe"; }
};

}  // namespace npr_graphics

#endif  // PIPELINE_H_
