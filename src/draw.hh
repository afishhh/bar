#pragma once

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include <X11/extensions/Xrender.h>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Draw {
public:
  using color_type = unsigned long;
  using color_t = color_type;
  using pos_type = size_t;
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

class XDraw : public Draw {
private:
  Display *_dpy;
  Window _win;
  Drawable _drawable;
  GC _gc;
  std::vector<XftFont *> _fonts;
  XftDraw *_xft_draw;

  size_t _offset_x;
  size_t _offset_y;
  size_t _max_x;
  size_t _max_y;
  size_t _bar_height;

  Visual *_visual = DefaultVisual(_dpy, DefaultScreen(_dpy));
  Colormap _cmap = XCreateColormap(_dpy, _win, _visual, AllocNone);

  std::unordered_map<color_type, XftColor> _color_cache;
  std::unordered_map<long, XftFont *> _font_cache;
  XftFont *lookup_font(long codepoint);
  XftColor *lookup_color(color_type color);

public:
  friend int main();

  XDraw(Display *dpy, Window win, Drawable drawable, GC gc,
        std::vector<XftFont *> fonts, size_t offset_x, size_t offset_y,
        size_t max_x, size_t max_y, size_t bar_height)
      : _dpy(dpy), _win(win), _drawable(drawable), _gc(gc), _fonts(fonts),
        _offset_x(offset_x), _offset_y(offset_y), _max_x(max_x), _max_y(max_y),
        _bar_height(bar_height) {
    _xft_draw = XftDrawCreate(_dpy, _drawable, DefaultVisual(_dpy, 0),
                              DefaultColormap(_dpy, 0));
  }
  ~XDraw() {
    XftDrawDestroy(_xft_draw);
    for (auto &color : _color_cache) {
      XftColorFree(_dpy, _visual, _cmap, &color.second);
    }
    XFreeColormap(_dpy, _cmap);
  }

  pos_t screen_width() const override {
    return DisplayWidth(_dpy, DefaultScreen(_dpy));
  }
  pos_t screen_height() const override {
    return DisplayHeight(_dpy, DefaultScreen(_dpy));
  }

  pos_t height() const override { return _max_y - _offset_y; }
  pos_t width() const override { return _max_x - _offset_x; }

  pos_t vcenter() const override { return height() / 2; }
  pos_t hcenter() const override { return width() / 2; }

  void hrect(pos_t x, pos_t y, pos_t w, pos_t h, color_type color) override {
    XSetForeground(_dpy, _gc, color);
    XDrawRectangle(_dpy, _drawable, _gc, x + _offset_x, y + _offset_y, w, h);
  }
  void frect(pos_t x, pos_t y, pos_t width, pos_t height,
             color_type color) override {
    XSetForeground(_dpy, _gc, color);
    XFillRectangle(_dpy, _drawable, _gc, x + _offset_x, y + _offset_y, width,
                   height);
  }
  void rect(pos_t x, pos_t y, pos_t width, pos_t height,
            color_type color = 0xFFFFFF) {
    frect(x, y, width, height, color);
  }
  void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2, color_type color) override {
    XSetForeground(_dpy, _gc, color);
    XDrawLine(_dpy, _drawable, _gc, x1 + _offset_x, y1 + _offset_y,
              x2 + _offset_x, y2 + _offset_y);
  }
  pos_t text(pos_t x, pos_t y, std::string_view, color_type color) override;
  pos_t textw(std::string_view text) override;
};
