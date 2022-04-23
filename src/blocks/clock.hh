#pragma once

#include <array>
#include <string>

#include "../block.hh"

class ClockBlock : public Block {
  std::array<char, 80> _text;

public:
  size_t draw(Draw &, std::chrono::duration<double> delta) override;

  void animate(EventLoop::duration) override;
  // The animate is only here so that a redraw event is triggered.
  std::optional<EventLoop::duration> animate_interval() override {
    return std::chrono::milliseconds(250);
  }
};
