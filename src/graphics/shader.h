#ifndef SHADER_H_
#define SHADER_H_

#include "context.h"

namespace npr_graphics {
class Shader : public npr_core::NonCopyable {
 public:
  Shader(const Context& ctx, vk::ShaderStageFlagBits stage,
         const std::string& spirv_path, const std::string& src_path = "");

  virtual ~Shader();

  void CreateShaderModule();
  void DestroyShaderModule();

  uint64_t GetVersion() const { return version_; }
  vk::PipelineShaderStageCreateInfo GetShaderStageInfo(
      const std::string& entry) const;

  void Reload();
  void ReloadAsync();
  bool IsDirty();

 protected:
  bool IsUpToDate() const;
  bool Compile();

  void WaitForAsync() {
    if (handle_.valid()) dirty_ = handle_.get();
  }

 protected:
  const Context& ctx_;

  vk::ShaderStageFlagBits stage_;
  vk::ShaderModule module_{nullptr};

  std::string spirv_path_;
  std::string src_path_;

  std::string name_;

  std::future<bool> handle_;
  bool dirty_{false};
  uint64_t version_{0};
};

class VertexShader : public Shader {
 public:
  VertexShader(const Context& ctx, const std::string& spirv_path,
               const std::string& src_path = "")
      : Shader{ctx, vk::ShaderStageFlagBits::eVertex, spirv_path, src_path} {}
};

class FragmentShader : public Shader {
 public:
  FragmentShader(const Context& ctx, const std::string& spirv_path,
                 const std::string& src_path = "")
      : Shader{ctx, vk::ShaderStageFlagBits::eFragment, spirv_path, src_path} {}
};

}  // namespace npr_graphics

#endif  // SHADER_H_
