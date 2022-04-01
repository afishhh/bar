#include <X11/Xlib.h>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <string>

#include "battery.hh"

BatteryBlock::BatteryBlock(std::filesystem::path path) : _path(path) {}
BatteryBlock::~BatteryBlock() {}

static size_t read_int(std::filesystem::path path) {
  std::ifstream ifs(path);
  if (!ifs.is_open())
    throw std::runtime_error("Could not open file");
  std::string line;
  std::getline(ifs, line);
  return std::stoul(line);
}

std::optional<BatteryBlock> BatteryBlock::find_first() {
  for (auto &entry :
       std::filesystem::directory_iterator("/sys/class/power_supply")) {
    if (entry.path().filename() == "BAT0") {
      return BatteryBlock(entry.path());
    }
  }
  return {};
}

void BatteryBlock::update() {
  _charge_now = read_int(_path / "charge_now");
  _charge_full = read_int(_path / "charge_full");
  _charge_full_design = read_int(_path / "charge_full_design");

  std::ifstream ifs(_path / "status");
  if (!ifs.is_open())
    throw std::runtime_error("Could not open battery status file");
  std::string line;
  std::getline(ifs, line);
  _charging = line == "Charging";
}

size_t BatteryBlock::draw(Draw &draw) {
  double battery_percent = (double)_charge_now / _charge_full * 100;
  size_t x = 0;

  {
    std::ostringstream percent_str;
    percent_str << "Battery: " << std::setw(5) << std::setfill(' ')
                << std::right << std::setprecision(1) << std::fixed
                << battery_percent << '%';
    x += draw.text(x, draw.height() / 2, percent_str.str());
  }

  x += 5;

  auto top = 0;
  auto bottom = draw.height() - 1;
  auto height = bottom - top;
  auto left = x;
  auto right = x += 64;
  draw.line(left, top, right, top);
  draw.line(left, bottom, right, bottom);
  draw.line(left, top, left, bottom);
  draw.line(right, top, right, bottom);

  // FIXME: draw a little square bump on the right of the battery

  size_t fill_width = battery_percent / 100 * (right - left - 1);

  // If charging then fill the box with a gradient
  if (_charging) {
    for (auto i = 0; i < fill_width; ++i) {
      auto color = (155 + (unsigned long)((double)i / fill_width * 100)) << 8;
      auto x = left + 1 + ((i + _charging_gradient_offset / 10) % fill_width);
      draw.line(x, top + 1, x, bottom - 1, color);
    }
    _charging_gradient_offset = (_charging_gradient_offset + 2) % (fill_width * 10);
  } else {
    unsigned long color = 0;
    if (battery_percent > 80) {
      color = 0x00FF00;
    } else if (battery_percent > 60) {
      color = 0xFFFF00;
    } else if (battery_percent > 40) {
      color = 0xFFA500;
    } else if (battery_percent > 20) {
      color = 0xFF0000;
    } else {
      color = 0xFF0000;
    }

    draw.rect(left + 1, top + 1, fill_width, height - 1, color);
  }

  return x;
}
