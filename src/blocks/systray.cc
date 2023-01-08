#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "../config.hh"
#include "../events.hh"
#include "../format.hh"
#include "../log.hh"
#include "../ui/x11/connection.hh"
#include "../ui/x11/window.hh"
#include "systray.hh"

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define SYSTEM_TRAY_BEGIN_MESSAGE 1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

void XSystrayBlock::late_init() {
  auto *xconn =
      dynamic_cast<ui::x11::connection *>(&ui::connection::instance());
  if (!xconn) {
    std::print(warn, "XSystrayBlock requires using the X11 backend\n");
    return;
  }

  std::print(error, "TODO: Port XSystrayBlock to new ui system\n");
  return;
  _tray = XCreateSimpleWindow(xconn->display(), 0 /* TODO: xconn->window() */,
                              0, 0, 10, config::height, 0, 0, 0);
  if (!_tray)
    throw std::runtime_error("Failed to create tray window");
  if (!XMapWindow(xconn->display(), _tray))
    throw std::runtime_error("Failed to map tray window");
  // Selecting SubstructureNotifyMask will allow us to recieve DestroyNotify and
  // ReparentNotify
  if (!XSelectInput(xconn->display(), _tray, SubstructureNotifyMask))
    throw std::runtime_error("Failed to select input for tray window");

  auto opcode_atom = xconn->intern_atom("_NET_SYSTEM_TRAY_OPCODE", false);
  EV.on<ui::x11::xevent>([xconn, this, opcode_atom](const ui::x11::xevent &le) {
    const XEvent &e = le.raw();
    if (e.type == ClientMessage && e.xclient.message_type == opcode_atom) {
      switch (e.xclient.data.l[1]) {
      case SYSTEM_TRAY_REQUEST_DOCK: {
        auto window = ui::x11::window(xconn, e.xclient.data.l[2]);
        if (_icons.contains(window.window_id())) {
          std::print(warn, "xsystray: Window {} requested docking twice",
                     window.window_id());
          return;
        }

        xconn->trap_errors();
        auto hid = xconn->embed(window, ui::x11::window(xconn, _tray));
        XSync(xconn->display(), false);
        xconn->untrap_errors();
        if (auto err = xconn->trapped_error()) {
          char error_buffer[256];
          XGetErrorText(xconn->display(), err, error_buffer,
                        sizeof(error_buffer));
          std::print(warn, "xsystray: Could not dock window {} ({})\n", window.window_id(),
                     error_buffer);
          EV.off<ui::x11::xevent>(hid);
          xconn->trap_errors();
          XReparentWindow(xconn->display(), window.window_id(),
                          XDefaultRootWindow(xconn->display()), 0, 0);
          xconn->untrap_errors();
          return;
        }
        _icons.insert(window.window_id());
        std::print(info, "xsystray: Docked window {}\n", window.window_id());
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
      // The client left us for someone else :(
      auto window = e.xreparent.window;
      EV.off<ui::x11::xevent>(_icon_embed_callbacks[window]);
      _icon_embed_callbacks.erase(window);
      _icons.erase(window);
      std::print(info, "xsystray: Undocked reparented window {}\n",
                 e.xreparent.window);
    } else if (e.type == DestroyNotify &&
               _icons.contains(e.xdestroywindow.window)) {
      auto window = e.xdestroywindow.window;
      EV.off<ui::x11::xevent>(_icon_embed_callbacks[window]);
      _icon_embed_callbacks.erase(window);
      _icons.erase(window);
      std::print(info, "xsystray: Undocked destroyed window {}\n",
                 e.xdestroywindow.window);
    } else
      return;

  relayout_tray:
    if (!XResizeWindow(xconn->display(), _tray,
                       (config::height * _icons.size()) ?: 1, config::height))
      throw std::runtime_error("Failed to resize tray window");

    std::size_t i = 0;
    for (auto window : _icons) {
      xconn->trap_errors();
      XMoveResizeWindow(xconn->display(), window, i * config::height, 0,
                        config::height, config::height);
      XSync(xconn->display(), false);
      xconn->untrap_errors();
      if (auto err = xconn->trapped_error()) {
        if (err == BadWindow) {
          std::print(warn, "xsystray: Undocking broken window {}\n", window);
          EV.off<ui::x11::xevent>(_icon_embed_callbacks[window]);
          _icon_embed_callbacks.erase(window);
          _icons.erase(window);
          goto relayout_tray;
        } else
          std::print(
              warn, "xsystray: Could not move/resize window {} (X error: {})\n",
              window, err);
      }
      ++i;
    }

    XFlush(xconn->display());
    EV.fire_event(RedrawEvent());
  });

  auto orientation_atom = xconn->intern_atom("_NET_SYSTEM_TRAY_ORIENTATION");
  auto orientation_value_atom =
      xconn->intern_atom("_NET_SYSTEM_TRAY_ORIENTATION_HORZ");
  XChangeProperty(xconn->display(), _tray, orientation_atom, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&orientation_value_atom, 1);

  auto sn = XScreenNumberOfScreen(xconn->screen());
  auto selection_atom_name = std::format("_NET_SYSTEM_TRAY_S{}", sn);
  auto selection_atom = xconn->intern_atom(selection_atom_name);
  if (!XSetSelectionOwner(xconn->display(), selection_atom, _tray, CurrentTime))
    throw std::runtime_error("Failed to set system tray selection owner");
  if (XGetSelectionOwner(xconn->display(), selection_atom) != _tray)
    throw std::runtime_error("Failed to obtain system tray ownership");

  XEvent e;
  e.xclient.type = ClientMessage;
  e.xclient.window = XRootWindow(xconn->display(), sn);
  e.xclient.format = 32;
  e.xclient.message_type = xconn->intern_atom("MANAGER");
  e.xclient.data.l[0] = CurrentTime;
  e.xclient.data.l[1] = selection_atom;
  e.xclient.data.l[2] = _tray;
  e.xclient.data.l[3] = 0;
  e.xclient.data.l[4] = 0;

  XSendEvent(xconn->display(), e.xclient.window, false, StructureNotifyMask,
             &e);
  XSync(xconn->display(), false);
}

std::size_t XSystrayBlock::ddraw(ui::draw &, std::chrono::duration<double>,
                                 size_t x, bool right_aligned) {
  if (!_tray)
    return 0;

  ui::x11::connection *xconn =
      dynamic_cast<ui::x11::connection *>(&ui::connection::instance());
  if (!xconn)
    throw std::logic_error("Could not get XWindowBackend instance");

  auto tray_width = (_icons.size() * config::height) ?: 1;
  if (!XMoveWindow(xconn->display(), _tray, x - tray_width * right_aligned, 0))
    throw std::runtime_error("Failed to move system tray");

  XSync(xconn->display(), false);
  return tray_width;
}
