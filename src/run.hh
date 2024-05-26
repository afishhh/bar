#pragma once

#include <barrier>
#include <functional>
#include <string>
#include <uv.h>
#include <vector>

using run_callback = std::function<void(int status, int signal, std::string output)>;

// nuv = Not libUV
class nuv_process {
  size_t length{0};
  uv_pipe_t pipe{};
  std::barrier<std::function<void()>> close_synchronizer;
  int status, signal;
  run_callback callback;

  friend void run(nuv_process *, std::vector<std::string> argv, std::vector<char *> env, run_callback);

public:
  std::string buffer;
  uv_process_t process{};

  nuv_process() : close_synchronizer(3, [this] { this->callback(status, signal, buffer); }) {}
};

void run(nuv_process *, std::vector<std::string> argv, std::vector<char *> env, run_callback);
