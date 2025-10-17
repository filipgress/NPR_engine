#ifndef APP_H_
#define APP_H_

#include "frame_timer.h"
#include "input_handler.h"

#include "window/window.h"
#include "graphics/renderer.h"

namespace npr_core {
class App : public NonCopyable {
 public:
  void Run();

 private:
  void OnEvent(npr_window::Event& e);

 private:
  npr_window::Window window_{[this](npr_window::Event& e) { OnEvent(e); }};
  npr_graphics::Renderer renderer_{window_};

  FrameTimer timer_{60, [this](npr_window::Event& e) { return OnEvent(e); }};
  InputHandler inputs_;
  bool running_{true};
};

}  // namespace npr_core

#endif  // APP_H_
