#include "memory.hh"
#include "../util.hh"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

void MemoryBlock::update() {
  auto file = std::ifstream("/proc/meminfo");
  if (!file.is_open()) {
    std::cerr
        << "WARNING: Could not open /proc/meminfo; Skipping MemoryBlock update!"
        << std::endl;
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

size_t MemoryBlock::draw(Draw &draw, std::chrono::duration<double> delta) {
  size_t x = draw.text(0, draw.vcenter(), "Memory: ");

  std::string text = to_sensible_unit(_used * 1024);
  text += '/';
  text += to_sensible_unit(_total * 1024);

  auto top = 0;
  auto bottom = draw.height() - 1;
  auto left = x;
  auto width = draw.textw(text) + 10;
  auto right = x += width;
  auto height = bottom - top;
  draw.hrect(left, top, width, height);

  auto percent = (double)_used / _total;
  auto fillwidth = (width - 2) * percent;
  Draw::color_type color;
  if (percent > 0.85) {
    color = 0xCC0000;
  } else if (percent > 0.70) {
    color = 0xFFA500;
  } else if (percent > 0.50) {
    color = 0x00CC00;
  } else {
    color = 0x0088CC;
  }

  draw.frect(left + 1, top + 1, fillwidth, height - 1, color);

  draw.text(left + 6, draw.vcenter() + 1, text);

  return x;
}
