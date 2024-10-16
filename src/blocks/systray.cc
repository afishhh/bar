#define GLFW_EXPOSE_NATIVE_X11

#include "../ui/gl.hh"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include <fmt/core.h>

#include "../bar.hh"
#include "../config.hh"
#include "../log.hh"
#include "systray.hh"

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define SYSTEM_TRAY_BEGIN_MESSAGE 1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

thread_local int trapped_x11_error_code = 0;

void XSystrayBlock::relayout_tray() {
  Display *display = glfwGetX11Display();
  if (!display) {
    fmt::print(warn, "XSystrayBlock only works on the X11 backend\n");
    return;
  }

relayout_tray:
  if (!XResizeWindow(display, *_tray, (config::height * _icons.size()) ?: 1, config::height))
    throw std::runtime_error("Failed to resize tray window");

  std::size_t i = 0;
  for (auto const &[window, _] : _icons) {
    std::uint32_t bad_windows = 0;

    while (true) {
      trapped_x11_error_code = 0;
      XErrorHandler old_handler = XSetErrorHandler([](Display *, XErrorEvent *ev) {
        trapped_x11_error_code = ev->error_code;
        return 0;
      });
      XMoveResizeWindow(display, window, i * config::height, 0, config::height, config::height);
      XSync(display, false);
      XSetErrorHandler(old_handler);

      if (auto err = trapped_x11_error_code) {
        if (err == BadWindow) {
          if (++bad_windows >= 10) {
            _icons.erase(window);
            fmt::print(warn, "xsystray: Undocked broken window {}\n", window);
            goto relayout_tray;
          }
        } else
          fmt::print(warn, "xsystray: Could not move/resize window {} (X error: {})\n", window, err);
      }

      break;
    }

    ++i;
  }

  XFlush(display);
  bar::instance().schedule_redraw();
}

void XSystrayBlock::setup() {
  Display *display = glfwGetX11Display();
  if (!display) {
    fmt::print(warn, "XSystrayBlock only works on the X11 backend\n");
    return;
  }

  // ui::x11::window &mainw = static_cast<ui::x11::window &>(bar.window());
  //
  // _tray = XCreateSimpleWindow(*xconn, mainw.window_id(), 0, 0, 10, config::height, 0, 0, 0);
  // if (!_tray)
  //   throw std::runtime_error("Failed to create tray window");
  // if (!XMapWindow(*xconn, *_tray))
  //   throw std::runtime_error("Failed to map tray window");
  // // Selecting SubstructureNotifyMask will allow us to recieve DestroyNotify and
  // // ReparentNotify
  // if (!XSelectInput(*xconn, *_tray, SubstructureNotifyMask))
  //   throw std::runtime_error("Failed to select input for tray window");
  //
  // auto opcode_atom = xconn->intern_atom("_NET_SYSTEM_TRAY_OPCODE", false);
  // EV.on<ui::x11::xevent>([xconn, this, opcode_atom](const XEvent &e) {
  //   if (e.type == ClientMessage && e.xclient.message_type == opcode_atom) {
  //     switch (e.xclient.data.l[1]) {
  //     case SYSTEM_TRAY_REQUEST_DOCK: {
  //       auto window = e.xclient.data.l[2];
  //       if (_icons.contains(window)) {
  //         fmt::print(warn, "xsystray: Window {} requested docking twice", window);
  //         return;
  //       }
  //
  //       xconn->trap_errors();
  //       auto embedder(xconn->embed(window, *_tray));
  //       XSetWindowBackground(xconn->display(), window, config::background_color.as_rgb());
  //       XSync(*xconn, false);
  //       xconn->untrap_errors();
  //       if (auto err = xconn->trapped_error()) {
  //         char error_buffer[256];
  //         XGetErrorText(*xconn, err, error_buffer, sizeof(error_buffer));
  //         fmt::print(warn, "xsystray: Could not dock window {} ({})\n", window, error_buffer);
  //         xconn->trap_errors();
  //         embedder.drop();
  //         XSync(*xconn, false);
  //         xconn->untrap_errors();
  //       } else {
  //         _icons.emplace(window, std::move(embedder));
  //         fmt::print(info, "xsystray: Docked window {}\n", window);
  //       }
  //     } break;
  //     case SYSTEM_TRAY_BEGIN_MESSAGE:
  //       fmt::print(info, "xsystray: System tray BEGIN_MESSAGE (not implemented yet)\n");
  //       return;
  //     case SYSTEM_TRAY_CANCEL_MESSAGE:
  //       fmt::print(info, "xsystray: System tray CANCEL_MESSAGE (not implemented yet)\n");
  //       return;
  //     default:
  //       fmt::print(info, "xsystray: Recieved unknown systray opcode: {}\n", e.xclient.data.l[1]);
  //       return;
  //     }
  //   } else if (e.type == ReparentNotify && _icons.contains(e.xreparent.window) && e.xreparent.parent != *_tray) {
  //     // The client left us for someone else :(
  //     auto window = e.xreparent.window;
  //     _icons.erase(window);
  //     fmt::print(info, "xsystray: Undocked reparented window {}\n", e.xreparent.window);
  //   } else if (e.type == DestroyNotify && _icons.contains(e.xdestroywindow.window)) {
  //     auto window = e.xdestroywindow.window;
  //     _icons.erase(window);
  //     fmt::print(info, "xsystray: Undocked destroyed window {}\n", e.xdestroywindow.window);
  //   } else
  //     return;
  //
  //   this->relayout_tray();
  // });
  //
  // EV.add_timer(1000ms, [this](auto) { this->relayout_tray(); });
  //
  // auto orientation_atom = xconn->intern_atom("_NET_SYSTEM_TRAY_ORIENTATION");
  // auto orientation_value_atom = xconn->intern_atom("_NET_SYSTEM_TRAY_ORIENTATION_HORZ");
  // XChangeProperty(*xconn, *_tray, orientation_atom, XA_CARDINAL, 32, PropModeReplace,
  //                 (unsigned char *)&orientation_value_atom, 1);
  //
  // auto sn = XScreenNumberOfScreen(xconn->screen());
  // auto selection_atom_name = fmt::format("_NET_SYSTEM_TRAY_S{}", sn);
  // auto selection_atom = xconn->intern_atom(selection_atom_name);
  //
  // EV.on<StopEvent>([this, xconn, selection_atom](StopEvent const &) {
  //   if (XGetSelectionOwner(*xconn, selection_atom) == *_tray)
  //     if (!XSetSelectionOwner(*xconn, selection_atom, None, CurrentTime))
  //       warn << "xsystray: Failed to unset system tray selection owner\n";
  //
  //   xconn->trap_errors();
  //   for (auto &[xwin, embedder] : _icons)
  //     embedder.drop();
  //   XSync(*xconn, false);
  //   xconn->untrap_errors();
  // });
  //
  // if (!XSetSelectionOwner(*xconn, selection_atom, *_tray, CurrentTime))
  //   throw std::runtime_error("Failed to set system tray selection owner");
  // if (XGetSelectionOwner(*xconn, selection_atom) != *_tray)
  //   throw std::runtime_error("Failed to obtain system tray ownership");
  //
  // XEvent e;
  // e.xclient.type = ClientMessage;
  // e.xclient.window = XRootWindow(*xconn, sn);
  // e.xclient.format = 32;
  // e.xclient.message_type = xconn->intern_atom("MANAGER");
  // e.xclient.data.l[0] = CurrentTime;
  // e.xclient.data.l[1] = selection_atom;
  // e.xclient.data.l[2] = *_tray;
  // e.xclient.data.l[3] = 0;
  // e.xclient.data.l[4] = 0;
  //
  // XSendEvent(*xconn, e.xclient.window, false, StructureNotifyMask, &e);
  // XSync(*xconn, false);
}

bool XSystrayBlock::skip() { return _icons.empty(); }

std::size_t XSystrayBlock::draw(ui::draw &, std::chrono::duration<double>, size_t x, bool right_aligned) {
  if (!_tray)
    return 0;

  Display *display = glfwGetX11Display();
  if (!display)
    throw std::logic_error("Could not get X11 display from GLFW");

  auto tray_width = (_icons.size() * config::height) ?: 1;
  if (!XMoveWindow(display, *_tray, x - tray_width * right_aligned, 0))
    throw std::runtime_error("Failed to move system tray");

  XSync(display, false);
  return tray_width;
}
