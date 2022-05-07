#pragma once

#include <memory>
#include <stdexcept>

#include "../draw.hh"

class WindowBackend {
  static WindowBackend *_instance;

public:
  WindowBackend() {
    if (_instance)
      throw std::runtime_error("WindowBackend constructed twice");
    _instance = this;
  }
  // De-initializing the backend may fail in some cases so a noexcept(false)
  // destructor is necessary.
  virtual ~WindowBackend() noexcept(false) {}

  WindowBackend(const WindowBackend &) = delete;
  WindowBackend &operator=(const WindowBackend &) = delete;
  WindowBackend(WindowBackend &&) = delete;
  WindowBackend &operator=(WindowBackend &&) = delete;

  static WindowBackend &instance() {
    if (!_instance)
      throw std::runtime_error("Backend not yet initialized");
    return *_instance;
  }

  virtual void pre_draw() = 0;
  virtual std::unique_ptr<Draw> create_draw() = 0;
  virtual void post_draw() = 0;
};
