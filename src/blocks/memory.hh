#pragma once

#include <chrono>
#include <cstddef>
#include <fstream>
#include <string>

#include "../block.hh"

class MemoryBlock : public Block {
  size_t _total;
  size_t _used;
  size_t _avail;

public:
  struct Config {
    std::string prefix;
    Draw::color_t prefix_color = 0xFFFFFF;
  };

private:
  Config _config;

public:
  MemoryBlock(const Config &config);

  size_t draw(Draw &, std::chrono::duration<double> delta) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(500);
  }
};
