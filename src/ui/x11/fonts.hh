#pragma once

#include <X11/Xft/Xft.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <vector>

#include <fmt/core.h>

#include "connection.hh"

namespace ui::x11 {

class fonts final {
  friend class draw;

  x11::connection *_conn;
  std::vector<XftFont *> _fonts;

public:
  fonts(x11::connection *conn) : _conn(conn){};
  fonts(fonts const &) = delete;
  fonts(fonts &&) = default;
  fonts &operator=(fonts const &) = delete;
  fonts &operator=(fonts &&) = delete;
  ~fonts() {
    for (auto font : _fonts)
      XftFontClose(_conn->display(), font);
  }

  void add(std::string_view name) {
    if (auto *font = XftFontOpenName(
            _conn->display(), DefaultScreen(_conn->display()), name.data());
        font)
      _fonts.push_back(font);
    else
      throw std::runtime_error(fmt::format("Failed to load font {}", name));
  }
};

} // namespace ui::x11
