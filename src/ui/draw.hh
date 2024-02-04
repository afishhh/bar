#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

#include <fmt/core.h>

#include "../color.hh"
#include "util.hh"

namespace ui {

class draw {
public:
  using pos_type = std::uint32_t;
  using pos_t = pos_type;

public:
  draw() = default;
  draw(draw const &) = delete;
  draw(draw &&) = default;
  draw &operator=(draw const &) = delete;
  draw &operator=(draw &&) = delete;
  virtual ~draw() = default;

  // FIXME: How should this work?
  virtual pos_t height() const = 0;
  virtual pos_t width() const = 0;

  virtual pos_t vcenter() const = 0;
  virtual pos_t hcenter() const = 0;

  virtual void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2,
                    color = 0xFFFFFF) = 0;

  virtual void hrect(pos_t x, pos_t y, pos_t w, pos_t h, color = 0xFFFFFF) = 0;
  virtual void frect(pos_t x, pos_t y, pos_t w, pos_t h, color = 0xFFFFFF) = 0;

  virtual void fcircle(pos_t x, pos_t y, pos_t d, color = 0xFFFFFF) = 0;

  virtual pos_t text(pos_t x, pos_t y, std::string_view text,
                     color = 0xFFFFFF) = 0;

  virtual pos_t text(pos_t x, std::string_view text, color color = 0xFFFFFF) {
    return this->text(x, vcenter(), text, color);
  }
  virtual uvec2 textsz(std::string_view text) = 0;

  pos_t textw(std::string_view text) { return textsz(text).x; }
  pos_t texth(std::string_view text) { return textsz(text).y; }
};

} // namespace ui
