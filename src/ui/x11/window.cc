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
    _draw.emplace(_conn, _xwinid, *_back_buffer, this->size());
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

void window::move(uvec2 pos) {
  if (XMoveWindow(_conn->display(), _xwinid, pos.x, pos.y) == 0)
    throw std::runtime_error("window.move: XMoveWindow failed");
}

void window::resize(uvec2 size) {
  if (XResizeWindow(_conn->display(), _xwinid, size.x, size.y) == 0)
    throw std::runtime_error("window.resize: XResizeWindow failed");
  if (_draw) {
    _draw->_size = size;
    XftDrawChange(_draw->_xft_draw, _draw->_drw);
  }
}

void window::moveresize(uvec2 pos, uvec2 size) {
  if (XMoveResizeWindow(_conn->display(), _xwinid, pos.x, pos.y, size.x,
                        size.y) == 0)
    throw std::runtime_error("window.moveresize: XMoveResizeWindow failed");
  if (_draw) {
    _draw->_size = size;
    XftDrawChange(_draw->_xft_draw, _draw->_drw);
  }
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
