#pragma once

#include <atomic>
#include <barrier>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <uv.h>
#include <variant>
#include <vector>

#include <fmt/core.h>

#include "../block.hh"
#include "../util.hh"

class ScriptBlock : public SimpleBlock {
  std::filesystem::path _path;
  Interval _interval;
  std::list<uv_signal_t> _signal_handles;
  std::vector<std::string> _extra_environment_variables;
  bool _inherit_environment_variables;

  std::barrier<std::function<void()>> _close_synchronizer;
  std::atomic_flag _is_updating;
  uv_pipe_t _child_output_stream{};
  size_t _child_output_buffer_real_size;
  std::string _child_output_buffer;
  uv_process_t _child_process{};

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

  std::mutex _result_mutex;
  std::variant<SuccessR, TimedOut, SpawnFailed, NonZeroExit, Signaled> _result;

public:
  struct Config {
    std::filesystem::path path;
    Interval interval;
    std::vector<int> update_signals{};
    std::unordered_map<std::string, std::string> extra_environment_variables{};
    bool inherit_environment_variables = true;
  };

  ScriptBlock(Config &&config)
      : _path(std::move(config.path)), _interval(std::move(config.interval)),
        _inherit_environment_variables(config.inherit_environment_variables),
        _close_synchronizer(2, [this] { _is_updating.clear(std::memory_order::release); }) {
    setup_signals(config.update_signals);

    for (auto const &[name, value] : config.extra_environment_variables) {
      if (name.find('=') != std::string::npos)
        throw std::runtime_error("Environment variable name cannot contain an '='");
      _extra_environment_variables.emplace_back(fmt::format("{}={}", name, value));
    }
  }

  void setup_signals(std::vector<int> const &signals);

  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;
  bool skip() override {
    return std::holds_alternative<SuccessR>(_result) && std::get<SuccessR>(_result).output.empty();
  }
  void update() override;
  Interval update_interval() override { return _interval; }
};
