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
  glfw_throw_error();

  if (glfwGetX11Display()) {
    fmt::println(debug, "X11: Scaling width by {} to work around incorrect reported workarea",
                 config::x11::fractional_scaling_workaround);
    mon_width *= config::x11::fractional_scaling_workaround;
    mon_height *= config::x11::fractional_scaling_workaround;
  }

  _monitor_size = {mon_width, mon_height};

  fmt::println(debug, "Work area for primary monitor: {}x{}", mon_width, mon_height);

  GLFWwindow *window =
      BAR_GLFW_CALL(CreateWindow, mon_width, config::height, config::x11::window_name.data(), NULL, NULL);

  glfwWindowHintString(GLFW_X11_CLASS_NAME, "");
  glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "");

  GLFWwindow *tooltip_window = BAR_GLFW_CALL(CreateWindow, 1, 1, "bar tooltip", NULL, NULL);

  glfwSetWindowPos(window, 0, 0);
  glfw_throw_error();

  if (Window xwindow = glfwGetX11Window(window); xwindow != None) {
    XSetWindowAttributes attr;
    attr.override_redirect = true;
    XChangeWindowAttributes(glfwGetX11Display(), glfwGetX11Window(tooltip_window), CWOverrideRedirect, &attr);
    if (config::x11::override_redirect) {
      XChangeWindowAttributes(glfwGetX11Display(), xwindow, CWOverrideRedirect, &attr);
    }
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

  glfwSetCursorEnterCallback(window, [](GLFWwindow *, int entered) {
    if (entered == GLFW_FALSE)
      bar::instance()._hovered_block_threatened |= 1;
    else
      bar::instance()._hovered_block_threatened |= 2;
  });

  glfwSetCursorEnterCallback(tooltip_window, [](GLFWwindow *, int entered) {
    if (entered == GLFW_FALSE)
      bar::instance()._hovered_block_threatened |= 1;
    else
      bar::instance()._hovered_block_threatened |= 2;
  });

  glfwSetCursorPosCallback(window, [](GLFWwindow *, double x, double y) {
    auto &bar = bar::instance();
    auto block_lists = {std::views::all(bar._right_blocks), std::views::all(bar._left_blocks)};
    auto all_blocks = std::ranges::join_view(block_lists);
    bar._last_mouse_move = std::chrono::steady_clock::now();

    x /= bar._window.drawer().x_render_scale();
    y /= bar._window.drawer().y_render_scale();

    bar._hovered_block = nullptr;
    for (auto &info : all_blocks) {
      if (info.last_pos.x <= x && info.last_pos.y <= y && info.last_pos.x + info.last_size.x > x &&
          info.last_pos.y + info.last_size.y > y) {
        bar._hovered_block = &info;
        break;
      }
    }

    if (bar._hovered_block)
      bar._hovered_block_threatened = 0b100;
    // std::cerr << '(' << x << ", " << y << ") ";
    // if (bar._hovered_block)
    //   std::cerr << typeid(bar._hovered_block->block).name() << '\n';
    // else
    //   std::cerr << "none\n";
  });

  _window = ui::gwindow(window);
  _tooltip_window = ui::gwindow(tooltip_window);

  auto fonts = std::make_shared<ui::fonts>();
  for (auto fname : config::fonts)
    fonts->add(fname, _window.drawer().text_render_scale());

  _window.drawer().texter().set_fonts(std::shared_ptr(fonts));
  _tooltip_window.drawer().texter().set_fonts(std::move(fonts));
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
                   bar::instance().schedule_redraw();
                 });
  if (auto i = info.block.animate_interval())
    EV.add_timer(*i, [&block = info.block](auto delta) {
      block.animate(delta);
      bar::instance().schedule_redraw();
    });
}

void bar::_ui_loop(std::stop_token token) {
  try {
    while (true) {
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
      glfw_throw_error();
      _redraw_requested.store(true, std::memory_order_release);

      _hovered_block_threatened = 0;

      auto wait_end = _last_redraw + 48ms;
      while (true) {
        auto now = std::chrono::steady_clock::now();
        if (token.stop_requested())
          break;
        if (now >= wait_end) {
          if (_redraw_requested.load(std::memory_order_acquire))
            break;
          else
            glfwWaitEvents();
        } else
          glfwWaitEventsTimeout(std::chrono::duration_cast<std::chrono::milliseconds>(wait_end - now).count() / 1000.0);
      }

      if (_hovered_block_threatened == 0b01)
        _hovered_block = nullptr;

      if (_hovered_block_threatened & 0b100)
        _redraw_requested.store(true, std::memory_order_release);
    }

    glfwTerminate();
  } catch (std::exception &e) {
    fmt::print(error, "Exception in UI loop: {}\n", e.what());
    std::exit(1);
  }
}

void bar::redraw() {
  std::size_t x = 5;

  auto &direct_draw = _window.drawer();
  auto buffered_draw = BufDraw(direct_draw);

  glfwMakeContextCurrent(_window);
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

  glFlush();
  glfwSwapBuffers(_window);

  BlockInfo *hovered = _hovered_block;
  if (hovered && hovered->block.has_tooltip()) {
    glfwMakeContextCurrent(_tooltip_window);
    glClear(GL_COLOR_BUFFER_BIT);

    auto now = std::chrono::steady_clock::now();
    auto &block = hovered->block;
    auto &wd = _tooltip_window.drawer();
    auto bd = BufDraw(wd);
    block.draw_tooltip(bd, now - _last_tooltip_draw, hovered->last_size.x);

    auto dim = bd.calculate_size();
    dim.x *= wd.x_render_scale(), dim.y *= wd.y_render_scale();

    uvec2 pos{(uint32_t)(hovered->last_pos.x * wd.x_render_scale()) +
                  (signed)(hovered->last_size.x * wd.x_render_scale() - dim.x - 16) / 2,
              (unsigned)(hovered->last_pos.y * wd.y_render_scale() + config::height)};
    uvec2 size{dim.x + (unsigned)(16 * wd.x_render_scale()), dim.y + (unsigned)(16 * wd.y_render_scale())};
    uvec2 dsize = {(unsigned)_monitor_size.x, (unsigned)_monitor_size.y};

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
    glFlush();
    glfwSwapBuffers(_tooltip_window);

    glfwShowWindow(_tooltip_window);
  } else
    glfwHideWindow(_tooltip_window);
}
