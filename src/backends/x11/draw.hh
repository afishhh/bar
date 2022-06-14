#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../draw.hh"

class XDraw : public Draw {
private:
  Display *_dpy;
  Window _win;
  Drawable _drawable;
  GC _gc;
  std::vector<XftFont *> _fonts;
  XftDraw *_xft_draw;
  std::size_t _height;

  Visual *_visual = DefaultVisual(_dpy, DefaultScreen(_dpy));
  Colormap _cmap = XCreateColormap(_dpy, _win, _visual, AllocNone);

  std::unordered_map<color, XftColor> _color_cache;
  std::unordered_map<char32_t, XftFont *> _font_cache;
  XftFont *lookup_font(char32_t codepoint);
  XftColor *lookup_color(color color);

public:
  friend int main();

  XDraw(Display *dpy, Window win, Drawable drawable, GC gc,
        std::vector<XftFont *> fonts, std::size_t height)
      : _dpy(dpy), _win(win), _drawable(drawable), _gc(gc), _fonts(fonts),
        _height(height) {
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
  pos_t height() const override { return _height; }

  pos_t vcenter() const override { return height() / 2; }
  pos_t hcenter() const override { return width() / 2; }

  void hrect(pos_t x, pos_t y, pos_t w, pos_t h, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XDrawRectangle(_dpy, _drawable, _gc, x, y, w, h);
  }
  void frect(pos_t x, pos_t y, pos_t width, pos_t height,
             color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XFillRectangle(_dpy, _drawable, _gc, x, y, width, height);
  }
  void rect(pos_t x, pos_t y, pos_t width, pos_t height, color color) {
    frect(x, y, width, height, color.as_rgb());
  }
  void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XDrawLine(_dpy, _drawable, _gc, x1, y1, x2, y2);
  }
  pos_t text(pos_t x, pos_t y, std::string_view, color color) override;
  pos_t textw(std::string_view text) override;
};
