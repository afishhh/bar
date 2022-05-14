#pragma once

#include <chrono>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>

#include "../block.hh"
#include "../format.hh"
#include "../util.hh"

class ScriptBlock : public Block {
  std::filesystem::path _path;
  std::chrono::duration<double> _interval;
  std::optional<int> _update_signal;

  std::mutex _update_mutex;
  std::timed_mutex _process_mutex;

  std::string _output;
  bool _timed_out = false;

public:
  ScriptBlock(const std::filesystem::path &path,
              const std::chrono::duration<double> &interval)
      : _path(path), _interval(interval) {}
  ScriptBlock(const std::filesystem::path &path,
              const std::chrono::duration<double> &interval, int update_signal)
      : _path(path), _interval(interval), _update_signal(update_signal) {
    if (*_update_signal > SIGRTMAX || _update_signal < SIGRTMIN)
      throw std::runtime_error(std::format(
          "Update signal number out of range! Available range: [{}, {}]",
          SIGRTMIN, SIGRTMAX));
  }

  void late_init() override;

  size_t draw(Draw &, std::chrono::duration<double> delta) override;
  void update() override;
  std::chrono::duration<double> update_interval() override { return _interval; }
};
