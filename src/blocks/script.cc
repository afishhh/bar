#include <cstddef>
#include <fcntl.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <wait.h>

extern char **environ;

#include <csignal>
#include <fstream>
#include <system_error>
#include <thread>

#include "../util.hh"
#include "script.hh"

std::unordered_map<int, ScriptBlock *> signal_block_update_map;
void ScriptBlock::handle_update_signal(int sig) {
  if (signal_block_update_map.contains(sig)) {
    ScriptBlock *block{signal_block_update_map[sig]};
    block->update();
  }
}

bool sigchld_handler_init{false};
std::mutex process_map_mutex;
std::unordered_map<pid_t, ScriptBlock *> process_block_map;
void ScriptBlock::handle_sigchld_signal(int, siginfo_t *info, void *) {
  std::lock_guard lock{process_map_mutex};
  if (process_block_map.contains(info->si_pid)) {
    ScriptBlock *block{process_block_map[info->si_pid]};
    block->_process_mutex.unlock();
    process_block_map.erase(info->si_pid);
  }
}

void ScriptBlock::late_init() {
  if (_update_signal) {
    signal_block_update_map[*_update_signal] = this;

    struct sigaction sa;
    sa.sa_handler = &handle_update_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(*_update_signal, &sa, nullptr);
  }
  if (!sigchld_handler_init) {
    struct sigaction sa;
    sa.sa_sigaction = &handle_sigchld_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);
    sigchld_handler_init = true;
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

      std::lock_guard lock{process_map_mutex};
      if (posix_spawnp(&pid, this->_path.c_str(), &actions, &attr, argv,
                       environ) < 0)
        throw std::system_error(errno, std::system_category(), "posix_spawnp");
      process_block_map[pid] = this;
    }

    if (posix_spawn_file_actions_destroy(&actions) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawn_file_actions_destroy");
    if (posix_spawnattr_destroy(&attr) < 0)
      throw std::system_error(errno, std::system_category(),
                              "posix_spawnattr_destroy");

    auto end = std::chrono::steady_clock::now() + _interval -
               std::chrono::milliseconds(25);
    if (!_process_mutex.try_lock_for(_interval -
                                     std::chrono::milliseconds(30))) {
      kill(pid, SIGKILL);
      _timed_out = true;
    }
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

size_t ScriptBlock::draw(Draw &draw, std::chrono::duration<double> delta) {
  if (_timed_out)
    return draw.text(0, draw.vcenter(), "TIMED OUT", 0xFF0000);

  return draw.text(0, draw.vcenter(), _output);
}
