#include <memory>

#include "block.hh"
#include "bufdraw.hh"
#include "config.hh"
#include "events.hh"
#include "format.hh"
#include "guard.hh"
#include "log.hh"
#include "loop.hh"
#include "ui/connection.hh"
#include "ui/draw.hh"
#include "ui/window.hh"
#include "ui/x11/connection.hh"
#include "ui/x11/window.hh"
#include "util.hh"

class bar {
  std::unique_ptr<ui::connection> _connection;
  std::unique_ptr<ui::window> _window;

  void _init_ui() {
    if (auto conn_opt = ui::x11::connection::try_create(); conn_opt) {
      _connection = std::unique_ptr(std::move(*conn_opt));
      _window = _connection->create_window(
          "bar", {0, 0}, {_connection->available_size().x, config::height});

      if (auto *xwin = dynamic_cast<ui::x11::window *>(_window.get()); xwin) {
        xwin->class_hint(config::x11::window_class, config::x11::window_class);
        if (config::x11::override_redirect) {
          xwin->override_redirect(true);
        }
      }

      auto &drawer = _window->drawer();
      for (auto fname : config::fonts) {
        drawer.load_font(fname);
      }
    } else {
      std::print(error, "No suitable window backend found!\n");
      std::exit(1);
    }
  }

  struct BlockInfo {
    Block &block;
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_update;
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_draw;

    BlockInfo(Block &block) : block(block) {}
  };
  std::vector<BlockInfo> _left_blocks;
  std::vector<BlockInfo> _right_blocks;

  void _setup_block(BlockInfo &info) {
    info.block.late_init();

    info.last_update = std::chrono::steady_clock::now();
    info.block.update();

    info.last_draw = std::chrono::steady_clock::now();

    if (info.block.update_interval() != std::chrono::duration<double>::max())
      EV.add_timer(std::chrono::duration_cast<EventLoop::duration>(
                       info.block.update_interval()),
                   [&block = info.block](auto) {
                     block.update();
                     EV.fire_event(RedrawEvent());
                   });
    if (auto i = info.block.animate_interval())
      EV.add_timer(*i, [&block = info.block](auto delta) {
        block.animate(delta);
        EV.fire_event(RedrawEvent());
      });
  }

  bar() {
    try {
      _init_ui();
    } catch (std::runtime_error &e) {
      std::print(error, "Failed to initialize window backend: {}\n", e.what());
      std::exit(1);
    }
  }

public:
  static bar &instance() {
    static bar instance;
    return instance;
  }

  void show() { _window->show(); }

  ui::connection &connection() { return *_connection; }
  ui::window &window() { return *_window; }

  void add_left(Block &block) {
    _setup_block(_left_blocks.emplace_back(block));
  };

  void add_right(Block &block) {
    _setup_block(_right_blocks.emplace_back(block));
  };

  void redraw() {
    std::size_t x = 5;

    auto &direct_draw = _window->drawer();
    auto buffered_draw = BufDraw(direct_draw);

    for (auto &info : _left_blocks) {
      auto &block = info.block;
      auto now = std::chrono::steady_clock::now();
      auto width = block.draw(buffered_draw, now - info.last_draw);
      width += block.ddraw(buffered_draw, now - info.last_draw, x, false);
      info.last_draw = now;

      buffered_draw.draw_offset(x, 0);
      buffered_draw.clear();

      x += width;

      if (&info != &_left_blocks.back()) {
        x += 8;
        direct_draw.frect(x, 3, 2, direct_draw.height() - 6, 0xD3D3D3);
        x += 8 + 2;
      }
    }

    x = direct_draw.width() - 5;

    for (auto &info : _right_blocks) {
      auto &block = info.block;
      auto now = std::chrono::steady_clock::now();
      auto width = block.draw(buffered_draw, now - info.last_draw);
      width +=
          block.ddraw(buffered_draw, now - info.last_draw, x - width, true);
      info.last_draw = now;

      x -= width;
      direct_draw.frect(x - 8, 0, width + 16, config::height, 0x000000);
      buffered_draw.draw_offset(x, 0);
      buffered_draw.clear();

      if (&info != &_right_blocks.back() && &info != &_right_blocks.front()) {
        x -= 8 + 2;
        direct_draw.frect(x, 3, 2, direct_draw.height() - 6, 0xD3D3D3);
        x -= 8;
      }
    }

    _window->flip();
  }
};
