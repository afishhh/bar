#pragma once

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include <algorithm>
#include <bits/iterator_concepts.h>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../format.hh"
#include "../draw.hh"
#include "../util.hh"

namespace ui::x11 {

class draw : public ui::draw {
private:
  friend class window;

  Display *_dpy;
  Drawable _win;
  Drawable _drw;
  GC _gc;
  std::vector<XftFont *> _fonts;
  XftDraw *_xft_draw;
  uvec2 _size;

  Visual *_visual = DefaultVisual(_dpy, DefaultScreen(_dpy));
  Colormap _cmap = XCreateColormap(_dpy, _win, _visual, AllocNone);

  std::unordered_map<color, XftColor> _xft_color_cache;
  std::unordered_map<char32_t, XftFont *> _font_cache;
  XftFont *lookup_font(char32_t codepoint);
  XftColor *lookup_xft_color(color color);

  template <typename Iter, typename End>
    requires requires(Iter it, End end) {
      { it != end } -> std::convertible_to<bool>;
      { *it } -> std::convertible_to<char32_t>;
      { ++it };
    }
  uvec2 _iterator_textsz(Iter begin, End end);

public:
  draw(Display *dpy, Window window, Drawable drawable, uvec2 size)
      : _dpy(dpy), _win(window), _drw(drawable), _size(size) {
    _gc = XCreateGC(dpy, drawable, 0, nullptr);
    _xft_draw = XftDrawCreate(dpy, drawable, DefaultVisual(dpy, 0),
                              DefaultColormap(dpy, 0));
  }

  ~draw() {
    XftDrawDestroy(_xft_draw);
    for (auto &color : _xft_color_cache)
      XftColorFree(_dpy, _visual, _cmap, &color.second);
    for (auto font : _fonts)
      XftFontClose(_dpy, font);
    XFreeColormap(_dpy, _cmap);
  }

  void set_fonts(std::vector<XftFont *> &&fonts) { _fonts = std::move(fonts); }
  void load_font(std::string_view name) override {
    if (auto *font = XftFontOpenName(_dpy, DefaultScreen(_dpy), name.data());
        font)
      _fonts.push_back(font);
    else
      throw std::runtime_error(std::format("Failed to load font {}", name));
  }

  pos_t width() const override { return _size.x; }
  pos_t height() const override { return _size.y; }

  pos_t vcenter() const override { return height() / 2; }
  pos_t hcenter() const override { return width() / 2; }

  void fcircle(pos_t x, pos_t y, pos_t d, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XFillArc(_dpy, _drw, _gc, x, y, d, d, 0, 23040);
  }

  void hrect(pos_t x, pos_t y, pos_t w, pos_t h, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XDrawRectangle(_dpy, _drw, _gc, x, y, w, h);
  }
  void frect(pos_t x, pos_t y, pos_t width, pos_t height,
             color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XFillRectangle(_dpy, _drw, _gc, x, y, width, height);
  }
  void rect(pos_t x, pos_t y, pos_t width, pos_t height, color color) {
    frect(x, y, width, height, color.as_rgb());
  }
  void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XDrawLine(_dpy, _drw, _gc, x1, y1, x2, y2);
  }

  pos_t text(pos_t x, pos_t y, std::string_view, color color) override;

  uvec2 textsz(std::string_view text) final override;
  uvec2 textsz(std::u32string_view text) final override;
};

} // namespace ui::x11
