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
#include <ranges>
#include <ratio>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "block.hh"
#include "bufdraw.hh"
#include "config.hh"
#include "guard.hh"
#include "log.hh"
#include "loop.hh"
#include "util.hh"
#include "xdraw.hh"

int main() {
  // Create a connection to the X server
  Display *display = XOpenDisplay(nullptr);
  guard display_guard([display] { XCloseDisplay(display); });
  if (display == NULL) {
    error << "Cannot open display" << '\n';
    return 1;
  }

  {
    int mayor, minor;
    if (XdbeQueryExtension(display, &mayor, &minor)) {
      info << "Supported Xdbe extension version " << mayor << '.' << minor
           << '\n';
    } else {
      error << "Xdbe extension not supported" << '\n';
      return 1;
    }
  }

  // Get the size of the screen in pixels
  int screen = DefaultScreen(display);
  int display_width = DisplayWidth(display, screen);

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
      info << "Cannot load font " << std::quoted(font_name) << '\n';
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

  // clang-format off
  XDraw real_draw (
    display,
    window,
    backbuffer,
    gc,
    fonts,

    0 /* offset x */,
    5 /* offset y */,
    display_width /* max x */,
    config::height - 5 /* max y */,

    config::height /* bar height */
  );
  BufDraw draw(real_draw);
  // clang-format on

  struct BlockInfo {
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_update;
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_draw;
  };
  std::unordered_map<Block *, BlockInfo> block_info;
  auto &loop = EventLoop::instance();

  auto setup_block = [&](Block &block) {
    block.late_init();

    auto &info = block_info[&block];
    info.last_update = std::chrono::steady_clock::now();
    block.update();

    info.last_draw = std::chrono::steady_clock::now();

    if (block.update_interval() != std::chrono::duration<double>::max())
      loop.add_timer(true,
                     std::chrono::duration_cast<EventLoop::duration>(
                         block.update_interval()),
                     [&](auto) {
                       block.update();
                       loop.fire_event(EventLoop::Event::REDRAW);
                     });
    if (auto i = block.animate_interval())
      loop.add_timer(true, std::move(*i), [&](auto delta) {
        block.animate(delta);
        loop.fire_event(EventLoop::Event::REDRAW);
      });
  };

  for (auto &block : config::left_blocks)
    setup_block(*block);
  for (auto &block : config::right_blocks)
    setup_block(*block);

  loop.on_event(EventLoop::Event::REDRAW, [&]() {
    while (XEventsQueued(display, QueuedAfterFlush) > 0) {
      XEvent e;
      XNextEvent(display, &e);
      // Ignore all events :sunglasses:
      XFreeEventData(display, &e.xcookie);
    }

    std::size_t x = 5;

    for (auto &block : config::left_blocks) {
      auto now = std::chrono::steady_clock::now();
      auto info = block_info[block.get()];
      real_draw._offset_x = x;
      x += block->draw(real_draw, now - info.last_draw);
      info.last_draw = now;

      if (&block != &config::left_blocks[sizeof config::left_blocks /
                                             sizeof(config::left_blocks[0]) -
                                         1]) {
        x += 8;
        XSetForeground(display, gc, 0xD3D3D3);
        XFillRectangle(display, backbuffer, gc, x, 3, 2, config::height - 6);
        x += 10;
      }
    }

    XWindowAttributes attr;
    XGetWindowAttributes(display, window, &attr);

    x = attr.width - 5;

    for (auto &block : config::right_blocks | std::views::reverse) {
      auto now = std::chrono::steady_clock::now();
      auto info = block_info[block.get()];
      auto width = block->draw(draw, now - info.last_draw);
      info.last_draw = now;

      x -= width;
      XSetForeground(display, gc, BlackPixel(display, screen));
      XFillRectangle(display, backbuffer, gc, x - 8, 0, width + 18,
                     config::height);
      draw.draw_offset(x, 0);
      draw.clear();

      if (&block != &config::right_blocks[0]) {
        x -= 6;
        XSetForeground(display, gc, 0xD3D3D3);
        XFillRectangle(display, backbuffer, gc, x, 3, 2,
                       config::height - 6);
        x -= 14;
      }
    }

    if (!XdbeSwapBuffers(display, &swap_info, 1))
      throw std::runtime_error("XdbeSwapBuffers failed");
    XFlush(display);
  });

  loop.run();

  return 0;
}
