#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "context.h"
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
  friend class PipeManager;

 public:
  Pipeline(const Context& ctx, const PipelineCache& cache,
           const BasePass& render_pass, const std::string& dbg_name);
  virtual ~Pipeline();

  vk::Pipeline GetPipeline() const { return pipeline_; }
  vk::PipelineLayout GetLayout() const { return layout_; }

  bool IsUpToDate() const;
  void BuildPipeline();

 private:
  void CreateLayout(const std::vector<vk::DescriptorSetLayout>& sets,
                    const std::vector<PushConstInfo>& push_consts = {});

  void AddShader(const Shader& shader,
                 const std::vector<SpecConstInfo>& spec_consts = {},
                 const std::string& entry = "main") {
    state_.shaders.push_back({shader, spec_consts, entry});
  }

  void AddLightVertexAttribs();
  void AddObjectInstanceAttribs();

  void AddVertexBinding(
      uint32_t binding, uint32_t stride,
      vk::VertexInputRate input_rate = vk::VertexInputRate::eVertex) {
    state_.vertex_bindings.push_back({binding, stride, input_rate});
  }
  void AddVertexAttrib(uint32_t location, uint32_t binding, vk::Format format,
                       uint32_t offset) {
    state_.vertex_attribs.push_back({location, binding, format, offset});
  }

  template <typename T>
  static PushConstInfo MakePushConst(vk::ShaderStageFlags stages) {
    return {sizeof(T), stages};
  }
  template <typename T>
  static SpecConstInfo MakeSpecConst(uint32_t constant_id, const T& value) {
    return {constant_id, sizeof(T), &value};
  }

 private:
  const Context& ctx_;
  const PipelineCache& cache_;
  const BasePass& render_pass_;

  vk::Pipeline pipeline_{nullptr};
  vk::PipelineLayout layout_{nullptr};

  PipelineState state_;
  std::string dbg_name_;
};
}  // namespace npr_graphics

#endif  // PIPELINE_H_
