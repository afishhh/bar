#pragma once

#include "../block.hh"

class ClockBlock : public Block {
public:
  size_t draw(Draw &, std::chrono::duration<double> delta) override;

  void animate(EventLoop::duration delta) override {}
  // The animate is only here so that a redraw event is triggered.
  std::optional<EventLoop::duration> animate_interval() override {
    return std::chrono::milliseconds(250);
  }
};
