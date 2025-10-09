#ifndef WINDOW_H_
#define WINDOW_H_

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

namespace npr_window {
class Event;
class Window {
  using EventCallbackFn = std::function<void(Event&)>;

 public:
  Window(EventCallbackFn callback_fn);
  ~Window() { glfwTerminate(); }

  vk::SurfaceKHR CreateSurface(vk::Instance instance) const;
  void PollEvents() const { glfwPollEvents(); }

  static void ErrorCallback(int err_code, const char* desc);

  glm::ivec2 GetSize() const;
  bool IsMinimized() const {
    glm::ivec2 size = GetSize();
    return size.x == 0 || size.y == 0;
  }

 private:
  void SetEventCallbacks();

 private:
  GLFWwindow* window_;
  EventCallbackFn callback_fn_;
};
}  // namespace npr_window

#endif  // WINDOW_H_
