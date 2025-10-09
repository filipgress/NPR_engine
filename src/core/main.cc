#include "app.h"

int main(void) {
  try {
    npr_core::App app;
    app.Run();

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    ERR(e.what());
    return EXIT_FAILURE;
  }
}
