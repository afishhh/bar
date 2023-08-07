#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "../window.hh"
#include "connection.hh"
#include "draw.hh"
#include "util.hh"

namespace ui::x11 {

class window final : public ui::window {
  connection *_conn;
  XWinID _xwinid;
  std::optional<x11::draw> _draw;
  std::optional<XdbeBackBuffer> _back_buffer;

public:
  window(connection *conn, XWinID xwinid)
      : ui::window(), _conn(conn), _xwinid(xwinid) {}
  ~window() noexcept(false) {
    XDestroyWindow((_conn->display()), _xwinid);
    // TODO: This should be in a backbuffer_draw or similiar instead
    if (_back_buffer)
      XdbeDeallocateBackBufferName(_conn->display(), *_back_buffer);
  }

  inline XWinID window_id() const noexcept { return _xwinid; }

  ui::draw &drawer() override;
  uvec2 size() const override;
  void move(uvec2) override;
  void resize(uvec2) override;
  void moveresize(uvec2 pos, uvec2 size) override;
  void flip() override;

  void show() override {
    XSelectInput(_conn->display(), _xwinid, StructureNotifyMask);
    XMapWindow(_conn->display(), _xwinid);
    _conn->flush();
  }

  void hide() override {
    XSelectInput(_conn->display(), _xwinid, StructureNotifyMask);
    XUnmapWindow(_conn->display(), _xwinid);
    _conn->flush();
  }

  void class_hint(std::string_view name, std::string_view class_) {
    XClassHint class_hint;
    class_hint.res_name = const_cast<char *>(name.data());
    class_hint.res_class = const_cast<char *>(class_.data());
    XSetClassHint(_conn->display(), _xwinid, &class_hint);
  }

  void override_redirect(bool enabled) {
    XSetWindowAttributes attr;
    attr.override_redirect = enabled;
    XChangeWindowAttributes(_conn->display(), _xwinid, CWOverrideRedirect,
                            &attr);
  }
};

} // namespace ui::x11
