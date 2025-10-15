#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "vulkan_context.h"
#include "render_pass.h"
#include "shader.h"

namespace npr_graphics {

// Header for robust pipeline cache serialization
// Based on "Robust pipeline cache serialization" article
struct PipelineCachePrefixHeader {
  uint32_t magic_number;        // Magic number for identification
  uint32_t data_size;            // Size of cache data after header
  uint64_t hash;                 // Hash of cache data for integrity
  uint32_t vendor_id;            // GPU vendor ID
  uint32_t device_id;            // GPU device ID
  uint32_t driver_version;       // Driver version
  uint32_t driver_abi;           // Driver ABI
  uint8_t uuid[VK_UUID_SIZE];    // Pipeline cache UUID
};

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

  vk::VertexInputBindingDescription binding_desc;
  std::vector<vk::VertexInputAttributeDescription> attribute_descs;

  std::vector<vk::PipelineColorBlendAttachmentState> color_attachments;
  std::vector<vk::DynamicState> dynamic_states;
};

class Pipeline : public npr_core::NonCopyable {
 public:
  Pipeline(const VulkanContext& context, const BasePass& render_pass,
           VertexShader& vert_shader, FragmentShader& frag_shader)
      : c_{context},
        render_pass_{render_pass},
        vert_shader_{vert_shader},
        frag_shader_{frag_shader} {
    // Initialize device properties for cache validation
    device_properties_ = c_.GetPhysicalDevice().getProperties();
    
    // Use a shared cache file for all pipelines (best practice for performance)
    // Sharing cache across pipeline types improves compilation time
    cache_file_path_ = "pipeline_cache.bin";
  }
  virtual ~Pipeline() { Destroy(); }

  vk::Pipeline GetPipeline() const { return pipeline_; }
  vk::PipelineLayout GetLayout() const { return layout_; }

  virtual void Recreate() = 0;
  bool IsUpToDate() const {
    return vert_shader_.GetVersion() == vert_shader_ver_ &&
           frag_shader_.GetVersion() == frag_shader_ver_;
  }

 protected:
  void Destroy();
  void CreatePipeline(
      size_t subpass = 0, bool use_vbo = false,
      const std::string& vert_entry = "main",
      const std::string& frag_entry = "main",
      const std::vector<SpecConstInfo>& specialization_consts = {});

  void CreateLayout(
      const std::vector<vk::DescriptorSetLayout>& set_layouts = {},
      const std::vector<PushConstInfo>& push_consts = {});

  virtual const char* GetDbgName() const = 0;

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

 private:
  // Pipeline cache management
  bool LoadPipelineCache();
  void SavePipelineCache();
  uint64_t ComputeHash(const void* data, size_t size);

 protected:
  const VulkanContext& c_;
  const BasePass& render_pass_;

  VertexShader& vert_shader_;
  FragmentShader& frag_shader_;

  vk::Pipeline pipeline_{nullptr};
  vk::PipelineLayout layout_{nullptr};
  vk::PipelineCache pipeline_cache_{nullptr};

  // Device properties for cache validation
  vk::PhysicalDeviceProperties device_properties_;
  std::string cache_file_path_;

  // track currently used shader version for hot-reloading
  uint64_t vert_shader_ver_;
  uint64_t frag_shader_ver_;
};

class SwapPipe : public Pipeline {
 public:
  SwapPipe(const VulkanContext& context, const SwapPass& render_pass,
           VertexShader& vert_shader, FragmentShader& frag_shader)
      : Pipeline(context, render_pass, vert_shader, frag_shader) {
    Recreate();
  }

  void Recreate() {
    Destroy();

    CreateLayout();
    CreatePipeline(0, false, "main", "main", {});
  }

 private:
  const char* GetDbgName() const { return "SwapPipe"; }
};

}  // namespace npr_graphics

#endif  // PIPELINE_H_
