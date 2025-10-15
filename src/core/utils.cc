#include "utils.h"

namespace npr_core {

Logger::Logger() {
  std::lock_guard<std::mutex> lock(log_mutex_);

  log_file_.open(kLogPath, std::ios::app);
  if (!log_file_.is_open())
    std::cerr << "Failed to open log file: " << kLogPath << std::endl;
}

Logger::~Logger() {
  std::lock_guard<std::mutex> lock(log_mutex_);
  if (log_file_.is_open()) log_file_.close();
}

void Logger::log_to_file(const std::string& msg) {
  std::lock_guard<std::mutex> lock(log_mutex_);
  if (!log_file_.is_open()) return;

  const auto now = std::chrono::system_clock::now();
  const auto time_t = std::chrono::system_clock::to_time_t(now);

  log_file_ << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
            << " " << msg;
  log_file_.flush();
}

std::vector<uint8_t> ReadFile(const std::string& filepath) {
  std::ifstream file(filepath, std::ios::ate | std::ios::binary);
  if (!file) {
    std::ostringstream err;
    err << "unable to read the file: \'" << npr_core::GetFilename(filepath)
        << "'";
    throw std::runtime_error(err.str());
  }

  size_t file_size = file.tellg();
  std::vector<uint8_t> buff(file_size, '\0');

  file.seekg(0);
  file.read(reinterpret_cast<char*>(buff.data()), file_size);
  file.close();

  return buff;
}

}  // namespace npr_core
