#pragma once

#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>

#include "../log.hh"
#include "draw.hh"
#include "gl.hh"
#include "text.hh"
#include "util.hh"

namespace ui {

class window {
public:
  window() {}
  window(window const &) = delete;
  window(window &&) = default;
  window &operator=(window const &) = delete;
  window &operator=(window &&) = delete;
  ~window() noexcept(false) {}

  virtual draw &drawer() = 0;
  virtual void flip() = 0;
  virtual uvec2 size() const = 0;
  virtual void move(uvec2) = 0;
  virtual void resize(uvec2) = 0;
  virtual void moveresize(uvec2 pos, uvec2 size) = 0;

  virtual void show() = 0;
  virtual void hide() = 0;
};

class gdraw;

class gwindow {
  GLFWwindow *_window;
  gdraw *_drawer = nullptr;

public:
  operator GLFWwindow *() const { return _window; }
  gwindow() : _window(nullptr) {}
  explicit gwindow(GLFWwindow *window) : _window(window) {}
  ~gwindow();

  BAR_NON_COPYABLE(gwindow);
  gwindow(gwindow &&other) : _window(other._window) { other._window = nullptr; }
  gwindow &operator=(gwindow &&other) {
    _window = other._window;
    other._window = nullptr;
    return *this;
  }

  gdraw &drawer();
};

class gdraw final : public draw {
  GLFWwindow *_window;
  int _width, _height;
  int _available_width, _available_height;
  float _xscale, _yscale;
  int _fixed_rendering_height = -1;
  TextRenderer _texter;

  gdraw(GLFWwindow *win) : _window(win) {
    glfwGetFramebufferSize(win, &_width, &_height);
    _update_projection();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    glfwSetWindowUserPointer(win, this);
    glfwSetFramebufferSizeCallback(_window, [](GLFWwindow *window, int width, int height) {
      gdraw *self = ((gdraw *)glfwGetWindowUserPointer(window));
      self->_width = width;
      self->_height = height;
      self->_update_projection();
    });
  }

  BAR_NON_COPYABLE(gdraw);
  BAR_NON_MOVEABLE(gdraw);

  friend class gwindow;

  void _update_projection() {
    int window_width, window_height;
    glfwGetWindowSize(_window, &window_width, &window_height);
    glfwGetWindowContentScale(_window, &_xscale, &_yscale);

    if (_fixed_rendering_height != -1) {
      _available_height = _fixed_rendering_height;
      _yscale = (float)_height / _available_height;
      _xscale = _yscale;
      _available_width = _width / _xscale;
    } else {
      _available_width = (float)_width / _xscale;
      _available_height = (float)_height / _yscale;
    }

    glfwMakeContextCurrent(_window);
    glViewport(0, 0, _width, _height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, _available_width, _available_height, 0, -1, 1);

    fmt::print(debug, "Window {} resized", (void *)_window);
    if(_fixed_rendering_height != -1) {
      fmt::print(debug, " (fixed height {})", _fixed_rendering_height);
    }
    fmt::print(debug, "\n");
    fmt::print(debug, "  Framebuffer size changed to {}x{}\n", _width, _height);
    fmt::print(debug, "  Window size is {}x{}\n", window_width, window_height);
    fmt::print(debug, "  Content scale is x:{} y:{}\n", _xscale, _yscale);
    fmt::print(debug, "  Scaled size is {}x{}\n", _available_width, _available_height);

    texter().set_scale(text_render_scale());
  }

public:
  void set_fixed_rendering_height(int height) {
    _fixed_rendering_height = height;
    _update_projection();
  }

  TextRenderer &texter() { return _texter; }

  pos_t height() const { return _available_height; }
  pos_t width() const { return _available_width; }

  pos_t vcenter() const { return _available_height / 2; }
  pos_t hcenter() const { return _available_width / 2; }

  void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2, color color) {
    color::rgb rgb = color;
    glColor3ub(rgb.r, rgb.g, rgb.b);

    glBegin(GL_LINE);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
  }

  void hrect(pos_t x, pos_t y, pos_t w, pos_t h, color color) {
    color::rgb rgb = color;
    glColor3ub(rgb.r, rgb.g, rgb.b);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_POLYGON);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
  }

  void frect(pos_t x, pos_t y, pos_t w, pos_t h, color color) {
    color::rgb rgb = color;
    glColor3ub(rgb.r, rgb.g, rgb.b);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_POLYGON);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
  }

  void fcircle(pos_t x, pos_t y, pos_t d, color color) {
    color::rgb rgb = color;
    glColor3ub(rgb.r, rgb.g, rgb.b);

    constexpr int segments = 100;
    static struct CircleThetas {
      float values[segments];
      CircleThetas() {
        for (int i = 0; i < segments; ++i)
          values[i] = 2.0 * std::numbers::pi * i / segments;
      }
      float operator[](int i) { return values[i]; }
    } thetas;

    x -= 1;

    float r = d / 2.0;
    float cx = x + r;
    float cy = y + r;

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_POLYGON);
    for (int i = 0; i < segments; ++i) {
      float nx = r * std::cos(thetas[i]);
      float ny = r * std::sin(thetas[i]);
      glVertex2f(cx + nx, cy + ny);
    }
    glEnd();
  }

  pos_t text(pos_t x, pos_t y, std::string_view text, color color) {
    auto [logical, ink, off, texture] = _texter.render(text);

    if (texture) {
      color::rgb rgb = color;
      ink.x /= text_render_scale(), ink.y /= text_render_scale();
      x += off.x / text_render_scale(), y += off.y / text_render_scale();

      // fmt::println("drawing texture {} for {:?} at ({}, {})", texture, text, x, y);
      glActiveTexture(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, texture);
      glColor3ub(rgb.r, rgb.g, rgb.b);

      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glBegin(GL_QUADS);
      glTexCoord2f(0.0, 0.0);
      glVertex2i(x, y);
      glTexCoord2f(1.0, 0.0);
      glVertex2i(x + ink.x, y);
      glTexCoord2f(1.0, 1.0);
      glVertex2i(x + ink.x, y + ink.y);
      glTexCoord2f(0.0, 1.0);
      glVertex2i(x, y + ink.y);
      glEnd();

      glBindTexture(GL_TEXTURE_2D, 0);
    }

    return logical.x / text_render_scale();
  }

  pos_t text(pos_t x, std::string_view text, color color) { return this->text(x, vcenter(), text, color); }
  uvec2 textsz(std::string_view text) {
    auto unscaled = _texter.size(text);
    return {(unsigned)(unscaled.x / text_render_scale()), (unsigned)(unscaled.y / text_render_scale())};
  }

  float x_render_scale() { return _xscale; }
  float y_render_scale() { return _yscale; }
  float text_render_scale() { return _yscale; }
};

} // namespace ui
