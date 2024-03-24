#define GLFW_EXPOSE_NATIVE_X11

#include "ui/gl.hh"

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
#include "guard.hh"
#include "log.hh"
#include "loop.hh"
#include "ui/draw.hh"
#include "ui/window.hh"
#include "util.hh"

class bar {
  struct BlockInfo {
    Block &block;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> last_update;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> last_draw;

    uvec2 last_pos;
    uvec2 last_size;

    BlockInfo(Block &block) : block(block) {}
  };

  std::jthread _ui_thread;
  std::atomic<bool> _redraw_requested;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> _last_redraw;

  ui::gwindow _window;
  ui::gwindow _tooltip_window;

  // Used to implement tooltip drawing
  ivec2 _monitor_size;
  BlockInfo *_hovered_block;
  int _hovered_block_threatened;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> _last_mouse_move;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> _last_tooltip_draw;

  std::vector<BlockInfo> _left_blocks;
  std::vector<BlockInfo> _right_blocks;

  void _ui_init();
  void _ui_loop(std::stop_token);
  void _setup_block(BlockInfo &info);

  bar() {
    std::latch ui_initialized_latch(1);
    // TODO: Wait for the ui thread to open windows
    _ui_thread = std::jthread([this, &ui_initialized_latch](std::stop_token st) {
      _ui_init();
      ui_initialized_latch.count_down();
      _ui_loop(st);
    });
    ui_initialized_latch.wait();
  }

  ~bar() {
    _ui_thread.request_stop();
    schedule_redraw();
  }

public:
  static bar &instance() {
    static bar instance;
    return instance;
  }

  void show() { glfwShowWindow(_window); }

  ui::gwindow &window() { return _window; }
  ui::gwindow &tooltip_window() { return _tooltip_window; }

  void add_left(Block &block) { _setup_block(_left_blocks.emplace_back(block)); };

  void add_right(Block &block) { _setup_block(_right_blocks.emplace_back(block)); };

  void schedule_redraw() {
    if(!_redraw_requested.exchange(true, std::memory_order_acq_rel))
      glfwPostEmptyEvent();
  }

  void redraw();
};
