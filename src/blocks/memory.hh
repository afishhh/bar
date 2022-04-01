#pragma once

#include <chrono>
#include <cstddef>
#include <fstream>

#include "../block.hh"

class MemoryBlock : public Block {
  size_t _total;
  size_t _used;
  size_t _free;

public:
  size_t draw(Draw &) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(500);
  }
};
