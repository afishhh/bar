#include "memory.hh"
#include "../format.hh"
#include "../log.hh"
#include "../util.hh"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

MemoryBlock::MemoryBlock(const Config &config) : _config(config) {}

void MemoryBlock::update() {
  auto file = std::ifstream("/proc/meminfo");
  if (!file.is_open()) {
    std::print(error,
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

size_t MemoryBlock::draw(Draw &draw, std::chrono::duration<double>) {
  size_t x = draw.text(0, draw.vcenter(), _config.prefix, _config.prefix_color);

  std::string text = std::format("{}/{}", to_sensible_unit(_used * 1024),
                                 to_sensible_unit(_total * 1024));

  auto top = 0;
  auto bottom = draw.height() - 1;
  auto left = x;
  auto width = draw.textw(text) + 10;
  auto height = bottom - top;
  x += width;
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
