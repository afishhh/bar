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
#include "../connection.hh"
#include "util.hh"

namespace ui::x11 {

class window;

class connection final : public ::ui::connection {
  Display *_display;

  std::jthread _event_thread;

  static int _trapped_error_code;
  static int (*_old_error_handler)(Display *, XErrorEvent *);
  static int _trapped_error_handler(Display *, XErrorEvent *ev) {
    _trapped_error_code = ev->error_code;
    return 0;
  }

  connection() = default;

public:
  connection(const connection &) = delete;
  connection(connection &&) = delete;
  connection &operator=(const connection &) = delete;
  connection &operator=(connection &&) = delete;
  ~connection() noexcept(false);

  static std::optional<std::unique_ptr<connection>> try_create();

  std::unique_ptr<ui::window> create_window(std::string_view name, uvec2 pos,
                                            uvec2 size) override;

  inline Display *display() const { return _display; }
  inline int screen_id() const { return DefaultScreen(_display); }
  inline Screen *screen() const { return DefaultScreenOfDisplay(_display); }
  // XWinID window() const { return _window; }
  // GC gc() const { return _gc; }
  inline XWinID root() const {
    return RootWindow(_display, DefaultScreen(_display));
  }

  inline void flush() const { XFlush(_display); }

  uvec2 available_size() override {
    return uvec2{
        .x = static_cast<unsigned>(DisplayWidth(_display, this->screen_id())),
        .y = static_cast<unsigned>(DisplayHeight(_display, this->screen_id()))};
  }

  Atom intern_atom(std::string_view name, bool only_if_exists = false) const;
  // TODO: Design a better interface around XEmbed, for example allow to handle
  //        when the client stops being embedded.
  //
  //       Focus and activate stuff we should be able to safely ignore since
  //       this is a status bar that is not supposed to gain focus either due
  //       to override-redirect or some window manager specific means.
  std::size_t embed(XWinID client, XWinID parent);

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
class xevent : public Event {
  connection *_conn;
  XEvent _event;

  xevent(connection *conn, XEvent &&event) : _conn(conn), _event(event) {}
  friend class connection;

public:
  ~xevent() { XFreeEventData(_conn->display(), &_event.xcookie); }

  inline XEvent const &raw() const { return _event; }
};

} // namespace ui::x11
