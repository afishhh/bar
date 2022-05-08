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

#include "backends/x11/window_backend.hh"
#include "block.hh"
#include "bufdraw.hh"
#include "config.hh"
#include "format.hh"
#include "guard.hh"
#include "log.hh"
#include "loop.hh"
#include "util.hh"

std::unique_ptr<WindowBackend> select_window_backend() {
  if (XWindowBackend::is_available())
    return std::make_unique<XWindowBackend>();
  else {
    std::print(error, "No suitable window backend found!\n");
    std::exit(1);
  }
}

int main() {
  std::unique_ptr<WindowBackend> window_backend;
  try {
    window_backend = select_window_backend();
  } catch (std::runtime_error &e) {
    std::print(error, "Failed to initialize window backend: {}\n", e.what());
    std::exit(1);
  }

  std::unique_ptr<Draw> real_draw = window_backend->create_draw();
  BufDraw draw(*real_draw);

  struct BlockInfo {
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_update;
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_draw;
  };
  std::unordered_map<Block *, BlockInfo> block_info;
  auto &loop = EventLoop::instance();

  auto setup_block = [&](Block &block) {
    block.late_init();

    auto &info = block_info[&block];
    info.last_update = std::chrono::steady_clock::now();
    block.update();

    info.last_draw = std::chrono::steady_clock::now();

    if (block.update_interval() != std::chrono::duration<double>::max())
      loop.add_timer(true,
                     std::chrono::duration_cast<EventLoop::duration>(
                         block.update_interval()),
                     [&](auto) {
                       block.update();
                       loop.fire_event(EventLoop::Event::REDRAW);
                     });
    if (auto i = block.animate_interval())
      loop.add_timer(true, std::move(*i), [&](auto delta) {
        block.animate(delta);
        loop.fire_event(EventLoop::Event::REDRAW);
      });
  };

  for (auto &block : config::left_blocks)
    setup_block(*block);
  for (auto &block : config::right_blocks)
    setup_block(*block);

  loop.on_event(EventLoop::Event::REDRAW, [&]() {
    window_backend->pre_draw();

    std::size_t x = 5;

    for (auto &block : config::left_blocks) {
      auto now = std::chrono::steady_clock::now();
      auto info = block_info[block.get()];
      x += block->draw(*real_draw, now - info.last_draw);
      x += block->ddraw(*real_draw, now - info.last_draw, x, false);
      info.last_draw = now;

      if (&block != &config::left_blocks[sizeof config::left_blocks /
                                             sizeof(config::left_blocks[0]) -
                                         1]) {
        x += 8;
        real_draw->frect(x, 3, 2, real_draw->height() - 6, 0xD3D3D3);
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
      real_draw->frect(x - 8, 0, width + 16, config::height, 0x000000);
      draw.draw_offset(x, 0);
      draw.clear();

      if (&block != &config::right_blocks[0]) {
        x -= 8 + 2;
        real_draw->frect(x, 3, 2, draw.height() - 6, 0xD3D3D3);
        x -= 8;
      }
    }

    window_backend->post_draw();
  });

  loop.run();

  return 0;
}
