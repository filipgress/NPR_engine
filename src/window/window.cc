#include "window/window.h"
#include "window/window_event.h"
#include "window/mouse_event.h"
#include "window/key_event.h"

namespace npr_window {

Window::Window(EventCallbackFn callback_fn) : callback_fn_{callback_fn} {
  glfwSetErrorCallback(ErrorCallback);

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window_ = glfwCreateWindow(1200, 800, PROJECT_NAME, nullptr, nullptr);

  SetEventCallbacks();
}

void Window::SetEventCallbacks() {
  glfwSetWindowUserPointer(window_, &callback_fn_);
  glfwSetWindowSizeCallback(
      window_, [](GLFWwindow* window, int width, int height) {
        auto callback_fn =
            *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

        WindowResizeEvent e{{width, height}};
        callback_fn(e);
      });

  glfwSetWindowCloseCallback(window_, [](GLFWwindow* window) {
    auto callback_fn =
        *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

    WindowCloseEvent e;
    callback_fn(e);
  });

  glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scan_code,
                                 int action, int mods) {
    (void)scan_code, (void)mods;
    auto callback_fn =
        *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

    switch (action) {
      case GLFW_PRESS: {
        KeyPressEvent e(key, false);
        callback_fn(e);
        break;
      }
      case GLFW_RELEASE: {
        KeyReleaseEvent e(key);
        callback_fn(e);
        break;
      }
      case GLFW_REPEAT: {
        KeyPressEvent e(key, true);
        callback_fn(e);
        break;
      }
    }
  });

  glfwSetMouseButtonCallback(
      window_, [](GLFWwindow* window, int button, int action, int mods) {
        (void)mods;
        auto callback_fn =
            *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

        switch (action) {
          case GLFW_PRESS: {
            MousePressEvent e(button);
            callback_fn(e);
            break;
          }
          case GLFW_RELEASE: {
            MouseReleaseEvent e(button);
            callback_fn(e);
            break;
          }
        }
      });

  glfwSetScrollCallback(
      window_, [](GLFWwindow* window, double offset_x, double offset_y) {
        auto callback_fn =
            *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

        MouseScrollEvent e({offset_x, offset_y});
        callback_fn(e);
      });

  glfwSetCursorPosCallback(
      window_, [](GLFWwindow* window, double pos_x, double pos_y) {
        auto callback_fn =
            *static_cast<EventCallbackFn*>(glfwGetWindowUserPointer(window));

        MouseMoveEvent e({pos_x, pos_y});
        callback_fn(e);
      });
}

vk::SurfaceKHR Window::CreateSurface(vk::Instance instance) const {
  VkSurfaceKHR c_surface;
  if (glfwCreateWindowSurface(instance, window_, nullptr, &c_surface) !=
      VK_SUCCESS)
    throw std::runtime_error("Unable to create window surface");

  return c_surface;
}

void Window::ErrorCallback(int err_code, const char* desc) {
  std::ostringstream msg;

  msg << "GLFW error [" << err_code << "]: " << desc;
  throw std::runtime_error(msg.str());
}

glm::ivec2 Window::GetSize() const {
  glm::ivec2 size;
  glfwGetFramebufferSize(window_, &size.x, &size.y);

  return size;
}

}  // namespace npr_window
