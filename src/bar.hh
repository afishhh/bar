#include <chrono>
#include <memory>
#include <ranges>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "block.hh"
#include "bufdraw.hh"
#include "config.hh"
#include "events.hh"
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
  struct BlockInfo {
    Block &block;
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_update;
    std::chrono::time_point<std::chrono::steady_clock,
                            std::chrono::duration<double>>
        last_draw;

    uvec2 last_pos;
    uvec2 last_size;

    BlockInfo(Block &block) : block(block) {}
  };

  std::unique_ptr<ui::connection> _connection;
  std::unique_ptr<ui::window> _window;
  std::unique_ptr<ui::window> _tooltip_window;

  // Used to implement tooltip drawing
  BlockInfo *_hovered_block;
  std::chrono::time_point<std::chrono::steady_clock,
                          std::chrono::duration<double>>
      _last_mouse_move;
  std::chrono::time_point<std::chrono::steady_clock,
                          std::chrono::duration<double>>
      _last_tooltip_draw;

  void _init_ui() {
    if (auto conn_opt = ui::x11::connection::try_create(); conn_opt) {
      _connection = std::unique_ptr(std::move(*conn_opt));
      _window = _connection->create_window(
          config::x11::window_name, {0, 0},
          {_connection->available_size().x, config::height});

      if (auto *xwin = dynamic_cast<ui::x11::window *>(_window.get()); xwin) {
        xwin->class_hint(config::x11::window_class, config::x11::window_class);
        if (config::x11::override_redirect)
          xwin->override_redirect(true);
      }

      _tooltip_window = _connection->create_window(
          fmt::format("{} tooltip", config::x11::window_name), {0, 0}, {1, 1});

      if (auto *xwin = dynamic_cast<ui::x11::window *>(_tooltip_window.get());
          xwin)
        xwin->override_redirect(true);

      auto fonts = std::make_shared<ui::x11::fonts>(
          (ui::x11::connection *)_connection.get());
      for (auto fname : config::fonts)
        fonts->add(fname);

      ((ui::x11::draw &)_window->drawer()).set_fonts(fonts);
      ((ui::x11::draw &)_tooltip_window->drawer()).set_fonts(fonts);

      // TODO: Abstract this away from X11
      auto x11conn = static_cast<ui::x11::connection *>(_connection.get());
      auto x11mainwin = static_cast<ui::x11::window *>(_window.get());
      auto x11tooltipwin =
          static_cast<ui::x11::window *>(_tooltip_window.get());
      XSetTransientForHint(x11conn->display(), x11tooltipwin->window_id(),
                           x11mainwin->window_id());
      XSelectInput(x11conn->display(), x11conn->root(), PointerMotionMask);
      XSelectInput(x11conn->display(), x11mainwin->window_id(),
                   LeaveWindowMask | EnterWindowMask);
      XSelectInput(x11conn->display(), x11tooltipwin->window_id(),
                   LeaveWindowMask | EnterWindowMask);
      EV.on<ui::x11::xevent>([this](ui::x11::xevent const &ev) {
        XEvent const &e = ev;
        if (e.type == MotionNotify) {
          auto block_lists = {std::views::all(_right_blocks),
                              std::views::all(_left_blocks)};
          auto all_blocks = std::ranges::join_view(block_lists);
          unsigned x = e.xmotion.x, y = e.xmotion.y;
          _last_mouse_move = std::chrono::steady_clock::now();

          _hovered_block = nullptr;
          for (auto &info : all_blocks) {
            if (info.last_pos.x <= x && info.last_pos.y <= y &&
                info.last_pos.x + info.last_size.x > x &&
                info.last_pos.y + info.last_size.y > y) {
              _hovered_block = &info;
              break;
            }
          }
        } else if (e.type == LeaveNotify) {
          // TODO: Allow mousing over the tooltip window
          _hovered_block = nullptr;
        }
      });
    } else {
      fmt::print(error, "No suitable window backend found!\n");
      std::exit(1);
    }
  }

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
      fmt::print(error, "Failed to initialize window backend: {}\n", e.what());
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
  ui::window &tooltip_window() { return *_tooltip_window; }

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

    {
      auto filtered =
          _left_blocks | std::views::filter([](BlockInfo const &info) {
            return !info.block.skip();
          });
      auto it = filtered.begin();
      if (it != filtered.end())
        while (true) {
          auto &info = *it;
          auto &block = info.block;
          auto now = std::chrono::steady_clock::now();
          auto width = block.draw(buffered_draw, now - info.last_draw);
          width += block.ddraw(buffered_draw, now - info.last_draw, x, false);
          info.last_pos = {(unsigned)x, 0};
          info.last_size = {(unsigned)width, config::height};
          info.last_draw = now;

          buffered_draw.draw_offset(x, 0);
          buffered_draw.clear();

          x += width;

          if (++it == filtered.end())
            break;

          x += 8;
          direct_draw.frect(x, 3, 2, direct_draw.height() - 6, 0xD3D3D3);
          x += 8 + 2;
        }
    }

    x = direct_draw.width() - 5;

    {
      auto filtered =
          _right_blocks | std::views::filter([](BlockInfo const &info) {
            return !info.block.skip();
          });
      auto it = filtered.begin();
      if (it != filtered.end())
        while (true) {
          auto &info = *it;
          auto &block = info.block;
          auto now = std::chrono::steady_clock::now();
          auto width = block.draw(buffered_draw, now - info.last_draw);
          width +=
              block.ddraw(buffered_draw, now - info.last_draw, x - width, true);
          info.last_pos = {(unsigned)(x - width), 0};
          info.last_size = {(unsigned)width, config::height};
          info.last_draw = now;

          x -= width;
          direct_draw.frect(x - 8, 0, width + 16, config::height, 0x000000);
          buffered_draw.draw_offset(x, 0);
          buffered_draw.clear();

          if (++it == filtered.end())
            break;

          if (it != filtered.begin()) {
            x -= 8 + 2;
            direct_draw.frect(x, 3, 2, direct_draw.height() - 6, 0xD3D3D3);
            x -= 8;
          }
        }
    }

    if (_hovered_block && _hovered_block->block.has_tooltip()) {
      auto now = std::chrono::steady_clock::now();
      auto &block = _hovered_block->block;
      auto bd = BufDraw(_tooltip_window->drawer());
      block.draw_tooltip(bd, now - _last_tooltip_draw,
                         _hovered_block->last_size.x);

      auto dim = bd.calculate_size();

      uvec2 pos{_hovered_block->last_pos.x +
                    (signed)(_hovered_block->last_size.x - dim.x - 16) / 2,
                (unsigned)(_hovered_block->last_pos.y + config::height)};
      uvec2 size{dim.x + 16, dim.y + 16};
      auto dsize = _connection->available_size();

      if (pos.x + size.x > dsize.x)
        pos.x = dsize.x - size.x;
      // If pos.x overflows then it will surely be bigger than dsize.x and
      // if something else lead to it being bigger then we won't be able to
      // put it outside the screen anyway
      if (pos.x > dsize.x)
        pos.x = 0;

      _tooltip_window->moveresize(pos, size);
      _tooltip_window->drawer().hrect(0, 0, size.x - 1, size.y - 1,
                                      color(0xFFAA00));
      bd.draw_offset(8, 8);

      _last_tooltip_draw = now;
      _tooltip_window->show();
      _tooltip_window->flip();
    } else
      _tooltip_window->hide();

    _window->flip();
  }
};
