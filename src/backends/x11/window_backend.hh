#pragma once

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../../loop.hh"
#include "../base.hh"

class XWindowBackend : public WindowBackend {
  Display *_display;
  Window _window;
  GC _gc;
  XdbeBackBuffer _back_buffer;
  std::vector<XftFont *> _fonts;

  std::jthread _event_thread;

  static int _trapped_error_code;
  static int (*_old_error_handler)(Display *, XErrorEvent *);
  static int _trapped_error_handler(Display *, XErrorEvent *ev) {
    _trapped_error_code = ev->error_code;
    return 0;
  }

public:
  XWindowBackend();
  ~XWindowBackend() noexcept(false);

  static bool is_available();

  void pre_draw() override;
  std::unique_ptr<Draw> create_draw() override;
  void post_draw() override;

  Display *display() { return _display; }
  Screen *screen() { return DefaultScreenOfDisplay(_display); }
  Window window() { return _window; }
  GC gc() { return _gc; }

  Atom intern_atom(std::string_view name, bool only_if_exists = false);
  // TODO: Design a better interface around XEmbed, for example allow to handle
  //        when the client stops being embedded.
  //
  //       Focus and activate stuff we should be able to safely ignore since
  //       this is a status bar that is not supposed to gain focus either due
  //       to override-redirect or some window manager specific means.
  std::size_t embed(Window client, Window parent);

  // FIXME: What if an errors occurs in a different thread?
  //        Is it even possible to handle something like that?
  static void trap_errors() {
    _trapped_error_code = 0;
    _old_error_handler = XSetErrorHandler(_trapped_error_handler);
  }
  static void untrap_errors() { XSetErrorHandler(_old_error_handler); }
  static int trapped_error() { return _trapped_error_code; }
};

// This Event wraps an XEvent and makes it accessible via the event loop.
class LXEvent : public Event {
  XEvent _event;

  LXEvent(XEvent &&event) : _event(event) {}
  friend class XWindowBackend;

public:
  inline const XEvent &xevent() const { return _event; }
};
