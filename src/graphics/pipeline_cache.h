#ifndef PIPELINE_CACHE_H_
#define PIPELINE_CACHE_H_

#include "vulkan_context.h"

namespace npr_graphics {

// https://zeux.io/2019/07/17/serializing-pipeline-cache/
constexpr uint32_t kPipelineCacheMagic = 0x504C434E;
struct PipelineCachePrefixHeader {
  uint32_t magic_num;

  uint32_t vendor_id;
  uint32_t device_id;
  uint32_t driver_version;
  uint32_t api_version;

  uint32_t data_size;
  uint64_t data_hash;

  // pipeline cache UUID
  std::array<uint8_t, VK_UUID_SIZE> uuid;
};

class PipelineCache : public npr_core::NonCopyable {
 public:
  PipelineCache(const VulkanContext& context);
  ~PipelineCache();

  vk::PipelineCache GetCache() const { return pipeline_cache_; }

 private:
  void Save();
  std::vector<uint8_t> Load();
  uint64_t ComputeHash(const void* data, size_t size);

  PipelineCachePrefixHeader CreateHeader(
      const std::vector<uint8_t>& cache_data);
  bool IsValidHeader(const PipelineCachePrefixHeader& header,
                     const std::vector<uint8_t>& cache_data);

 private:
  const VulkanContext& c_;
  vk::PipelineCache pipeline_cache_{nullptr};

  const std::string kFilePath_ = "pipeline.cache";
};
}  // namespace npr_graphics

#endif  // PIPELINE_CACHE_H_
