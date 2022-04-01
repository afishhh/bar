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
    } else if (line.find("MemFree:") != std::string::npos) {
      _free = std::stoul(line.substr(line.find_last_of(':') + 1));
    }
  }
  _used = _total - _free;
}

size_t MemoryBlock::draw(Draw &draw) {
  size_t x = draw.text(0, draw.vcenter(), "Memory: ");
  x += draw.text(x, draw.vcenter(), to_sensible_unit(_used * 1024));
  x += draw.text(x, draw.vcenter(), "/");
  x += draw.text(x, draw.vcenter(), to_sensible_unit(_total * 1024));
  return x;
}
