#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "base.hh"

class XWindowBackend : public WindowBackend {
  Display *_display;
  Window _window;
  GC _gc;
  XdbeBackBuffer _back_buffer;
  std::vector<XftFont *> _fonts;

  std::jthread _event_thread;
  std::vector<std::function<void(XEvent)>> _event_handlers;

public:
  XWindowBackend();
  ~XWindowBackend() noexcept(false);

  static bool is_available();

  void pre_draw() override;
  std::unique_ptr<Draw> create_draw() override;
  void post_draw() override;

  void add_event_handler(std::function<void(XEvent)>);
};
