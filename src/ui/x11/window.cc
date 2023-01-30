#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>
#include <X11/extensions/dbe.h>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../../config.hh"
#include "draw.hh"
#include "window.hh"

namespace ui::x11 {

ui::draw &window::drawer() {
  if (!_draw.has_value()) {
    _back_buffer =
        XdbeAllocateBackBufferName(_conn->display(), _xwinid, XdbeBackground);
    _draw.emplace(_conn->display(), _xwinid, *_back_buffer, this->size().y);
  }

  return _draw.value();
}

uvec2 window::size() const {
  Window root;
  int x, y;
  unsigned int w, h, borderw, depth;

  if (XGetGeometry(_conn->display(), _xwinid, &root, &x, &y, &w, &h, &borderw,
                   &depth) == 0)
    throw std::runtime_error("window.size: XGetGeometry failed");

  return {w, h};
}

void window::flip() {
  XdbeSwapInfo swap_info;
  swap_info.swap_window = _xwinid;
  swap_info.swap_action = XdbeBackground;

  if (XdbeSwapBuffers(_conn->display(), &swap_info, 1) == 0)
    throw std::runtime_error("XdbeSwapBuffers failed");

  _conn->flush();
}

} // namespace ui::x11
