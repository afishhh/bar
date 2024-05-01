#include <uv.h>
#define GLFW_EXPOSE_NATIVE_X11

#include "ui/gl.hh"

#include <chrono>
#include <latch>
#include <memory>
#include <ranges>
#include <thread>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "block.hh"
#include "bufdraw.hh"
#include "config.hh"
#include "guard.hh"
#include "log.hh"
#include "ui/draw.hh"
#include "ui/window.hh"
#include "util.hh"

class bar {
  struct BlockInfo {
    Block &block;

    uvec2 last_pos;
    uvec2 last_size;

    BlockInfo(Block &block) : block(block) {}
    BlockInfo(BlockInfo const &) = delete;
    BlockInfo(BlockInfo &&) = default;
    BlockInfo &operator=(BlockInfo const &) = delete;
    BlockInfo &operator=(BlockInfo &&) = delete;
  };

  std::jthread _ui_thread;
  std::atomic<bool> _redraw_requested;
  std::chrono::steady_clock::time_point _last_redraw;

  ui::gwindow _window;
  ui::gwindow _tooltip_window;

  // Used to implement tooltip drawing
  ivec2 _monitor_size;
  BlockInfo *_hovered_block;
  int _hovered_block_threatened;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> _last_mouse_move;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> _last_tooltip_draw;

  std::list<BlockInfo> _left_blocks;
  std::list<BlockInfo> _right_blocks;

  void _ui_init();
  void _ui_process_events(std::stop_token, std::chrono::steady_clock::time_point until);
  void _ui_loop(std::stop_token);
  void _setup_block(BlockInfo &info);

  bar() {}

  ~bar() {
    if (_ui_thread.joinable()) {
      _ui_thread.request_stop();
      schedule_redraw();
      _ui_thread.join();
    }
  }

  HandleMap<std::function<void(XEvent *)>> _x11_event_callbacks;

public:
  static bar &instance() {
    static bar instance;
    return instance;
  }

  ui::gwindow &window() { return _window; }
  ui::gwindow &tooltip_window() { return _tooltip_window; }

  void add_left(Block &block) { _setup_block(_left_blocks.emplace_back(block)); };

  void add_right(Block &block) { _setup_block(_right_blocks.emplace_back(block)); };

  void schedule_redraw() {
    if (!_redraw_requested.exchange(true, std::memory_order_acq_rel))
      glfwPostEmptyEvent();
  }

  void redraw();

  void start_ui() {
    std::latch ui_initialized_latch(1);
    _ui_thread = std::jthread([this, &ui_initialized_latch](std::stop_token st) {
      uv_async_t handle;
      uv_async_init(uv_default_loop(), &handle, [](uv_async_t *) {});

      _ui_init();
      ui_initialized_latch.count_down();
      _ui_loop(st);

      uv_close((uv_handle_t *)&handle, nullptr);
    });
    ui_initialized_latch.wait();
  }

  size_t on_x11_event(std::function<void(XEvent *)> callback) { return _x11_event_callbacks.emplace(callback); }
  void off_x11_event(size_t handle) { _x11_event_callbacks.remove(handle); }

  void join();
};
