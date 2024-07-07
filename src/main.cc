#include <X11/X.h>
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
#include <uv.h>
#include <vector>

#include "bar.hh"
#include "block.hh"
#include "config.hh"
#include "log.hh"
#include "ui/gl.hh"
#include "util.hh"

int main() {
  std::locale::global(std::locale(""));

  bar &bar = bar::instance();

  for (auto &block : config::left_blocks)
    bar.add_left(*block);

  for (auto &block : config::right_blocks | std::views::reverse)
    bar.add_right(*block);

  bar.start_ui();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  uv_loop_close(uv_default_loop());

  return 0;
}
