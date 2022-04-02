#pragma once

#include <chrono>
#include <cstddef>

#include "draw.hh"

class Block {
public:
  virtual ~Block() {}

  virtual size_t draw(Draw &) = 0;
  virtual void update(){};
  virtual std::chrono::duration<double> update_interval() {
    return std::chrono::duration<double>::max();
  };
};
