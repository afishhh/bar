#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <string>

#include "../util.hh"
#include "battery.hh"

BatteryBlock::BatteryBlock(std::filesystem::path path,
                           BatteryBlock::Config config)
    : _path(path), _config(config) {}
BatteryBlock::~BatteryBlock() {}

static size_t read_int(std::filesystem::path path) {
  std::ifstream ifs(path);
  if (!ifs.is_open())
    throw std::runtime_error("Could not open file");
  std::string line;
  std::getline(ifs, line);
  // Happens right after unplugging or plugging in the battery.
  if (line.empty())
    return 0;
  return std::stoul(line);
}

std::optional<BatteryBlock> BatteryBlock::find_first(Config config) {
  for (auto &entry :
       std::filesystem::directory_iterator("/sys/class/power_supply")) {
    if (entry.path().filename() == "BAT0") {
      return BatteryBlock(entry.path(), config);
    }
  }
  return {};
}

void BatteryBlock::update() {
  _charge_now = read_int(_path / "charge_now");
  _charge_full = read_int(_path / "charge_full");
  _charge_full_design = read_int(_path / "charge_full_design");
  _current_now = read_int(_path / "current_now");
  _voltage_now = read_int(_path / "voltage_now");

  std::ifstream ifs(_path / "status");
  if (!ifs.is_open())
    throw std::runtime_error("Could not open battery status file");
  std::string line;
  std::getline(ifs, line);
  _charging = line == "Charging";
  _full = line == "Full";
}

void BatteryBlock::animate(EventLoop::duration delta) {
  if (_charging)
    _charging_gradient_offset =
        (_charging_gradient_offset +
         std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() /
             2500000) %
        size_t(((double)_charge_now / _charge_full * (_config.bar_width - 1) *
                20));
}

size_t BatteryBlock::draw(Draw &draw, std::chrono::duration<double>) {
  double battery_percent = (double)_charge_now / _charge_full * 100;
  size_t x = 0;

  if (!_config.prefix.empty())
    x += draw.text(x, draw.vcenter(), _config.prefix);
  if (_config.show_percentage) {
    std::ostringstream percent_str;
    percent_str << std::setw(5) << std::setfill(' ') << std::right
                << std::setprecision(1) << std::fixed << battery_percent << '%';
    x += draw.text(x, draw.height() / 2, percent_str.str());
  }

  x += 5;

  if (_config.show_wattage) {
    std::ostringstream current_str;
    current_str << std::setw(4) << std::setfill(' ') << std::right
                << std::setprecision(1) << std::fixed
                << (_voltage_now / 1000. / 1000.) *
                       (_current_now / 1000. / 1000.)
                << "W";
    x += draw.text(x, draw.vcenter(), current_str.str());
  }

  x += 5;

  auto top = 0;
  auto bottom = draw.height() - 1;
  auto height = bottom - top;
  auto left = x;
  auto width = _config.bar_width;
  x += width;

  draw.hrect(left, top, width, height);

  size_t fill_width = battery_percent / 100 * (width - 1);

  auto format_time = [this](size_t seconds) {
    std::string time_str;
    unsigned short blocks = 0;
    if (seconds >= 24 * 60 * 60 && blocks < _config.time_precision) {
      time_str += std::to_string(seconds / (24 * 60 * 60));
      time_str += "d ";
      seconds %= 24 * 60 * 60;
      ++blocks;
    }
    if (seconds >= 60 * 60 && blocks < _config.time_precision) {
      time_str += std::to_string(seconds / (60 * 60));
      time_str += "h ";
      seconds %= 60 * 60;
      ++blocks;
    }
    if (seconds >= 60 && blocks < _config.time_precision) {
      time_str += std::to_string(seconds / 60);
      time_str += "m ";
      seconds %= 60;
      ++blocks;
    }
    if (blocks < _config.time_precision) {
      time_str += std::to_string(seconds);
      time_str += "s ";
    }

    if (time_str.back() == ' ')
      time_str.pop_back();
    return time_str;
  };

  // If charging then fill the box with a gradient
  if (_charging) {
    for (size_t i = 0; i < fill_width; ++i) {
      auto color = (155 + (unsigned long)((double)i / fill_width * 100)) << 8;
      auto x = left + 1 + ((i + _charging_gradient_offset / 10) % fill_width);
      draw.line(x, top + 1, x, bottom - 1, color);
    }

    if (_config.show_time_left_charging && !_full) {
      auto time_left_str = format_time((double)(_charge_full - _charge_now) /
                                       _current_now * 3600);
      draw.text(left + width / 2 - draw.textw(time_left_str) / 2,
                1 + draw.vcenter(), time_left_str);
    }
  } else {
    // 0-100 in HSL is the Red-Green range so we can act as if battery_percent
    // is in the HSL hue range and then just map it to a percentage
    auto hue = map_range(battery_percent, 0, 360, 0, 1);
    unsigned long color = rgb_to_long(hsl_to_rgb(hue, .9, .45));

    draw.frect(left + 1, top + 1, fill_width, height - 1, color);

    if (_config.show_time_left_discharging && !_full) {
      auto time_left_str =
          format_time((double)_charge_now / _current_now * 3600);
      draw.text(left + width / 2 - draw.textw(time_left_str) / 2,
                1 + draw.vcenter(), time_left_str);
    }
  }

  if (_config.show_degradation) {
    x += 5;
    std::ostringstream ss;
    ss << std::setw(5) << std::setfill(' ') << std::right
       << std::setprecision(1) << std::fixed
       << ((double)_charge_full / _charge_full_design * 100.) << '%';
    x += draw.text(x, draw.vcenter(), ss.str());
  }

  return x;
}
