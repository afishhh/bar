#include "bar.hh"
#include "config.hh"

void bar::_ui_init() {
  glfwInit();

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
  glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  glfwWindowHintString(GLFW_X11_CLASS_NAME, config::x11::window_class.data());
  glfwWindowHintString(GLFW_X11_INSTANCE_NAME, config::x11::window_class.data());

  GLFWmonitor *mon = glfwGetPrimaryMonitor();
  int mon_width, mon_height;
  glfwGetMonitorWorkarea(mon, NULL, NULL, &mon_width, &mon_height);

  if (glfwGetX11Display()) {
    fmt::println(debug, "X11: Scaling width by {} to work around incorrect reported workarea",
                 config::x11::fractional_scaling_workaround);
    mon_width *= config::x11::fractional_scaling_workaround;
  }

  fmt::println(debug, "Work area for primary monitor: {}x{}", mon_width, mon_height);

  GLFWwindow *window = glfwCreateWindow(mon_width, config::height, config::x11::window_name.data(), NULL, NULL);
  if (!window) {
    fmt::print(error, "Failed to create a window!\n");
    char const *description;
    glfwGetError(&description);
    fmt::print(error, "{}\n", description);
  }

  glfwSetWindowPos(window, 0, 0);

  if (Window xwindow = glfwGetX11Window(window); xwindow != None)
    if (config::x11::override_redirect) {
      XSetWindowAttributes attr;
      attr.override_redirect = true;
      XChangeWindowAttributes(glfwGetX11Display(), xwindow, CWOverrideRedirect, &attr);
    }

  glfwMakeContextCurrent(window);
  int version = gladLoadGL(glfwGetProcAddress);
  fmt::print(info, "OpenGL version: {}.{}\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

  // if (auto conn_opt = ui::x11::connection::try_create(); conn_opt) {
  // _connection = std::unique_ptr(std::move(*conn_opt));

  // XSetTransientForHint(x11conn->display(), x11tooltipwin->window_id(), x11mainwin->window_id());
  // XSelectInput(x11conn->display(), x11conn->root(), PointerMotionMask);
  // XSelectInput(x11conn->display(), x11mainwin->window_id(), LeaveWindowMask | EnterWindowMask);
  // XSelectInput(x11conn->display(), x11tooltipwin->window_id(), LeaveWindowMask | EnterWindowMask);
  // EV.on<ui::x11::xevent>([this](ui::x11::xevent const &ev) {
  //   XEvent const &e = ev;
  //   if (e.type == MotionNotify) {
  //     auto block_lists = {std::views::all(_right_blocks), std::views::all(_left_blocks)};
  //     auto all_blocks = std::ranges::join_view(block_lists);
  //     unsigned x = e.xmotion.x, y = e.xmotion.y;
  //     _last_mouse_move = std::chrono::steady_clock::now();
  //
  //     _hovered_block = nullptr;
  //     for (auto &info : all_blocks) {
  //       if (info.last_pos.x <= x && info.last_pos.y <= y && info.last_pos.x + info.last_size.x > x &&
  //           info.last_pos.y + info.last_size.y > y) {
  //         _hovered_block = &info;
  //         break;
  //       }
  //     }
  //   } else if (e.type == LeaveNotify) {
  //     // TODO: Allow mousing over the tooltip window
  //     _hovered_block = nullptr;
  //   }
  // });
  // } else {
  //   fmt::print(error, "No suitable window backend found!\n");
  //   std::exit(1);
  // }

  _window = ui::gwindow(window);

  auto fonts = std::make_shared<ui::fonts>();
  for (auto fname : config::fonts)
    fonts->add(fname, _window.drawer().get_text_render_scale());

  _window.drawer().texter().set_fonts(std::move(fonts));
}

void bar::_setup_block(BlockInfo &info) {
  info.block.late_init();

  info.last_update = std::chrono::steady_clock::now();
  info.block.update();

  info.last_draw = std::chrono::steady_clock::now();

  if (info.block.update_interval() != std::chrono::duration<double>::max())
    EV.add_timer(std::chrono::duration_cast<EventLoop::duration>(info.block.update_interval()),
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

void bar::_ui_loop(std::stop_token token) {
  try {
    while (true) {
      _redraw_requested.wait(false, std::memory_order_acquire);
      if (token.stop_requested())
        break;

      {
        auto start = std::chrono::steady_clock::now();
        // fmt::println(debug, "Redrawing! ({:>6.3f}ms elapsed since last redraw)",
        //              (double)std::chrono::duration_cast<std::chrono::microseconds>(start - _last_redraw).count() /
        //                  1000);
        _last_redraw = start;
      }

      redraw();

      _redraw_requested.store(true, std::memory_order_release);
      std::this_thread::sleep_until(_last_redraw + 100ms);
    }

    glfwTerminate();
  } catch (std::exception &e) {
    fmt::print(error, "Failed to initialize window backend: {}\n", e.what());
    std::exit(1);
  }
}

void bar::redraw() {
  std::size_t x = 5;

  auto &direct_draw = _window.drawer();
  auto buffered_draw = BufDraw(direct_draw);

  glClear(GL_COLOR_BUFFER_BIT);

  {
    auto filtered = _left_blocks | std::views::filter([](BlockInfo const &info) { return !info.block.skip(); });
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
    auto filtered = _right_blocks | std::views::filter([](BlockInfo const &info) { return !info.block.skip(); });
    auto it = filtered.begin();
    if (it != filtered.end())
      while (true) {
        auto &info = *it;
        auto &block = info.block;
        auto now = std::chrono::steady_clock::now();
        auto width = block.draw(buffered_draw, now - info.last_draw);
        width += block.ddraw(buffered_draw, now - info.last_draw, x - width, true);
        info.last_pos = {(unsigned)(x - width), 0};
        info.last_size = {(unsigned)width, config::height};
        info.last_draw = now;

        x -= width;
        direct_draw.frect(x - 8, 0, width + 16, config::height, config::background_color.as_rgb());
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

  /*
  BlockInfo *hovered = _hovered_block;
  if (hovered && hovered->block.has_tooltip()) {
    auto now = std::chrono::steady_clock::now();
    auto &block = hovered->block;
    auto bd = BufDraw(_tooltip_window.drawer());
    block.draw_tooltip(bd, now - _last_tooltip_draw, hovered->last_size.x);

    auto dim = bd.calculate_size();

    uvec2 pos{hovered->last_pos.x + (signed)(hovered->last_size.x - dim.x - 16) / 2,
              (unsigned)(hovered->last_pos.y + config::height)};
    uvec2 size{dim.x + 16, dim.y + 16};
    auto dsize = _connection->available_size();

    if (pos.x + size.x > dsize.x)
      pos.x = dsize.x - size.x;
    // If pos.x overflows then it will surely be bigger than dsize.x and
    // if something else lead to it being bigger then we won't be able to
    // put it outside the screen anyway
    if (pos.x > dsize.x)
      pos.x = 0;

    glfwSetWindowPos(_tooltip_window, pos.x, pos.y);
    glfwSetWindowSize(_tooltip_window, size.x, size.y);
    _tooltip_window.drawer().hrect(0, 0, size.x - 1, size.y - 1, color(0xFFAA00));
    bd.draw_offset(8, 8);

    _last_tooltip_draw = now;
    glfwShowWindow(_tooltip_window);
    glfwSwapBuffers(_tooltip_window);
  } else
    glfwHideWindow(_tooltip_window);
  */

  glFlush();
  glfwSwapBuffers(_window);
}
