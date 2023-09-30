#pragma once

#include <chrono>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <variant>
#include <vector>

#include <fmt/core.h>

#include "../block.hh"
#include "../util.hh"

class ScriptBlock : public Block {
  std::filesystem::path _path;
  std::chrono::duration<double> _interval;
  std::vector<int> _update_signals;
  bool skip_on_empty;

  std::mutex _update_mutex;
  std::timed_mutex _process_mutex;

  // Success is a macro... is this X's fault?
  struct SuccessR {
    std::string output;
  };
  struct TimedOut {};
  struct SpawnFailed {
    int error;
  };
  struct NonZeroExit {
    int status;
  };
  struct Signaled {
    int signal;
  };

  std::variant<SuccessR, TimedOut, SpawnFailed, NonZeroExit, Signaled> _result;

public:
  struct Config {
    std::filesystem::path path;
    std::chrono::duration<double> interval;
    std::vector<int> update_signals{};
    bool skip_on_empty = true;
  };

  ScriptBlock(Config &&config)
      : _path(std::move(config.path)), _interval(std::move(config.interval)),
        _update_signals(std::move(config.update_signals)),
        skip_on_empty(config.skip_on_empty) {
    for (auto signal : _update_signals)
      if (signal > SIGRTMAX || signal < SIGRTMIN)
        throw std::runtime_error(fmt::format(
            "Update signal number out of range! Available range: [{}, {}]",
            SIGRTMIN, SIGRTMAX));
  }

  void late_init() override;

  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;
  bool skip() override {
    return std::holds_alternative<SuccessR>(_result) &&
           std::get<SuccessR>(_result).output.empty();
  }
  void update() override;
  std::chrono::duration<double> update_interval() override { return _interval; }
};
