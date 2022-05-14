#include <csignal>
#include <cstddef>
#include <fcntl.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wait.h>

extern char **environ;

#include <fstream>
#include <system_error>
#include <thread>
#include <unordered_map>

#include "../events.hh"
#include "../signal.hh"
#include "../util.hh"
#include "script.hh"

void ScriptBlock::late_init() {
  if (_update_signal) {
    SignalEvent::attach(*_update_signal);

    EV.on<SignalEvent>([this](const SignalEvent &ev) {
      if (ev.signum != _update_signal)
        return;

      update();
      EV.fire_event(RedrawEvent());
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

      if (posix_spawnp(&pid, this->_path.c_str(), &actions, &attr, argv,
                       environ) < 0)
        throw std::system_error(errno, std::system_category(), "posix_spawnp");
    }
    EventLoop::callback_id sigchld_handler = EV.on<SignalEvent>(
        [this, pid, &sigchld_handler](const SignalEvent &ev) {
          if (ev.signum != SIGCHLD || ev.info.si_pid != pid)
            return;

          _process_mutex.unlock();
          EV.off<SignalEvent>(sigchld_handler);
        });

    if (posix_spawn_file_actions_destroy(&actions) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawn_file_actions_destroy");
    if (posix_spawnattr_destroy(&attr) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawnattr_destroy");

    if (!_process_mutex.try_lock_for(_interval -
                                     std::chrono::milliseconds(30))) {
      EV.off<SignalEvent>(sigchld_handler);
      kill(pid, SIGKILL);
      _timed_out = true;
    }

    // Get rid of the zombie process.
    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR)
      ;

    _timed_out = false;

    // If we got the lock that means the process finished before the timeout
    // and we can unlock the mutex safely.
    _process_mutex.unlock();

    std::ifstream ifs("/proc/self/fd/" + std::to_string(memfd),
                      std::ios::binary);
    if (!ifs)
      throw std::runtime_error("Failed to open memfd with script output");

    ifs.seekg(0, std::ios::end);
    _output.resize(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    ifs.read(&_output[0], _output.size());
    ifs.close();

    _output = std::string(trim(_output));

    if (close(memfd) < 0)
      throw std::system_error(errno, std::system_category(), "close");

    _update_mutex.unlock();
  });
  update_thread.detach();
}

size_t ScriptBlock::draw(Draw &draw, std::chrono::duration<double>) {
  if (_timed_out)
    return draw.text(0, draw.vcenter(), "TIMED OUT", 0xFF0000);

  return draw.text(0, draw.vcenter(), _output);
}
