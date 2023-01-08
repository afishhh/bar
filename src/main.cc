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

#include "block.hh"
#include "bufdraw.hh"
#include "config.hh"
#include "events.hh"
#include "format.hh"
#include "guard.hh"
#include "log.hh"
#include "loop.hh"
#include "ui/connection.hh"
#include "ui/x11/connection.hh"
#include "util.hh"

std::unique_ptr<ui::connection> connect_to_ui_backend() {
  if (auto conn = ui::x11::connection::try_create(); conn)
    return std::move(*conn);
  else {
    std::print(error, "No suitable window backend found!\n");
    std::exit(1);
  }
}

int main() {
  std::unique_ptr<ui::connection> ui_connection;
  try {
    ui_connection = connect_to_ui_backend();
  } catch (std::runtime_error &e) {
    std::print(error, "Failed to initialize window backend: {}\n", e.what());
    std::exit(1);
  }

  auto window = ui_connection->create_window(
      "bar", {0, 0}, {ui_connection->available_size().x, config::height});

  if (auto *xwin = dynamic_cast<ui::x11::window *>(window.get()); xwin) {
    xwin->class_hint(config::x11::window_class, config::x11::window_class);
    if (config::x11::override_redirect) {
      xwin->override_redirect(true);
    }
  }

  window->show();

  ui::draw &real_draw = window->drawer();
  BufDraw draw(real_draw);

  for (auto fname : config::fonts) {
    draw.load_font(fname);
  }

  struct BlockInfo {
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_update;
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_draw;
  };
  std::unordered_map<Block *, BlockInfo> block_info;
  auto &loop = EV;

  auto setup_block = [&](Block &block) {
    block.late_init();

    auto &info = block_info[&block];
    info.last_update = std::chrono::steady_clock::now();
    block.update();

    info.last_draw = std::chrono::steady_clock::now();

    if (block.update_interval() != std::chrono::duration<double>::max())
      loop.add_timer(std::chrono::duration_cast<EventLoop::duration>(
                         block.update_interval()),
                     [&](auto) {
                       block.update();
                       loop.fire_event(RedrawEvent());
                     });
    if (auto i = block.animate_interval())
      loop.add_timer(*i, [&](auto delta) {
        block.animate(delta);
        loop.fire_event(RedrawEvent());
      });
  };

  for (auto &block : config::left_blocks)
    setup_block(*block);
  for (auto &block : config::right_blocks)
    setup_block(*block);

  loop.on<RedrawEvent>([&](const RedrawEvent &) {
    std::size_t x = 5;

    for (auto &block : config::left_blocks) {
      auto now = std::chrono::steady_clock::now();
      auto info = block_info[block.get()];
      auto width = block->draw(draw, now - info.last_draw);
      width += block->ddraw(draw, now - info.last_draw, x, false);
      info.last_draw = now;

      draw.draw_offset(x, 0);
      draw.clear();

      x += width;

      if (&block != &config::left_blocks[sizeof config::left_blocks /
                                             sizeof(config::left_blocks[0]) -
                                         1]) {
        x += 8;
        real_draw.frect(x, 3, 2, real_draw.height() - 6, 0xD3D3D3);
        x += 8 + 2;
      }
    }

    x = draw.width() - 5;

    for (auto &block : config::right_blocks | std::views::reverse) {
      auto now = std::chrono::steady_clock::now();
      auto info = block_info[block.get()];
      auto width = block->draw(draw, now - info.last_draw);
      width += block->ddraw(draw, now - info.last_draw, x - width, true);
      info.last_draw = now;

      x -= width;
      real_draw.frect(x - 8, 0, width + 16, config::height, 0x000000);
      draw.draw_offset(x, 0);
      draw.clear();

      if (&block != &config::right_blocks[0]) {
        x -= 8 + 2;
        real_draw.frect(x, 3, 2, draw.height() - 6, 0xD3D3D3);
        x -= 8;
      }
    }

    window->flip();
  });

  loop.run();

  return 0;
}
