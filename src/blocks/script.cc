#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wait.h>

extern char **environ;

#include <fstream>
#include <system_error>

#include "script.hh"

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
      throw std::runtime_error("Script timed out");
    }
  }

  std::ifstream ifs("/proc/self/fd/" + std::to_string(memfd), std::ios::binary);
  if (!ifs)
    throw std::runtime_error("Failed to open memfd with script output");

  ifs.seekg(0, std::ios::end);
  _output.resize(ifs.tellg());
  ifs.seekg(0, std::ios::beg);
  ifs.read(&_output[0], _output.size());
  ifs.close();
}

size_t ScriptBlock::draw(Draw &draw) {
  return draw.text(0, draw.vcenter(), _output.substr(0, _output.find('\n')));
}
