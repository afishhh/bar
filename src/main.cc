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
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bar.hh"
#include "block.hh"
#include "config.hh"
#include "events.hh"
#include "executor.hh"
#include "log.hh"
#include "loop.hh"
#include "signal.hh"
#include "util.hh"

int main() {
  SignalEvent::setup();
  StopEvent::attach_to_signals();
  EV.set_executor(std::make_unique<ThreadPoolExecutor>(std::thread::hardware_concurrency() * 2));
  EV.set_exception_handler([](std::exception &e) {
    error << "Uncaught exception of type " << typeid(e).name() << " in event loop\n";
    error << "    what(): " << e.what() << '\n';
    return EventLoop::ExceptionAction::IGNORE;
  });

  bar &bar = bar::instance();

  for (auto &block : config::left_blocks)
    bar.add_left(*block);

  for (auto &block : config::right_blocks | std::views::reverse)
    bar.add_right(*block);

  bar.show();

  EV.on<RedrawEvent>([&](const RedrawEvent &) { bar.redraw(); });

  EV.run();

  // The event loop may still contain xevents that reference the X11 connection owned by the bar instance that's about
  // to be destroyed, therefore we reset the event loop before destroying bar to avoid this use-after-free.
  EV.reset();

  return 0;
}
