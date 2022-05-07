#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <stdexcept>

#include "../config.hh"
#include "../draw.hh"
#include "../format.hh"
#include "../log.hh"
#include "../xdraw.hh"
#include "x11.hh"

bool XWindowBackend::is_available() {
  return std::getenv("DISPLAY") != nullptr;
}

XWindowBackend::XWindowBackend() {
  // Create a connection to the X server
  _display = XOpenDisplay(nullptr);
  if (_display == NULL)
    throw std::runtime_error("Cannot open display");

  {
    int mayor, minor;
    if (XdbeQueryExtension(_display, &mayor, &minor))
      std::print(info, "Supported Xdbe extension version {}.{}\n",
                 mayor, minor);
    else
      throw std::runtime_error("Xdbe extension not supported");
  }

  int screen = DefaultScreen(_display);
  int display_width = DisplayWidth(_display, screen);

  _window = XCreateSimpleWindow(_display, RootWindow(_display, screen), 0, 0,
                                display_width, config::height, 0, 0, 0);

  XStoreName(_display, _window, "Fishhh's custom status bar");

  if (config::x11::override_redirect) {
    XSetWindowAttributes attr;
    attr.background_pixel = BlackPixel(_display, screen);
    attr.override_redirect = true;
    XChangeWindowAttributes(_display, _window, CWOverrideRedirect, &attr);
  }

  XClassHint class_hint;
  class_hint.res_name = config::x11::window_class.data();
  class_hint.res_class = config::x11::window_class.data();
  XSetClassHint(_display, _window, &class_hint);

  for (auto font_name : config::fonts) {
    XftFont *font = XftFontOpenName(_display, screen, font_name);
    if (font == NULL)
      throw std::runtime_error(
          std::format("Cannot load font {}", std::quoted(font_name)));
    _fonts.push_back(font);
  }

  _back_buffer = XdbeAllocateBackBufferName(_display, _window, XdbeBackground);
  _gc = XCreateGC(_display, _back_buffer, 0, nullptr);
  XMapWindow(_display, _window);
}

XWindowBackend::~XWindowBackend() noexcept(false) {
  XFreeGC(_display, _gc);
  XUnmapWindow(_display, _window);
  XdbeDeallocateBackBufferName(_display, _back_buffer);

  for (XftFont *font : _fonts)
    XftFontClose(_display, font);

  XCloseDisplay(_display);
}

std::unique_ptr<Draw> XWindowBackend::create_draw() {
  return std::make_unique<XDraw>(_display, _window, _back_buffer, _gc, _fonts,
                                 config::height);
}

void XWindowBackend::pre_draw() {
  while (XEventsQueued(_display, QueuedAfterFlush) > 0) {
    XEvent e;
    XNextEvent(_display, &e);
    // Ignore all events :sunglasses:
    XFreeEventData(_display, &e.xcookie);
  }
}

void XWindowBackend::post_draw() {
  XdbeSwapInfo swap_info;
  swap_info.swap_window = _window;
  swap_info.swap_action = XdbeBackground;

  if (XdbeSwapBuffers(_display, &swap_info, 1) == 0)
    throw std::runtime_error("XdbeSwapBuffers failed");

  XFlush(_display);
}
