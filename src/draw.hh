#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

class Draw {
public:
  using color_type = std::uint32_t;
  using color_t = color_type;
  using pos_type = std::size_t;
  using pos_t = pos_type;

public:
  virtual ~Draw() = default;

  virtual pos_t screen_width() const = 0;
  virtual pos_t screen_height() const = 0;

  virtual pos_t height() const = 0;
  virtual pos_t width() const = 0;

  virtual pos_t vcenter() const = 0;
  virtual pos_t hcenter() const = 0;

  virtual void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2,
                    color_type = 0xFFFFFF) = 0;

  virtual void hrect(pos_t x, pos_t y, pos_t w, pos_t h,
                     color_type = 0xFFFFFF) = 0;
  virtual void frect(pos_t x, pos_t y, pos_t w, pos_t h,
                     color_type = 0xFFFFFF) = 0;

  virtual pos_t text(pos_t x, pos_t y, std::string_view text,
                     color_type = 0xFFFFFF) = 0;
  virtual pos_t textw(std::string_view text) = 0;
};
