#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>

#include "../block.hh"

class ScriptBlock : public Block {
  std::filesystem::path _path;
  std::chrono::duration<double> _interval;
  std::optional<size_t> _limit;

  std::string _output;

public:
  ScriptBlock(const std::filesystem::path &path,
              const std::chrono::duration<double> &interval)
      : _path(path), _interval(interval) {}
  ScriptBlock(const std::filesystem::path &path,
              const std::chrono::duration<double> &interval, size_t limit)
      : _path(path), _interval(interval), _limit(limit) {}

  size_t draw(Draw &) override;
  void update() override;
  std::chrono::duration<double> update_interval() override { return _interval; }
};
