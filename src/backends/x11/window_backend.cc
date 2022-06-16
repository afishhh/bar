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

#include "../../config.hh"
#include "../../draw.hh"
#include "../../format.hh"
#include "../../log.hh"
#include "draw.hh"
#include "window_backend.hh"

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

int XWindowBackend::_trapped_error_code{0};
int (*XWindowBackend::_old_error_handler)(Display *, XErrorEvent *){nullptr};

bool XWindowBackend::is_available() {
  return std::getenv("DISPLAY") != nullptr;
}

XWindowBackend::XWindowBackend() {
  XInitThreads();

  // Create a connection to the X server
  _display = XOpenDisplay(nullptr);
  if (_display == NULL)
    throw std::runtime_error("Cannot open display");

  {
    int mayor, minor;
    if (XdbeQueryExtension(_display, &mayor, &minor))
      std::print(info, "Supported Xdbe extension version {}.{}\n", mayor,
                 minor);
    else
      throw std::runtime_error("Xdbe extension not supported");
  }

  int screen = DefaultScreen(_display);
  int display_width = DisplayWidth(_display, screen);

  _window = XCreateSimpleWindow(_display, RootWindow(_display, screen), 0, 0,
                                display_width, config::height, 0, 0, 0);

  XStoreName(_display, _window, config::x11::window_name.data());

  if (config::x11::override_redirect) {
    XSetWindowAttributes attr;
    attr.override_redirect = true;
    XChangeWindowAttributes(_display, _window, CWOverrideRedirect, &attr);
  }

  XClassHint class_hint;
  class_hint.res_name = const_cast<char *>(config::x11::window_class.data());
  class_hint.res_class = const_cast<char *>(config::x11::window_class.data());
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

  // ------- Setup event handling -------

  _event_thread = std::jthread([this](std::stop_token token) {
    while (!token.stop_requested()) {
      XEvent event;
      while (!XEventsQueued(_display, QueuedAfterFlush)) {
        if (token.stop_requested())
          return;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }
      XNextEvent(_display, &event);

      EV.fire_event(LXEvent(std::move(event)));
    }
  });
  _event_thread.detach();
}

XWindowBackend::~XWindowBackend() noexcept(false) {
  _event_thread.request_stop();
  _event_thread.join();

  XFreeGC(_display, _gc);
  XUnmapWindow(_display, _window);
  XDestroyWindow(_display, _window);
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

Atom XWindowBackend::intern_atom(std::string_view name, bool only_if_exists) const {
  Atom atom = XInternAtom(_display, name.data(), only_if_exists);
  if (!atom)
    throw std::runtime_error(
        std::format("Cannot intern atom {}", std::quoted(name)));
  return atom;
}

std::size_t XWindowBackend::embed(Window window, Window parent) {
  if (!XReparentWindow(_display, window, parent, 0, 0))
    throw std::runtime_error("XReparentWindow failed");

  struct XEmbedInfo {
    long version;
    long flags;
  };

  if (!XMapWindow(_display, window))
    throw std::runtime_error("XMapWindow failed");

  if (!XSelectInput(_display, window, PropertyChangeMask))
    throw std::runtime_error("XSelectInput failed");
  // FIXME: This leaks memory as it's never removed!
  //        And is also ugly since we need to return the id in case of failure.
  auto id = EV.on<LXEvent>([this, window](const LXEvent &lxevent) {
    const auto &event = lxevent.xevent();
    if (event.type != PropertyNotify || event.xproperty.window != window)
      return;

    if (event.xproperty.atom == intern_atom("_XEMBED_INFO") &&
        event.xproperty.state == PropertyNewValue) {
      Atom type;
      unsigned long len, bytes_left;
      int format;
      unsigned char *data;

      if (!XGetWindowProperty(_display, window, intern_atom("_XEMBED_INFO"), 0,
                              sizeof(XEmbedInfo), false,
                              intern_atom("_XEMBED_INFO"), &type, &format, &len,
                              &bytes_left, &data))
        return; // Happily ignore errors caused by misbehaving clients.

      XEmbedInfo *info = reinterpret_cast<XEmbedInfo *>(data);
      if (info->flags & XEMBED_MAPPED) {
        if (!XMapWindow(_display, window))
          throw std::runtime_error("XMapWindow failed");
      } else if (!XUnmapWindow(_display, window))
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

  if (!XSendEvent(_display, window, false, NoEventMask, &event))
    throw std::runtime_error("XSendEvent failed");
  XFlush(_display);

  return id;
}
