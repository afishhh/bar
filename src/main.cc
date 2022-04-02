#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

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

template <typename T> struct guard {
private:
  T callback;

public:
  guard(const guard &) = delete;
  guard &operator=(const guard &) = delete;

  guard(T callback) : callback(callback) {}
  ~guard() { callback(); }
};

int main(int argc, char *argv[]) {
  // Create a connection to the X server
  Display *display = XOpenDisplay(nullptr);
  guard display_guard([display] { XCloseDisplay(display); });
  if (display == NULL) {
    std::cerr << "Cannot open display" << '\n';
    return 1;
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
    attr.override_redirect = true;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attr);
  }

  // Create a graphics context
  GC gc = XCreateGC(display, window, 0, 0);
  guard gc_guard([display, gc](void) { XFreeGC(display, gc); });
  XMapWindow(display, window);

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

  // Set the background color
  XSetBackground(display, gc, BlackPixel(display, screen));

  // Set the foreground color
  XSetForeground(display, gc, WhitePixel(display, screen));

  size_t i = 0;

  // clang-format off
  Draw draw (
    display,
    window,
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

  // PERF: This can be replaced with a priority queue that sorts by time and
  //       then we could just sleep for the time difference between the next
  //       block and now, then update that block and redraw everything.
  std::unordered_map<Block *,
                     std::chrono::time_point<std::chrono::steady_clock,
                                             std::chrono::duration<double>>>
      update_times;
  for (auto &block : config::blocks) {
    update_times[block.get()] =
        std::chrono::steady_clock::now() + block->update_interval();
    block->update();
  }

  while (1) {
    auto start = std::chrono::steady_clock::now();

    while (XEventsQueued(display, QueuedAfterFlush) > 0) {
      XEvent e;
      XNextEvent(display, &e);
      // Ignore all events :sunglasses:
    }

    XClearWindow(display, window);
    draw._offset_x = 5;

    size_t x = 0;
    for (auto &block : config::blocks) {
      auto now = std::chrono::steady_clock::now();
      if (now > update_times[block.get()]) {
        update_times[block.get()] = now + block->update_interval();
        block->update();
      }

      draw._offset_x += block->draw(draw);
      if (&block !=
          &config::blocks[sizeof config::blocks / sizeof(config::blocks[0]) -
                          1]) {
        draw._offset_x += 8;
        XSetForeground(display, gc, 0xD3D3D3);
        XFillRectangle(display, window, gc, draw._offset_x, 3, 2,
                       config::height - 6);
        draw._offset_x += 10;
      }
    }

    XFlush(display);
    auto end = std::chrono::steady_clock::now();

    auto sleep_dur = (std::chrono::milliseconds(1000) - (end - start)) / 160;
    draw._fps = std::chrono::milliseconds(1000) / (end - start + sleep_dur);
    std::this_thread::sleep_for(sleep_dur);
  }

  return 0;
}
