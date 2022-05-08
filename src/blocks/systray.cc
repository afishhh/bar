#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "../backends/base.hh"
#include "../backends/x11/window_backend.hh"
#include "../config.hh"
#include "../format.hh"
#include "../log.hh"
#include "systray.hh"

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define SYSTEM_TRAY_BEGIN_MESSAGE 1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

void XSystrayBlock::late_init() {
  XWindowBackend *xb =
      dynamic_cast<XWindowBackend *>(&WindowBackend::instance());
  if (!xb) {
    std::print(warn, "XSystrayBlock requires using X11 backend\n");
    return;
  }

  _tray = XCreateSimpleWindow(xb->display(), xb->window(), 0, 0, 10,
                              config::height, 0, 0, 0);
  if (!_tray)
    throw std::runtime_error("Failed to create tray window");
  if (!XMapWindow(xb->display(), _tray))
    throw std::runtime_error("Failed to map tray window");
  // Selecting SubstructureNotifyMask will allow us to recieve DestroyNotify and
  // ReparentNotify
  if (!XSelectInput(xb->display(), _tray, SubstructureNotifyMask))
    throw std::runtime_error("Failed to select input for tray window");

  auto opcode_atom = xb->intern_atom("_NET_SYSTEM_TRAY_OPCODE", false);
  xb->add_event_handler([xb, this, opcode_atom](XEvent e) {
    if (e.type == ClientMessage && e.xclient.message_type == opcode_atom) {
      switch (e.xclient.data.l[1]) {
      case SYSTEM_TRAY_REQUEST_DOCK: {
        Window window = e.xclient.data.l[2];
        if (_icons.contains(window)) {
          std::print(warn, "xsystray: Window {} requested docking twice",
                     window);
          return;
        }

        xb->trap_errors();
        auto hid = xb->embed(window, _tray);
        XSync(xb->display(), false);
        xb->untrap_errors();
        if (auto err = xb->trapped_error()) {
          char error_buffer[256];
          XGetErrorText(xb->display(), err, error_buffer, sizeof(error_buffer));
          std::print(warn, "xsystray: Could not dock window {} ({})\n", window,
                     error_buffer);
          xb->remove_event_handler(hid);
          xb->trap_errors();
          XReparentWindow(xb->display(), window,
                          XDefaultRootWindow(xb->display()), 0, 0);
          xb->untrap_errors();
          return;
        }
        _icons.insert(window);
        std::print(info, "xsystray: Docked window {}\n", window);
        break;
      }
      case SYSTEM_TRAY_BEGIN_MESSAGE:
        std::print(
            info,
            "xsystray: System tray BEGIN_MESSAGE (not implemented yet)\n");
        return;
      case SYSTEM_TRAY_CANCEL_MESSAGE:
        std::print(
            info,
            "xsystray: System tray CANCEL_MESSAGE (not implemented yet)\n");
        return;
      default:
        std::print(info, "xsystray: Recieved unknown systray opcode: {}\n",
                   e.xclient.data.l[1]);
        return;
      }
    } else if (e.type == ReparentNotify &&
               _icons.contains(e.xreparent.window) &&
               e.xreparent.parent != _tray) {
      // The client left us :(
      _icons.erase(e.xreparent.window);
      std::print(info, "xsystray: Undocked reparented window {}\n",
                 e.xreparent.window);
    } else if (e.type == DestroyNotify &&
               _icons.contains(e.xdestroywindow.window)) {
      _icons.erase(e.xdestroywindow.window);
      std::print(info, "xsystray: Undocked destroyed window {}\n",
                 e.xdestroywindow.window);
    } else
      return;

  relayout_tray:
    if (!XResizeWindow(xb->display(), _tray,
                       (config::height * _icons.size()) ?: 1, config::height))
      throw std::runtime_error("Failed to resize tray window");

    std::size_t i = 0;
    for (auto window : _icons) {
      xb->trap_errors();
      XMoveResizeWindow(xb->display(), window, i * config::height, 0,
                        config::height, config::height);
      XSync(xb->display(), false);
      xb->untrap_errors();
      if (auto err = xb->trapped_error()) {
        if (err == BadWindow) {
          std::print(warn, "xsystray: Undocking broken window {}\n", window);
          _icons.erase(window);
          goto relayout_tray;
        } else
          std::print(
              warn, "xsystray: Could not move/resize window {} (X error: {})\n",
              window, err);
      }
      ++i;
    }

    XFlush(xb->display());
    EventLoop::instance().fire_event(EventLoop::Event::REDRAW);
  });

  auto orientation_atom = xb->intern_atom("_NET_SYSTEM_TRAY_ORIENTATION");
  auto orientation_value_atom =
      xb->intern_atom("_NET_SYSTEM_TRAY_ORIENTATION_HORZ");
  XChangeProperty(xb->display(), _tray, orientation_atom, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&orientation_value_atom, 1);

  auto sn = XScreenNumberOfScreen(xb->screen());
  auto selection_atom_name = std::format("_NET_SYSTEM_TRAY_S{}", sn);
  auto selection_atom = xb->intern_atom(selection_atom_name);
  if (!XSetSelectionOwner(xb->display(), selection_atom, _tray, CurrentTime))
    throw std::runtime_error("Failed to set system tray selection owner");
  if (XGetSelectionOwner(xb->display(), selection_atom) != _tray)
    throw std::runtime_error("Failed to obtain system tray ownership");

  XEvent e;
  e.xclient.type = ClientMessage;
  e.xclient.window = XRootWindow(xb->display(), sn);
  e.xclient.format = 32;
  e.xclient.message_type = xb->intern_atom("MANAGER");
  e.xclient.data.l[0] = CurrentTime;
  e.xclient.data.l[1] = selection_atom;
  e.xclient.data.l[2] = _tray;
  e.xclient.data.l[3] = 0;
  e.xclient.data.l[4] = 0;

  XSendEvent(xb->display(), e.xclient.window, false, StructureNotifyMask, &e);
  XSync(xb->display(), false);
}

std::size_t XSystrayBlock::ddraw(Draw &, std::chrono::duration<double>,
                                 size_t x, bool right_aligned) {
  if (!_tray)
    return 0;

  XWindowBackend *xb =
      dynamic_cast<XWindowBackend *>(&WindowBackend::instance());
  if (!xb)
    throw std::logic_error("Could not get XWindowBackend instance");

  Window root_return;
  int x_return, y_return;
  unsigned int width_return, height_return;
  unsigned int border_width_return, depth_return;
  if (!XGetGeometry(xb->display(), _tray, &root_return, &x_return, &y_return,
                    &width_return, &height_return, &border_width_return,
                    &depth_return))
    throw std::runtime_error("Failed to get geometry of system tray");

  if (!XMoveWindow(xb->display(), _tray, x - width_return * right_aligned, 0))
    throw std::runtime_error("Failed to move system tray");

  XSync(xb->display(), false);
  return width_return;
}
