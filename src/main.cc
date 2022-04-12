#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdbe.h>

#include <X11/extensions/dbe.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <ratio>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "block.hh"
#include "config.hh"
#include "guard.hh"
#include "loop.hh"

const size_t EV_REDRAW_EVENT = EventLoop::create_event();

int main(int argc, char *argv[]) {
  // Create a connection to the X server
  Display *display = XOpenDisplay(nullptr);
  guard display_guard([display] { XCloseDisplay(display); });
  if (display == NULL) {
    std::cerr << "Cannot open display" << '\n';
    return 1;
  }

  {
    int mayor, minor;
    if (XdbeQueryExtension(display, &mayor, &minor)) {
      std::cout << "Supported Xdbe extension version " << mayor << '.' << minor
                << '\n';
    } else {
      std::cerr << "Xdbe extension not supported" << '\n';
      return 1;
    }
  }

  // Get the size of the screen in pixels
  int screen = DefaultScreen(display);
  int display_width = DisplayWidth(display, screen);
  int display_height = DisplayHeight(display, screen);

  // Create a window with width of display and height of config::height px
  Window window =
      XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0,
                          display_width, config::height, 0, 0, 0);

  // Set the window title
  XStoreName(display, window, "Fishhh's custom status bar");

  if (config::override_redirect) {
    XSetWindowAttributes attr;
    attr.background_pixel = BlackPixel(display, screen);
    attr.override_redirect = true;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attr);
  }

  XClassHint class_hint;
  class_hint.res_name = (char *)"fbar";
  class_hint.res_class = (char *)"fbar";
  XSetClassHint(display, window, &class_hint);

  // Load a font with Xft
  std::vector<XftFont *> fonts;
  for (auto font_name : config::fonts) {
    XftFont *font = XftFontOpenName(display, screen, font_name);
    if (font == NULL) {
      std::cerr << "Cannot load font" << '\n';
      return 1;
    }
    fonts.push_back(font);
  }
  guard fonts_guard([display, fonts]() {
    for (auto font : fonts) {
      XftFontClose(display, font);
    }
  });

  // Initialize Xdbe buffer
  auto backbuffer = XdbeAllocateBackBufferName(display, window, XdbeBackground);
  guard backbuffer_guard([display, backbuffer]() {
    XdbeDeallocateBackBufferName(display, backbuffer);
  });
  XdbeSwapInfo swap_info;
  swap_info.swap_window = window;
  swap_info.swap_action = XdbeBackground;
  XMapWindow(display, window);

  // Create a graphics context
  GC gc = XCreateGC(display, backbuffer, 0, nullptr);
  guard gc_guard([display, gc](void) { XFreeGC(display, gc); });

  size_t i = 0;

  // clang-format off
  Draw draw (
    display,
    window,
    backbuffer,
    gc,
    fonts,

    0 /* offset x */,
    5 /* offset y */,
    display_width /* max x */,
    config::height - 5 /* max y */,

    config::height /* bar height */,
    0
  );
  // clang-format on

  for (auto &block : config::blocks)
    block->late_init();

  std::unordered_map<Block *,
                     std::chrono::time_point<std::chrono::steady_clock,
                                             std::chrono::duration<double>>>
      update_times;
  for (auto &block : config::blocks) {
    update_times[block.get()] =
        std::chrono::steady_clock::now() + block->update_interval();
    block->update();
  }

  std::unordered_map<Block *,
                     std::chrono::time_point<std::chrono::steady_clock,
                                             std::chrono::duration<double>>>
      last_draw_points;
  for (auto &block : config::blocks) {
    last_draw_points[block.get()] = std::chrono::steady_clock::now();
  }

  auto loop = EventLoop();

  for (auto &block : config::blocks) {
    if (block->update_interval() != std::chrono::duration<double>::max())
      loop.add_timer(true,
                     std::chrono::duration_cast<decltype(loop)::duration>(
                         block->update_interval()),
                     [&](auto delta) {
                       block->update();
                       loop.fire_event(EV_REDRAW_EVENT);
                     });
    if (auto i = block->animate_interval())
      loop.add_timer(true, std::move(*i), [&](auto delta) {
        block->animate(delta);
        loop.fire_event(EV_REDRAW_EVENT);
      });
  }

  loop.on_event(EV_REDRAW_EVENT, [&]() {
    while (XEventsQueued(display, QueuedAfterFlush) > 0) {
      XEvent e;
      XNextEvent(display, &e);
      // Ignore all events :sunglasses:
      XFreeEventData(display, &e.xcookie);
    }

    draw._offset_x = 5;

    size_t x = 0;
    for (auto &block : config::blocks) {
      auto now = std::chrono::steady_clock::now();
      auto delta = now - last_draw_points[block.get()];
      draw._offset_x += block->draw(draw, delta);
      last_draw_points[block.get()] = now;
      if (&block !=
          &config::blocks[sizeof config::blocks / sizeof(config::blocks[0]) -
                          1]) {
        draw._offset_x += 8;
        XSetForeground(display, gc, 0xD3D3D3);
        XFillRectangle(display, backbuffer, gc, draw._offset_x, 3, 2,
                       config::height - 6);
        draw._offset_x += 10;
      }
    }

    if (!XdbeSwapBuffers(display, &swap_info, 1))
      throw std::runtime_error("XdbeSwapBuffers failed");
    XFlush(display);
  });

  loop.run();

  return 0;
}
