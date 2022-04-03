#pragma once

#include <chrono>
#include <cstddef>

#include "../block.hh"

class NetworkBlock : public Block {
  size_t _last_tx_bytes = 0, _last_rx_bytes = 0;
  size_t _tx_bytes, _rx_bytes;

public:
  size_t draw(Draw &, std::chrono::duration<double> delta) const override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    // NOTE: The unit in draw() should albo be changed if this ever changes.
    return std::chrono::milliseconds(500);
  }
};
