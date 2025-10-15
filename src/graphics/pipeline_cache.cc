#include "pipeline_cache.h"

namespace npr_graphics {

PipelineCache::PipelineCache(const VulkanContext& context) : c_{context} {
  auto cache_data = Load();

  vk::PipelineCacheCreateInfo cache_info{};
  cache_info.initialDataSize = cache_data.size();
  cache_info.pInitialData = cache_data.data();

  pipeline_cache_ = c_.GetDevice().createPipelineCache(cache_info);
}

PipelineCache::~PipelineCache() {
  if (pipeline_cache_) {
    Save();
    c_.GetDevice().destroyPipelineCache(pipeline_cache_);
  }
}

// Simple FNV-1a hash for data integrity checking
uint64_t PipelineCache::ComputeHash(const void* data, size_t size) {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  uint64_t hash = 0xcbf29ce484222325ULL;

  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 0x100000001b3ULL;
  }

  return hash;
}

bool PipelineCache::IsValidHeader(const PipelineCachePrefixHeader& header,
                                  const std::vector<uint8_t>& cache_data) {
  auto device_props = c_.GetProperties();
  if (header.magic_num != kPipelineCacheMagic ||
      header.vendor_id != device_props.vendorID ||
      header.device_id != device_props.deviceID ||
      header.driver_version != device_props.driverVersion ||
      VK_VERSION_MAJOR(header.api_version) !=
          VK_VERSION_MAJOR(device_props.apiVersion)) {
    INFO("pipeline cache device/version mismatch");
    return false;
  }

  if (!std::equal(header.uuid.begin(), header.uuid.end(),
                  std::begin(device_props.pipelineCacheUUID))) {
    INFO("pipeline cache UUID mismatch");
    return false;
  }

  if (cache_data.size() != header.data_size ||
      ComputeHash(cache_data.data(), cache_data.size()) != header.data_hash) {
    ERR("pipeline cache data corrupted");
    return false;
  }

  return true;
}

std::vector<uint8_t> PipelineCache::Load() {
  try {
    if (!std::filesystem::exists(kFilePath_)) {
      INFO("pipeline cache not found");
      return {};
    }
    auto file_content = npr_core::ReadFile(kFilePath_);

    if (file_content.size() < sizeof(PipelineCachePrefixHeader)) {
      ERR("pipeline cache incomplete header (possibly corrupted)");
      return {};
    }

    PipelineCachePrefixHeader header;
    std::memcpy(&header, file_content.data(), sizeof(header));

    size_t cache_data_size = file_content.size() - sizeof(header);
    std::vector<uint8_t> cache_data(cache_data_size);
    std::memcpy(cache_data.data(), file_content.data() + sizeof(header),
                cache_data_size);

    if (!IsValidHeader(header, cache_data)) return {};

    INFO("pipeline cache loaded");
    return cache_data;

  } catch (const std::runtime_error& e) {
    ERR("failed to load pipeline cache: ", e.what());
    return {};
  }
}

PipelineCachePrefixHeader PipelineCache::CreateHeader(
    const std::vector<uint8_t>& cache_data) {
  auto device_props = c_.GetProperties();

  PipelineCachePrefixHeader header{};
  header.magic_num = kPipelineCacheMagic;
  header.data_size = static_cast<uint32_t>(cache_data.size());
  header.data_hash = ComputeHash(cache_data.data(), cache_data.size());
  header.vendor_id = device_props.vendorID;
  header.device_id = device_props.deviceID;
  header.driver_version = device_props.driverVersion;
  header.api_version = device_props.apiVersion;
  std::copy(std::begin(device_props.pipelineCacheUUID),
            std::end(device_props.pipelineCacheUUID), header.uuid.begin());

  return header;
}

void PipelineCache::Save() {
  try {
    auto cache_data = c_.GetDevice().getPipelineCacheData(pipeline_cache_);
    if (cache_data.empty()) {
      INFO("no pipeline data to cache");
      return;
    }

    std::ofstream cache_file(kFilePath_, std::ios::binary);
    if (!cache_file.is_open()) {
      ERR("failed to open pipeline cache file for writing");
      return;
    }

    auto header = CreateHeader(cache_data);

    cache_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    cache_file.write(reinterpret_cast<const char*>(cache_data.data()),
                     cache_data.size());

    if (!cache_file.good()) {
      ERR("failed to write pipeline cache");
      cache_file.close();
      std::filesystem::remove(kFilePath_);
      return;
    }

    cache_file.close();

    INFO("pipeline cache saved");

  } catch (const std::exception& e) {
    ERR("failed to save pipeline cache: ", e.what());
  }
}

}  // namespace npr_graphics
