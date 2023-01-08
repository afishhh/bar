#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
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
    std::optional<std::string> title{};
    bool show_fs_type;
    bool show_usage_text;
    bool usage_text_in_bar;
    bool show_usage_bar;
    ui::draw::pos_t bar_width;
    std::optional<color> bar_fill_color{};
  };

private:
  Config _config;

public:
  DiskBlock(const std::filesystem::path &mountpoint, Config config);
  ~DiskBlock();

  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::seconds(3);
  }
};
