#pragma once

#include <X11/Xlib.h>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <stdexcept>

class Draw {
private:
  Display *_dpy;
  Window _win;
  GC _gc;
  XFontStruct *_font;

  size_t _offset_x;
  size_t _offset_y;
  size_t _max_x;
  size_t _max_y;
  size_t _bar_height;

  size_t _fps;

  void check_coords(size_t &x, size_t &y) const {
    if (_offset_x + x >= _max_x)
      throw std::logic_error("x coordinate out of range");
    if (_offset_y + y >= _max_y)
      throw std::logic_error("y coordinate out of range");
  }

public:
  friend int main(int argc, char *argv[]);

  Draw(Display *dpy, Window win, GC gc, XFontStruct *font, size_t offset_x,
       size_t offset_y, size_t max_x, size_t max_y, size_t bar_height,
       size_t fps)
      : _dpy(dpy), _win(win), _gc(gc), _font(font), _offset_x(offset_x),
        _offset_y(offset_y), _max_x(max_x), _max_y(max_y),
        _bar_height(bar_height), _fps(fps) {}

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
            unsigned long color = 0xFFFFFF) const {
    check_coords(x, y);
    XSetForeground(_dpy, _gc, color);
    XFillRectangle(_dpy, _win, _gc, x + _offset_x, y + _offset_y, width,
                   height);
  }
  void line(size_t x1, size_t y1, size_t x2, size_t y2,
            unsigned long color = 0xFFFFFF) const {
    check_coords(x1, y1);
    check_coords(x2, y2);

    XSetForeground(_dpy, _gc, color);
    XDrawLine(_dpy, _win, _gc, x1 + _offset_x, y1 + _offset_y, x2 + _offset_x,
              y2 + _offset_y);
  }
  size_t text(size_t x, size_t y, std::string_view text,
              unsigned long color = 0xFFFFFF) const {
    check_coords(x, y);
    XSetForeground(_dpy, _gc, color);
    XDrawString(_dpy, _win, _gc, x + _offset_x,
                y + _offset_y + (_font->ascent - _font->descent) / 2,
                text.data(), text.size());
    return text_width(text);
  }
  size_t text_width(std::string_view text) const {
    return XTextWidth(_font, text.data(), text.size());
  }

  size_t fps() const { return _fps; }
};

class Block {
public:
  virtual ~Block() {}

  virtual size_t draw(Draw &) = 0;
  virtual void update(){};
  virtual std::chrono::duration<double> update_interval() {
    return std::chrono::duration<double>::max();
  };
};
