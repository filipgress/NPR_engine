#ifndef UTILS_H_
#define UTILS_H_

#define BIT(x) x ? (1ull << (x - 1)) : 0

#define ERR(...) npr_core::Logger::GetInstance().log(true, __VA_ARGS__)
#ifdef NDEBUG
constexpr bool kEnabledDebug{false};
  #define INFO(...)
#else
constexpr bool kEnabledDebug{true};
  #define TIMER_ENABLED
  #define INFO(...) npr_core::Logger::GetInstance().log(false, __VA_ARGS__)
#endif

#ifdef TIMER_ENABLED
  #define TIMER_START(name) \
    auto name##_start = std::chrono::high_resolution_clock::now();

  #define TIMER_END(name)                                               \
    auto name##_end = std::chrono::high_resolution_clock::now();        \
    std::cout << #name << ": "                                          \
              << std::chrono::duration_cast<std::chrono::milliseconds>( \
                     name##_end - name##_start)                         \
                     .count()                                           \
              << " ms\n";
#else
  #define TIMER_START(...)
  #define TIMER_END(...)
#endif

namespace npr_core {

inline std::string GetFilename(const std::string& filepath) {
  return filepath.substr(filepath.find_last_of("/\\") + 1);
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

 protected:
  Logger();
  ~Logger();

 public:
  template <typename... Args>
  void log(bool is_err, Args... args) {
    std::ostringstream msg;

    if (is_err) msg << "[error] ";
    ((msg << args), ...);
    msg << std::endl;

    if (is_err)
      std::cerr << msg.str();
    else if (kEnabledDebug)
      std::cout << msg.str();

    log_to_file(msg.str());
  }

 private:
  void log_to_file(const std::string& msg);

 private:
  std::mutex log_mutex_;
  std::ofstream log_file_;
  const std::string kLogPath = "log.txt";
};

}  // namespace npr_core

#endif  // UTILS_H_
