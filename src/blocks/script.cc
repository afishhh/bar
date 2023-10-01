#include <charconv>
#include <csignal>
#include <cstddef>
#include <fcntl.h>
#include <fstream>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <wait.h>

extern char **environ;

#include "../events.hh"
#include "../signal.hh"
#include "../util.hh"
#include "script.hh"

void ScriptBlock::late_init() {
  for (auto signal : _update_signals) {
    SignalEvent::attach(signal);
  }

  if (!_update_signals.empty()) {
    EV.on<SignalEvent>(
        [this, signals = std::unordered_set<int>(_update_signals.begin(),
                                                 _update_signals.end())](
            const SignalEvent &ev) {
          if (signals.contains(ev.signum)) {
            update();
            EV.fire_event(RedrawEvent());
          }
        });
  }

  static bool sigchld_registered = false;
  if (!sigchld_registered) {
    SignalEvent::attach(SIGCHLD, SA_NOCLDSTOP);
    sigchld_registered = true;
  }
}

void ScriptBlock::update() {
  if (!_update_mutex.try_lock())
    return;

  std::thread update_thread([&] {
    DEFER([this] { _update_mutex.unlock(); });

    if (!_process_mutex.try_lock())
      throw std::runtime_error(
          "ScriptBlock::update() called while a process is still running");

    // Execute script from this->_path and put it's stdout into this->_output
    pid_t pid;
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawn_file_actions_init");

    int memfd = memfd_create("script_output", 0);
    if (memfd < 0)
      throw std::system_error(errno, std::system_category(), "memfd_create");

    DEFER([memfd] {
      if (close(memfd) < 0)
        throw std::system_error(errno, std::system_category(), "close");
    });

    if (posix_spawn_file_actions_adddup2(&actions, memfd, STDOUT_FILENO) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawn_file_actions_adddup2");

    posix_spawnattr_t attr;
    if (posix_spawnattr_init(&attr) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawnattr_init");

    {
      std::string path_str = _path.string();
      char *const argv[] = {path_str.data(), nullptr};

      if (int err = posix_spawnp(&pid, this->_path.c_str(), &actions, &attr,
                                 argv, environ);
          err != 0) {
        _result = SpawnFailed{err};
        _process_mutex.unlock();
        return;
      }
    }

    // TODO: Cleaner solution
    EventLoop::callback_id sigchld_handler;
    sigchld_handler = EV.on<SignalEvent>([this, pid](const SignalEvent &ev) {
      if (ev.signum != SIGCHLD || ev.info.si_pid != pid)
        return;

      _process_mutex.unlock();
    });

#define CHECK_WSTATUS(wstatus)                                                 \
  {                                                                            \
    if (WIFEXITED(wstatus)) {                                                  \
      auto status = WEXITSTATUS(wstatus);                                      \
      if (status) {                                                            \
        _result = NonZeroExit{status};                                         \
        return;                                                                \
      }                                                                        \
    } else if (WIFSIGNALED(wstatus)) {                                         \
      _result = Signaled{WTERMSIG(wstatus)};                                   \
      return;                                                                  \
    }                                                                          \
  }

    if (posix_spawn_file_actions_destroy(&actions) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawn_file_actions_destroy");
    if (posix_spawnattr_destroy(&attr) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawnattr_destroy");

    if (!_process_mutex.try_lock_for(_interval -
                                     std::chrono::milliseconds(30))) {
      EV.off<SignalEvent>(sigchld_handler);
      // not safe, check for lock with try lock first!
      _process_mutex.unlock();

      // HACK:
      int wstatus;
      if (waitpid(pid, &wstatus, WNOHANG) == pid)
        CHECK_WSTATUS(wstatus);

      kill(pid, SIGKILL);
      _result = TimedOut{};
      return;
    }

    int wstatus;
    // Get rid of the zombie process.
    while (waitpid(pid, &wstatus, 0) < 0 && errno == EINTR)
      ;

    // If we got the lock that means the process finished before the timeout
    // and we can unlock the mutex safely.
    _process_mutex.unlock();

    CHECK_WSTATUS(wstatus);

    std::ifstream ifs("/proc/self/fd/" + std::to_string(memfd),
                      std::ios::binary);
    if (!ifs)
      throw std::runtime_error("Failed to open memfd with script output");

    std::string output;

    ifs.seekg(0, std::ios::end);
    output.resize(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    ifs.read(&output[0], output.size());
    ifs.close();

    _result = SuccessR{std::string(trim(output))};
  });
  update_thread.detach();
}

ui::draw::pos_t draw_text_with_ansi_color(ui::draw::pos_t x,
                                          ui::draw::pos_t const y,
                                          ui::draw &draw,
                                          std::string_view text) {
  auto it = text.begin();
#define advance_or_bail()                                                      \
  do {                                                                         \
    if (++it == text.end())                                                    \
      goto bail;                                                               \
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
  return std::visit(
      overloaded{[&draw](SuccessR const &s) {
                   return draw_text_with_ansi_color(0, draw.vcenter(), draw,
                                                    s.output);
                 },
                 [&draw](TimedOut) {
                   return draw.text(0, draw.vcenter(), "TIMED OUT", 0xFF0000);
                 },
                 [&draw](SpawnFailed sf) {
                   return draw.text(0, draw.vcenter(), std::strerror(sf.error),
                                    0xFF0000);
                 },
                 [&draw](NonZeroExit nze) {
                   return draw.text(0, draw.vcenter(),
                                    fmt::format("EXITED WITH {}", nze.status),
                                    0xFF0000);
                 },
                 [&draw](Signaled s) {
                   char const *name = strsignal(s.signal);
                   if (name == nullptr)
                     return draw.text(0, draw.vcenter(),
                                      fmt::format("SIGNALED WITH {}", s.signal),
                                      0xFF0000);
                   else
                     return draw.text(0, draw.vcenter(),
                                      fmt::format("{}", name), 0xFF0000);
                 }},
      _result);
}
