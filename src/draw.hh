#pragma once

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include <X11/extensions/Xrender.h>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

class Draw {
public:
  typedef size_t color_type;

private:
  Display *_dpy;
  Window _win;
  GC _gc;
  std::vector<XftFont *> _fonts;
  XftDraw *_xft_draw;

  size_t _offset_x;
  size_t _offset_y;
  size_t _max_x;
  size_t _max_y;
  size_t _bar_height;

  size_t _fps;

  Visual *_visual = DefaultVisual(_dpy, DefaultScreen(_dpy));
  Colormap _cmap = XCreateColormap(_dpy, _win, _visual, AllocNone);

  std::unordered_map<color_type, XftColor> _color_cache;

public:
  friend int main(int argc, char *argv[]);

  Draw(Display *dpy, Window win, GC gc, std::vector<XftFont *> fonts,
       size_t offset_x, size_t offset_y, size_t max_x, size_t max_y,
       size_t bar_height, size_t fps)
      : _dpy(dpy), _win(win), _gc(gc), _fonts(fonts), _offset_x(offset_x),
        _offset_y(offset_y), _max_x(max_x), _max_y(max_y),
        _bar_height(bar_height), _fps(fps) {
    _xft_draw = XftDrawCreate(_dpy, _win, DefaultVisual(_dpy, 0),
                              DefaultColormap(_dpy, 0));
  }
  ~Draw() {
    XftDrawDestroy(_xft_draw);
    for (auto &color : _color_cache) {
      XftColorFree(_dpy, _visual, _cmap, &color.second);
    }
    XFreeColormap(_dpy, _cmap);
  }

  size_t screen_width() const {
    return DisplayWidth(_dpy, DefaultScreen(_dpy));
  }
  size_t screen_height() const {
    return DisplayHeight(_dpy, DefaultScreen(_dpy));
  }
  size_t bar_height() const { return _bar_height; }
  size_t height() const { return _max_y - _offset_y; }

  size_t vcenter() const { return height() / 2; }
  size_t hcenter() const { return (_max_x - _offset_x) / 2; }

  void rect(size_t x, size_t y, int width, int height,
            color_type color = 0xFFFFFF) const {
    XSetForeground(_dpy, _gc, color);
    XFillRectangle(_dpy, _win, _gc, x + _offset_x, y + _offset_y, width,
                   height);
  }
  void line(size_t x1, size_t y1, size_t x2, size_t y2,
            color_type color = 0xFFFFFF) const {
    XSetForeground(_dpy, _gc, color);
    XDrawLine(_dpy, _win, _gc, x1 + _offset_x, y1 + _offset_y, x2 + _offset_x,
              y2 + _offset_y);
  }
  size_t text(size_t x, size_t y, std::string_view,
              color_type color = 0xFFFFFF);
  size_t text_width(std::string_view) const;

  size_t fps() const { return _fps; }
};
