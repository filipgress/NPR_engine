#ifndef UTILS_H_
#define UTILS_H_

#define BIT(x) x ? (1ull << (x - 1)) : 0

#define ERR(...) npr_core::Logger::GetInstance().log(true, __VA_ARGS__)
#ifdef NDEBUG
constexpr bool kEnableDebug{false};
constexpr bool kEnableShaderReload{false};

  #define INFO(...)
#else
constexpr bool kEnableDebug{true};
constexpr bool kEnableShaderReload{true};

  // #define TIMER_ENABLE
  #define INFO(...) npr_core::Logger::GetInstance().log(false, __VA_ARGS__)
#endif

#ifdef TIMER_ENABLE
  #define TIMER_START(name) \
    auto name##_start = std::chrono::high_resolution_clock::now();

  #define TIMER_END(name)                                               \
    auto name##_end = std::chrono::high_resolution_clock::now();        \
    std::cout << #name << ": "                                          \
              << std::chrono::duration_cast<std::chrono::microseconds>( \
                     name##_end - name##_start)                         \
                     .count()                                           \
              << " us\n";
#else
  #define TIMER_START(...)
  #define TIMER_END(...)
#endif

namespace npr_core {

std::vector<uint8_t> ReadFile(const std::string& filepath);
inline std::string GetFilename(const std::string& filepath) {
  return filepath.substr(filepath.find_last_of("/\\") + 1);
}

inline uint32_t Align(uint32_t size, uint32_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

inline void HashCombine(std::size_t& seed, std::size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

class NonCopyable {
 protected:
  NonCopyable() = default;
  ~NonCopyable() = default;

 public:
  // Delete copy constructor/assignment operator
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;

  // Enable move semantics
  NonCopyable(NonCopyable&&) noexcept = default;
  NonCopyable& operator=(NonCopyable&&) noexcept = default;
};

template <typename T>
class Singleton : public NonCopyable {
 protected:
  Singleton() = default;
  ~Singleton() = default;

 public:
  static T& GetInstance() {
    static T instance;
    return instance;
  }
};

class Logger : public Singleton<Logger> {
  friend class Singleton<Logger>;

 public:
  template <typename... Args>
  void log(bool is_err, Args... args) {
    std::ostringstream msg;

    if (is_err) msg << "[error] ";
    ((msg << args), ...);
    msg << std::endl;

    if (is_err)
      std::cerr << msg.str();
    else if (kEnableDebug)
      std::cout << msg.str();

    log_to_file(msg.str());
  }

 private:
  Logger();
  ~Logger();

  void log_to_file(const std::string& msg);

 private:
  std::mutex log_mutex_;
  std::ofstream log_file_;
  const std::string kLogPath = "log.txt";
};

}  // namespace npr_core

#endif  // UTILS_H_
