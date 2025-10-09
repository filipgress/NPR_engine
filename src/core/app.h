#ifndef APP_H_
#define APP_H_

#include "frame_timer.h"
#include "input_handler.h"

#include "window/window.h"

namespace npr_core {
class App {
 public:
  void Run();

 private:
  void OnEvent(npr_window::Event& e);

 private:
  npr_window::Window window_{[this](npr_window::Event& e) { OnEvent(e); }};

  FrameTimer timer_;
  InputHandler inputs_;
  bool running_{true};
};

}  // namespace npr_core

#endif  // APP_H_
