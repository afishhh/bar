#pragma once

#include "../block.hh"

class ClockBlock : public Block {
public:
  size_t draw(Draw&) override;
};
