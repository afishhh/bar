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

void ScriptBlock::late_init() {
  if (_update_signal) {
    signal_block_update_map[*_update_signal] = this;

    struct sigaction sa;
    sa.sa_handler = &handle_update_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(*_update_signal, &sa, nullptr);
  }
}

void ScriptBlock::update() {
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

  std::string path_str = _path.string();
  char *const argv[] = {path_str.data(), nullptr};
  if (posix_spawnp(&pid, this->_path.c_str(), &actions, &attr, argv, environ) <
      0)
    throw std::system_error(errno, std::system_category(), "posix_spawnp");

  if (posix_spawn_file_actions_destroy(&actions) < 0)
    throw std::system_error(errno, std::system_category(),
                            "posix_spawn_file_actions_destroy");
  if (posix_spawnattr_destroy(&attr) < 0)
    throw std::system_error(errno, std::system_category(),
                            "posix_spawnattr_destroy");

  auto end = std::chrono::steady_clock::now() + _interval -
             std::chrono::milliseconds(25);
  int ret;
  while ((ret = waitpid(pid, nullptr, WNOHANG)) != pid) {
    if (ret != 0 && errno != EINTR)
      throw std::system_error(errno, std::system_category(), "waitpid");

    if (std::chrono::steady_clock::now() > end) {
      kill(pid, SIGKILL);
      _timed_out = true;
    }

    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
  }
  _timed_out = false;

  std::ifstream ifs("/proc/self/fd/" + std::to_string(memfd), std::ios::binary);
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
}

size_t ScriptBlock::draw(Draw &draw, std::chrono::duration<double> delta) {
  if (_timed_out)
    return draw.text(0, draw.vcenter(), "TIMED OUT", 0xFF0000);

  return draw.text(0, draw.vcenter(), _output);
}
