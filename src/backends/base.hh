#include <memory>
#include <stdexcept>

#include "../draw.hh"

class WindowBackend {
  static WindowBackend *_instance;

public:
  WindowBackend() {}
  // De-initializing the backend may fail in some cases so a noexcept(false)
  // destructor is necessary.
  virtual ~WindowBackend() noexcept(false) {}

  static WindowBackend &instance() {
    if (!_instance)
      throw std::runtime_error("Backend not yet initialized");
    return *_instance;
  }

  virtual void pre_draw() = 0;
  virtual std::unique_ptr<Draw> create_draw() = 0;
  virtual void post_draw() = 0;
};
