#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "memory.hh"
#include "../log.hh"
#include "../util.hh"

MemoryBlock::MemoryBlock(const Config &config) : _config(config) {}

void MemoryBlock::update() {
  auto file = std::ifstream("/proc/meminfo");
  if (!file.is_open()) {
    fmt::print(error,
               "Could not open /proc/meminfo; MemoryBlock update skipped!\n");
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.find("MemTotal:") != std::string::npos) {
      _total = std::stoul(line.substr(line.find_last_of(':') + 1));
    } else if (line.find("MemAvailable:") != std::string::npos) {
      _avail = std::stoul(line.substr(line.find_last_of(':') + 1));
    }
  }
  _used = _total - _avail;
}

size_t MemoryBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  size_t x = draw.text(0, _config.prefix, _config.prefix_color);

  std::string text = fmt::format("{}/{}", to_sensible_unit(_used * 1024),
                                 to_sensible_unit(_total * 1024));

  auto top = 3;
  auto bottom = draw.height() - 5;
  auto left = x;
  auto width = draw.textw(text) + 12;
  auto height = bottom - top;
  x += width;

  auto percent = (double)_used / _total;
  auto fillwidth = (width - 1) * percent;
  color color;
  if (percent > 0.85) {
    color = 0xCC0000;
  } else if (percent > 0.70) {
    color = 0xFFA500;
  } else if (percent > 0.50) {
    color = 0x00CC00;
  } else {
    color = 0x0088CC;
  }

  draw.frect(left, top, fillwidth + 1, height, color);
  draw.hrect(left, top, width, height, 0xFFFFFF);

  draw.text(left + 6, text);

  return x;
}
