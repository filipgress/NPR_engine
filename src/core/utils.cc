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

}  // namespace npr_core
