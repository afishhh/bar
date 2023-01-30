#pragma once

#include <memory>
#include <stdexcept>
#include <string_view>

#include "util.hh"
#include "window.hh"

namespace ui {

class connection {
public:
  connection() {}
  // De-initializing the backend may fail in some cases so a noexcept(false)
  // destructor is necessary.
  virtual ~connection() noexcept(false) {}

  connection(const connection &) = delete;
  connection &operator=(const connection &) = delete;
  // TODO: This could potentially support moving if the above TODO is solved
  connection(connection &&conn) = delete;
  connection &operator=(connection &&) = delete;

  virtual std::unique_ptr<ui::window> create_window(std::string_view name,
                                                    uvec2 pos, uvec2 size) = 0;

  // TODO: Figure out a better interface for this
  virtual uvec2 available_size() = 0;
};

} // namespace ui
