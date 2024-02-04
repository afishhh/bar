#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdbe.h>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <thread>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "../../config.hh"
#include "../../log.hh"
#include "../draw.hh"
#include "connection.hh"
#include "draw.hh"
#include "util.hh"
#include "window.hh"

/* _XEMBED_INFO flags */
#define XEMBED_MAPPED (1 << 0)
/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define XEMBED_REQUEST_FOCUS 3
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5
#define XEMBED_FOCUS_NEXT 6
#define XEMBED_FOCUS_PREV 7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON 10
#define XEMBED_MODALITY_OFF 11
#define XEMBED_REGISTER_ACCELERATOR 12
#define XEMBED_UNREGISTER_ACCELERATOR 13
#define XEMBED_ACTIVATE_ACCELERATOR 14

namespace ui::x11 {

int connection::_trapped_error_code{0};
int (*connection::_old_error_handler)(Display *, XErrorEvent *){nullptr};

std::unique_ptr<ui::window> connection::create_window(std::string_view name, uvec2 pos, uvec2 size) {
  XWinID w = XCreateSimpleWindow(_display, this->root(), pos.x, pos.y, size.x, size.y, 0, 0, 0);

  XStoreName(_display, w, name.data());

  return std::unique_ptr<window>(new x11::window(this, w));
}

std::optional<std::unique_ptr<connection>> connection::try_create() {
  if (getenv("DISPLAY") == nullptr)
    return std::nullopt;

  XInitThreads();

  // Create a connection to the X server
  auto display = XOpenDisplay(nullptr);
  if (display == NULL)
    return std::nullopt;

  {
    int mayor, minor;
    if (XdbeQueryExtension(display, &mayor, &minor))
      fmt::print(info, "Supported Xdbe extension version {}.{}\n", mayor, minor);
    else
      throw std::runtime_error("Xdbe extension not supported");
  }

  // Replace with: connection.load_font();
  // for (auto font_name : config::fonts) {
  //   XftFont *font = XftFontOpenName(_display, screen, font_name);
  //   if (font == NULL)
  //     throw std::runtime_error(
  //         fmt::format("Cannot load font {}", std::quoted(font_name)));
  //   _fonts.push_back(font);
  // }

  // ------- Setup event handling -------

  XSetIOErrorHandler([](Display *) -> int {
    exit(1);
  });

  auto conn = new x11::connection;
  conn->_display = display;
  conn->_event_thread = std::jthread([conn](std::stop_token token) {
    while (!token.stop_requested()) {
      XEvent event;
      while (!XEventsQueued(conn->_display, QueuedAfterFlush)) {
        if (token.stop_requested())
          return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      XNextEvent(conn->_display, &event);

      EV.fire_event(xevent(conn, std::move(event)));
    }

    XCloseDisplay(conn->_display);
  });

  return std::unique_ptr<x11::connection>(conn);
}

connection::~connection() noexcept(false) {
  _event_thread.request_stop();
}

Atom connection::intern_atom(std::string_view name, bool only_if_exists) const {
  Atom atom = XInternAtom(_display, name.data(), only_if_exists);
  if (!atom)
    throw std::runtime_error(fmt::format("Cannot intern atom {:?}", name));
  return atom;
}

embedder connection::embed(XWinID client, XWinID parent) {
  if (!XReparentWindow(_display, client, parent, 0, 0))
    throw std::runtime_error("XReparentWindow failed");

  struct XEmbedInfo {
    long version;
    long flags;
  };

  if (!XMapWindow(_display, client))
    throw std::runtime_error("XMapWindow failed");

  if (!XSelectInput(_display, client, PropertyChangeMask))
    throw std::runtime_error("XSelectInput failed");

  auto id = EV.on<xevent>([this, winid = client](const XEvent &event) {
    if (event.type != PropertyNotify || event.xproperty.window != winid)
      return;

    if (event.xproperty.atom == intern_atom("_XEMBED_INFO") && event.xproperty.state == PropertyNewValue) {
      Atom type;
      unsigned long len, bytes_left;
      int format;
      unsigned char *data;

      if (!XGetWindowProperty(_display, winid, intern_atom("_XEMBED_INFO"), 0, sizeof(XEmbedInfo), false,
                              intern_atom("_XEMBED_INFO"), &type, &format, &len, &bytes_left, &data))
        return; // Happily ignore errors caused by misbehaving clients.

      XEmbedInfo *info = reinterpret_cast<XEmbedInfo *>(data);
      if (info->flags & XEMBED_MAPPED) {
        if (!XMapWindow(_display, winid))
          throw std::runtime_error("XMapWindow failed");
      } else if (!XUnmapWindow(_display, winid))
        throw std::runtime_error("XUnmapWindow failed");
    }
  });

  XEvent event;
  event.xclient.type = ClientMessage;
  event.xclient.message_type = intern_atom("_XEMBED");
  event.xclient.format = 32;
  event.xclient.data.l[0] = CurrentTime;
  event.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY;
  event.xclient.data.l[2] = 0;
  event.xclient.data.l[3] = parent;
  event.xclient.data.l[4] = 0;

  if (!XSendEvent(_display, client, false, NoEventMask, &event))
    throw std::runtime_error("XSendEvent failed");
  XFlush(_display);

  return embedder(this, client, id);
}

} // namespace ui::x11
