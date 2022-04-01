#pragma once

#include "../block.hh"

class FpsBlock : public Block {
public:
  FpsBlock() {}

  size_t draw(Draw& draw) {
    return draw.text(0, draw.vcenter(), "FPS: " + std::to_string(draw.fps()));
  }
};
