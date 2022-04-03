#pragma once

#include "../block.hh"

class ClockBlock : public Block {
public:
  size_t draw(Draw&, std::chrono::duration<double> delta) const override;
};
