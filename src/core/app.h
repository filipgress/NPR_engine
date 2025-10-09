#ifndef APP_H_
#define APP_H_

#include "window/window.h"

namespace npr_core {
class App {
 public:
  void Run();

 private:
  void OnEvent(npr_window::Event& e);

 private:
  npr_window::Window window_{
      [this](npr_window::Event& e) { this->OnEvent(e); }};
  bool running_{true};
};

}  // namespace npr_core

#endif  // APP_H_
