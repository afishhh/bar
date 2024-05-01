#include <charconv>
#include <csignal>
#include <cstddef>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <uv.h>
#include <vector>
#include <wait.h>

extern char **environ;

#include "../bar.hh"
#include "../log.hh"
#include "../util.hh"
#include "script.hh"

void ScriptBlock::setup_signals(std::vector<int> const &signals) {
  for (auto signal : signals) {
    if (signal > SIGRTMAX || signal < SIGRTMIN)
      throw std::runtime_error(
          fmt::format("Update signal number out of range! Available range: [{}, {}]", SIGRTMIN, SIGRTMAX));

    _signal_handles.emplace_back();
    uv_signal_t *handle = &_signal_handles.back();
    uv_signal_init(uv_default_loop(), handle);
    handle->data = this;

    uv_signal_start(
        handle,
        [](uv_signal_t *handle, int) {
          auto *self = (ScriptBlock *)handle->data;
          self->update();
          bar::instance().schedule_redraw();
          // FIXME: Why does this cause a segmentation fault??
          // uv_timer_again(&self->_update_timer);
        },
        signal);
    uv_unref((uv_handle_t *)handle);
  }
}

void ScriptBlock::update() {
  if (_is_updating.test_and_set(std::memory_order::acq_rel)) {
    debug << "Update skipped: already running\n";
    return;
  }

  uv_pipe_init(uv_default_loop(), &_child_output_stream, false);
  _child_output_buffer_real_size = 0;
  _child_output_stream.data = this;
  _child_process.data = this;
  uv_process_options_t options{};
  uv_stdio_container_t child_stdio[2];
  child_stdio[0].flags = UV_IGNORE;
  child_stdio[1].flags = (uv_stdio_flags)(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
  child_stdio[1].data.stream = (uv_stream_t *)&_child_output_stream;
  options.stdio = child_stdio;
  options.stdio_count = 2;
  char *args[2];
  args[0] = strdup(_path.c_str());
  args[1] = NULL;
  DEFER([args] { free(args[0]); });
  options.args = args;
  options.file = _path.c_str();
  options.exit_cb = [](uv_process_t *process, int64_t status, int signal) {
    auto *self = (ScriptBlock *)process->data;

    uv_close((uv_handle_t *)process,
             [](uv_handle_t *handle) { (void)((ScriptBlock *)handle->data)->_close_synchronizer.arrive(); });
    uv_close((uv_handle_t *)&self->_child_output_stream,
             [](uv_handle_t *handle) { (void)((ScriptBlock *)handle->data)->_close_synchronizer.arrive(); });

    {
      std::unique_lock lg(self->_result_mutex);
      if (signal)
        self->_result = Signaled{signal};
      else if (status)
        self->_result = NonZeroExit{(int)status};
      else {
        while (self->_child_output_buffer_real_size > 0 &&
               std::isspace(self->_child_output_buffer[self->_child_output_buffer_real_size - 1]))
          self->_child_output_buffer_real_size -= 1;
        self->_child_output_buffer.resize(self->_child_output_buffer_real_size);
        self->_result = SuccessR{self->_child_output_buffer};
      }
    }
  };

  std::vector<char *> env;
  if (_inherit_environment_variables)
    for (char **cur = environ; *cur; ++cur)
      env.push_back(*cur);

  for (auto &var : _extra_environment_variables)
    env.push_back(var.data());

  env.push_back(nullptr);

  options.env = env.data();

  if (int err = uv_spawn(uv_default_loop(), &_child_process, &options); err < 0) {
    _is_updating.clear(std::memory_order::release);
    throw std::runtime_error("libuv spawn failed: " + std::string(uv_strerror(err)));
  }

  uv_read_start((uv_stream_t *)&_child_output_stream,
                [](uv_handle_t *handle, size_t size, uv_buf_t *buf) {
                  auto *self = (ScriptBlock *)handle->data;
                  self->_child_output_buffer.resize(self->_child_output_buffer_real_size + size);
                  buf->base = self->_child_output_buffer.data() + self->_child_output_buffer_real_size;
                  buf->len = size;
                },
                [](uv_stream_t *stream, ssize_t nread, uv_buf_t const *) {
                  auto *self = (ScriptBlock *)stream->data;

                  if (nread == UV_EOF)
                    return;
                  else if (nread < 0)
                    error << "Reading child pipe failed!\n";
                  else {
                    self->_child_output_buffer_real_size += nread;
                  }
                });
}

ui::draw::pos_t draw_text_with_ansi_color(ui::draw::pos_t x, ui::draw::pos_t const y, ui::draw &draw,
                                          std::string_view text) {
  auto it = text.begin();
#define advance_or_bail()                                                                                              \
  do {                                                                                                                 \
    if (++it == text.end())                                                                                            \
      goto bail;                                                                                                       \
  } while (false)

  color current_color = 0xFFFFFF;
  auto segment_start = text.begin();

  auto flush = [&] {
    x += draw.text(x, y, std::string_view(segment_start, it), current_color);
    segment_start = it;
  };

  while (it != text.end()) {
    if (*it == '\x1b') {
      flush();

      advance_or_bail();
      if (*it == '[') {
        std::vector<unsigned> modifiers;

        while (true) {
          advance_or_bail();
          auto number_start = it;
          while (std::isdigit(*it))
            advance_or_bail();
          std::string_view number_view(number_start, it);

          modifiers.push_back(0);
          auto [_, ec] = std::from_chars(number_start, it, modifiers.back());
          // Invalid ANSI escape
          if (ec != std::errc()) {
            modifiers.clear();
            break;
          }

          if (*it == ';')
            continue;
          // End of valid ANSI escape
          else if (*it == 'm')
            break;
          // Invalid ANSI escape
          else {
            modifiers.clear();
            break;
          }
        }

        segment_start = ++it;

        // clang-format off
        constexpr std::array<color, 9> colors8{
          0x000000, 0xFF7777, 0x77FF77,
          0xFFF93F, 0x7777FF, 0xC300FF,
          0x00FFEA, 0xFFFFFF, 0xFFFFFF
        };
        // clang-format on
        for (auto mod : modifiers) {
          if (mod >= 30 && mod <= 39)
            current_color = colors8[mod - 30];
          if (mod == 0)
            current_color = colors8.back();
        }
      }
    } else
      ++it;
  }

  flush();

bail:;
  return x;
}

size_t ScriptBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  std::unique_lock lock(_result_mutex);
  return std::visit(
      overloaded{[&draw](SuccessR const &s) { return draw_text_with_ansi_color(0, draw.vcenter(), draw, s.output); },
                 [&draw](TimedOut) { return draw.text(0, draw.vcenter(), "TIMED OUT", 0xFF0000); },
                 [&draw](SpawnFailed sf) { return draw.text(0, draw.vcenter(), std::strerror(sf.error), 0xFF0000); },
                 [&draw](NonZeroExit nze) {
                   return draw.text(0, draw.vcenter(), fmt::format("EXITED WITH {}", nze.status), 0xFF0000);
                 },
                 [&draw](Signaled s) {
                   char const *name = strsignal(s.signal);
                   if (name == nullptr)
                     return draw.text(0, draw.vcenter(), fmt::format("SIGNALED WITH {}", s.signal), 0xFF0000);
                   else
                     return draw.text(0, draw.vcenter(), fmt::format("{}", name), 0xFF0000);
                 }},
      _result);
}
