#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

#include <sys/vfs.h>

#include "../block.hh"

class DiskBlock : public Block {
  struct statfs _statfs;

  std::filesystem::path _mountpoint;
public:
  struct Config {
    bool show_fs_type;
    bool show_usage_text;
    bool show_usage_bar;
    size_t bar_width;
    std::optional<Draw::color_type> bar_fill_color;
  };
private:
  Config _config;

public:
  DiskBlock(const std::filesystem::path &mountpoint, Config config);
  ~DiskBlock();

  size_t draw(Draw &, std::chrono::duration<double> delta) const override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::seconds(3);
  }
};
