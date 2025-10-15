#include "shader.h"

namespace npr_graphics {
Shader::Shader(const VulkanContext& context, vk::ShaderStageFlagBits stage,
               const std::string& spirv_path, const std::string& src_path)
    : c_{context}, stage_{stage}, spirv_path_{spirv_path}, src_path_(src_path) {
  name_ = std::filesystem::path(spirv_path_).stem().string();

  if constexpr (kEnableShaderReload) Compile();
  CreateShaderModule();
}

Shader::~Shader() {
  if constexpr (kEnableShaderReload) WaitForAsync();
  DestroyShaderModule();
}

void Shader::DestroyShaderModule() {
  if (module_) {
    c_.GetDevice().destroyShaderModule(module_);
    module_ = nullptr;
  }
}

void Shader::CreateShaderModule() {
  DestroyShaderModule();
  auto spirv = npr_core::ReadFile(spirv_path_);

  vk::ShaderModuleCreateInfo createInfo{};
  createInfo.codeSize = spirv.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());

  module_ = c_.GetDevice().createShaderModule(createInfo);
  c_.SetDbgName((uint64_t)(VkShaderModule)module_,
                vk::ObjectType::eShaderModule, name_);

  version_++;
  dirty_ = false;
}

vk::PipelineShaderStageCreateInfo Shader::GetShaderStageInfo(
    const std::string& entry) const {
  assert(module_);

  vk::PipelineShaderStageCreateInfo info;
  info.stage = stage_;
  info.module = module_;
  info.pName = entry.c_str();

  return info;
}

void Shader::ReloadAsync() {
  if constexpr (!kEnableShaderReload) return;

  if (handle_.valid()) return;
  handle_ = std::async(std::launch::async, [this]() { return Compile(); });
}

void Shader::Reload() {
  if constexpr (!kEnableShaderReload) return;

  WaitForAsync();
  dirty_ = Compile();
}

bool Shader::IsUpToDate() const {
  try {
    if (src_path_.empty() || !std::filesystem::exists(src_path_)) return true;
    return std::filesystem::exists(spirv_path_) &&
           std::filesystem::last_write_time(spirv_path_) >=
               std::filesystem::last_write_time(src_path_);
  } catch (const std::filesystem::filesystem_error& e) {
    // File was deleted/moved between checks
    // Assume up to date if we can't check
    return true;
  }
}

bool Shader::Compile() {
  if (IsUpToDate()) return false;

  std::filesystem::path dst_filepath(spirv_path_);
  std::filesystem::create_directories(dst_filepath.parent_path());

  std::ostringstream cmd;
  cmd << "glslc \"" << src_path_ << "\" -o \"" << spirv_path_ << "\"";

  if (std::system(cmd.str().c_str()) != 0) {
    ERR("failed to compile shader '", name_, "'");
    return false;
  }

  INFO("compiled shader '", npr_core::GetFilename(src_path_), "'");
  return true;
}

bool Shader::IsDirty() {
  if constexpr (!kEnableShaderReload) return false;

  if (!handle_.valid()) return dirty_;
  if (handle_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return false;

  dirty_ = handle_.get();
  return dirty_;
}

}  // namespace npr_graphics
