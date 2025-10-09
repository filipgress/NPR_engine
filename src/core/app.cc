#include "core/app.h"
#include "window/window_event.h"

namespace npr_core {

void App::Run() {
  while (running_) {
    window_.PollEvents();
  }
}

void App::OnEvent(npr_window::Event& e) {
  using namespace npr_window;
  EventDispatcher dispatcher(e);

  // window events
  dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
    (void)e;
    running_ = false;
    return true;
  });

  dispatcher.Dispatch<WindowResizeEvent>([](WindowResizeEvent& e) {
    (void)e;
    return true;
  });
}

}  // namespace npr_core
