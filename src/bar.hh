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
#include "events.hh"
#include "guard.hh"
#include "log.hh"
#include "loop.hh"
#include "ui/draw.hh"
#include "ui/ui.hh"
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

  ui::gwindow _window;
  ui::gwindow _tooltip_window;

  // Used to implement tooltip drawing
  BlockInfo *_hovered_block;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> _last_mouse_move;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> _last_tooltip_draw;

  std::vector<BlockInfo> _left_blocks;
  std::vector<BlockInfo> _right_blocks;

  void _init_ui();
  void _setup_block(BlockInfo &info);

  bar() {
    try {
      ui::ui_thread.execute_now([this] { _init_ui(); });
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

  void show() { glfwShowWindow(_window); }

  ui::gwindow &window() { return _window; }
  ui::gwindow &tooltip_window() { return _tooltip_window; }

  void add_left(Block &block) { _setup_block(_left_blocks.emplace_back(block)); };

  void add_right(Block &block) { _setup_block(_right_blocks.emplace_back(block)); };

  void redraw();
};
