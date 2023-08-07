#pragma once

#include <memory>
#include <stdexcept>

#include "draw.hh"
#include "util.hh"

namespace ui {

class window {
public:
  window() {}
  window(window const &) = delete;
  window(window &&) = default;
  window &operator=(window const &) = delete;
  window &operator=(window &&) = delete;
  virtual ~window() noexcept(false) {}

  virtual draw &drawer() = 0;
  virtual void flip() = 0;
  virtual uvec2 size() const = 0;
  virtual void move(uvec2) = 0;
  virtual void resize(uvec2) = 0;
  virtual void moveresize(uvec2 pos, uvec2 size) = 0;

  virtual void show() = 0;
  virtual void hide() = 0;
};

} // namespace ui
