#pragma once

#include <cstdint>
#include <istream>
#include <ostream>

struct uvec2 {
  std::uint32_t x, y;

  bool is_zero() { return x == 0 && y == 0; }

  friend std::ostream &operator<<(std::ostream &os, uvec2 const &self) {
    return os << "[" << self.x << ", " << self.y << "]";
  }
};

struct ivec2 {
  std::int32_t x, y;

  friend std::ostream &operator<<(std::ostream &os, ivec2 const &self) {
    return os << "[" << self.x << ", " << self.y << "]";
  }
};
