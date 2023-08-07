#pragma once

#include <array>
#include <chrono>
#include <string>

#include "../block.hh"

class ClockBlock : public Block {
  struct tm *_time;

public:
  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;

  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(250);
  }

  bool has_tooltip() const override { return true; }
  void draw_tooltip(ui::draw &, std::chrono::duration<double>,
                    unsigned int) const override;
};
